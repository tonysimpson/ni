 /***************************************************************/
/***          Structures used by the dispatcher part           ***/
 /***************************************************************/

#ifndef _DISPATCHER_H
#define _DISPATCHER_H


#include "psyco.h"
#include "vcompiler.h"
#include "processor.h"


inline void psyco_dispatcher_init(void) { }


/* a frozen PsycoObject is a snapshot of an actual PsycoObject,
   capturing the state of the compiler in a form that can be used
   later to compare live states to it. Currently implemented as a
   copy of the PsycoObject vlocals and stack_depth.
   XXX should be optimized very differently
   the FrozenPsycoObject's secondary goal is to capture enough
   information to rebuild a "live" PsycoObject, close enough to
   the original to let the next few Python instructions produce
   exactly the same machine code as the original. See
   psyco_prepare_respawn(). Be careful, there are a lot of such
   snapshots around in memory; keep them as small as possible. */
struct FrozenPsycoObject_s {
  union {
    int as_int;   /* last_used_reg in bits 0-7 and stack_depth in the rest */
    struct respawn_s* respawning;
  } fz_stuff;
  vinfo_array_t* fz_vlocals;
  short fz_arguments_count;
  short fz_respawned_cnt;
  CodeBufferObject* fz_respawned_from;
  pyc_data_t* fz_pyc_data;  /* only partially allocated */
};

/* construction */
inline void fpo_mark_new(FrozenPsycoObject* fpo) {
	fpo->fz_respawned_from = NULL;
}
inline void fpo_mark_unused(FrozenPsycoObject* fpo) {
	fpo->fz_vlocals = NullArray;
	fpo->fz_pyc_data = NULL;
}
EXTERNFN void fpo_build(FrozenPsycoObject* fpo, PsycoObject* po);
EXTERNFN void fpo_release(FrozenPsycoObject* fpo);

/* build a 'live' PsycoObject from frozen snapshot */
EXTERNFN PsycoObject* fpo_unfreeze(FrozenPsycoObject* fpo);

/* inspection */
inline int get_stack_depth(FrozenPsycoObject* fpo) {
	return fpo->fz_stuff.as_int >> 8;
}


/* psyco_compatible():
   search in the given global_entries_t for a match to the living PsycoObject.
   Return the best match in *matching. The result is either COMPATIBLE,
   INCOMPATIBLE (no match), or an actual vinfo_t* of 'po' which is a compile-
   time value that should be un-promoted to run-time.
   
   If the result is COMPATIBLE, this call leaves the dispatcher in a
   unstable state; it must be fixed by calling one of the psyco_unify()
   functions below, or by psyco_stabilize().
*/
#define COMPATIBLE    NULL
#define INCOMPATIBLE  ((vinfo_t*) 1)

EXTERNFN vinfo_t* psyco_compatible(PsycoObject* po, global_entries_t* pattern,
				   CodeBufferObject** matching);

EXTERNFN void psyco_stabilize(CodeBufferObject* lastmatch);


/*****************************************************************/
 /***   "Global Entries"                                        ***/

/* global entry points for the compiler. One global entry point holds a list
   of already-compiled code buffers corresponding to the various states
   in which the compiler has already be seen at this point. See
   psyco_compatible().

   The dispatcher saves all CodeBufferObjects (with their copy of the
   compiler state) in a list for each 'entry point' of the compiler
   (the main entry point is spec_main_loop, the start of the main loop).
   When coming back to this entry point later, the list let us determine
   whether we already encountered the same state.
   
   The details of this structure are private.
   XXX implemented as a list object holding CodeBufferObjects in no
   XXX particular order. Must be optimized for reasonably fast searches
   XXX if the lists become large (more than just a few items).
*/
struct global_entries_s {
	PyObject* fatlist;      /* list of CodeBufferObjects */
};

/* initialize a global_entries_t structure */
inline void psyco_ge_init(global_entries_t* ge) {
	ge->fatlist = PyList_New(0);
	if (ge->fatlist == NULL)
		OUT_OF_MEMORY();
}

/* register the code buffer; it will be found by future calls to
   psyco_compatible(). */
inline int register_codebuf(global_entries_t* ge, CodeBufferObject* codebuf) {
	return PyList_Append(ge->fatlist, (PyObject*) codebuf);
}


/*****************************************************************/
 /***   Unification                                             ***/

/* Update 'po' to match 'target', then jump to 'target'.
   For the conversion we might have to emit some code.
   If this->code == NULL or there is not enough room between code and
   this->codelimit, a new code buffer is created and returned in 'target';
   otherwise, Py_INCREF(*target). If this->code != NULL, the return
   value points at the end of the code that has been written there;
   otherwise, the return value is undefined (but not NULL).
   
   Note: this function only works right after a successful call
   to compare_array. It needs the data left in the 'tmp' fields.
   It releases 'po'.
*/
EXTERNFN code_t* psyco_unify(PsycoObject* po, CodeBufferObject** target);

/* Simplified interface to psyco_unify() without using a previously
   existing code buffer (i.e. 'po->code' is uninitialized). If needed,
   return a new buffer with the necessary code followed by a JMP to
   'target'. If no code is needed, just return a new reference to 'target'.
*/
EXTERNFN CodeBufferObject* psyco_unify_code(PsycoObject* po,
                                            CodeBufferObject* target);

/* To "simplify" recursively a vinfo_array_t. The simplification done
   is to replace run-time values inside a sub-array of a non-virtual
   value with NULL. We assume that these can still be reloaded later if
   necessary. Returns the number of run-time values left.
   This assumes that all 'tmp' marks are cleared in 'array'. */
EXTERNFN int psyco_simplify_array(vinfo_array_t* array);

/*****************************************************************/
 /***   Promotion                                               ***/

/* Promotion of a run-time variable into a fixed compile-time one.
   Finish the code block with a jump to the dispatcher that
   promotes the run-time variable 'fix' to compile-time. This
   usually means the compiler will be called back again, at the
   given entry point.
   Note: Releases 'po'.
*/
EXTERNFN code_t* psyco_finish_promotion(PsycoObject* po, vinfo_t* fix,
                                        long kflags);

/* Promotion of certain run-time values into compile-time ones
   (promotion only occurs for values inside a given set, e.g. for
   types that we know how to optimize). The special values are
   described in an array of long, turned into a source_known_t
   (see processor.h).
   Note: Releases 'po'.
*/
EXTERNFN code_t* psyco_finish_fixed_switch(PsycoObject* po, vinfo_t* fix,
                                           long kflags,
                                           fixed_switch_t* special_values);

/* Un-Promotion from non-fixed compile-time into run-time.
   Note: this does not release 'po'. Un-promoting is easy and
   don't require encoding calls to the dispatcher.
*/
EXTERNFN void psyco_unfix(PsycoObject* po, vinfo_t* vi);


/*****************************************************************/
 /***   Respawning                                              ***/

/* internal use */
EXTERNFN void psyco_prepare_respawn(PsycoObject* po,
                                    condition_code_t jmpcondition);
EXTERNFN void psyco_respawn_detected(PsycoObject* po);
inline bool detect_respawn(PsycoObject* po) {
	if (!++po->respawn_cnt) {
		psyco_respawn_detected(po);
		return true;
	}
	else
		return false;
}
inline bool is_respawning(PsycoObject* po) { return po->respawn_cnt < 0; }
	
/* the following powerful function stands for 'if the processor flag (cond) is
   set at run-time, then...'. Of course we do not know yet if this will be
   the case or not, but the macro takes care of preparing the required
   respawns if needed. 'cond' may be CC_ALWAYS_xxx or a real processor flag.
   runtime_condition_f() assumes the outcome is generally false,
   runtime_condition_t() assumes the outcome is generally true. */
inline bool runtime_condition_f(PsycoObject* po, condition_code_t cond) {
	if (cond == CC_ALWAYS_FALSE) return false;
	if (cond == CC_ALWAYS_TRUE) return true;
	if (detect_respawn(po)) return true;
	psyco_prepare_respawn(po, cond);
	return false;
}
inline bool runtime_condition_t(PsycoObject* po, condition_code_t cond) {
	if (cond == CC_ALWAYS_TRUE) return true;
	if (cond == CC_ALWAYS_FALSE) return false;
	if (detect_respawn(po)) return false;
	psyco_prepare_respawn(po, INVERT_CC(cond));
	return true;
}


#endif /* _DISPATCHER_H */
