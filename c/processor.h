 /***************************************************************/
/***             Processor-specific definitions                ***/
 /***************************************************************/

#ifndef _PROCESSOR_H
#define _PROCESSOR_H


#include "vcompiler.h"

EXTERNFN void psyco_processor_init();


/***************************************************************/
 /*** Utilities                                               ***/

/* executes a block of code. A new stack frame is created and
   the 'initial_stack' values are PUSHed into it. The number
   of values pushed are determined by the saved arguments_count of
   the 'codebuf'.
*/
EXTERNFN PyObject* psyco_processor_run(CodeBufferObject* codebuf,
                                       long initial_stack[]);

/* return a new vinfo_t* meaning `in the processor flags, true if <cc>',
   as an integer 0 or 1. The source of the vinfo_t* is compile-time
   if cc is CC_ALWAYS_TRUE/FALSE, and virtual-time otherwise. (In the latter
   case it gets stored in po->ccreg.) */
EXTERNFN vinfo_t* psyco_vinfo_condition(PsycoObject* po, condition_code_t cc);

/* if 'source' comes from psyco_vinfo_condition(), return its <cc>;
   otherwise return CC_ALWAYS_FALSE. */
EXTERNFN condition_code_t psyco_vsource_cc(Source source);

/* returns the next register that should be used */
inline reg_t next_free_reg(PsycoObject* po) {
	return (po->last_used_reg = RegistersLoop[(int)(po->last_used_reg)]);
}

/* call a C function with a variable number of arguments
   (implemented as a pointer to assembler code) */
EXTERNVAR long (*psyco_call_var) (void* c_func, int argcount, long arguments[]);


/*****************************************************************/
 /***   read and write fields of structures in memory           ***/


/* only called by the inlined functions defined below; do not use directly. */
EXTERNFN vinfo_t* psyco_get_array_item(PsycoObject* po, vinfo_t* vi, int index);
EXTERNFN vinfo_t* psyco_read_array_item(PsycoObject* po, vinfo_t* vi, int index);
EXTERNFN vinfo_t* psyco_read_array_item_var(PsycoObject* po, vinfo_t* v0,
                                            vinfo_t* v1, int ofsbase, int shift);
EXTERNFN bool psyco_write_array_item(PsycoObject* po, vinfo_t* src,
                                     vinfo_t* v, int index);
EXTERNFN bool psyco_write_array_item_var(PsycoObject* po, vinfo_t* src,
                                         vinfo_t* v0, vinfo_t* v1, int ofsbase);


/* Use read/write_array_item to emit the code that performs the read or
   write to a structure. Use get/set_array_item if the vinfo_array_t*
   can be considered as a cache over the in-memory structure. In other
   words, get_array_item will not reload a value if it has already been
   loaded once, and set_array_item never actually writes any value at
   all (it stays in the vinfo_array_t* cache).
*/

inline vinfo_t* get_array_item(PsycoObject* po, vinfo_t* vi, int index) {
	/* does not return a new reference */
	vinfo_t* result = vinfo_getitem(vi, index);
	if (result == NULL)
		result = psyco_get_array_item(po, vi, index);
	return result;
}

inline vinfo_t* read_array_item(PsycoObject* po, vinfo_t* vi, int index) {
	/* returns a new reference */
	vinfo_t* result;
	if (is_virtualtime(vi->source) &&
	    (result = vinfo_getitem(vi, index)) != NULL)
		vinfo_incref(result);   /* done, bypass compute() */
	else
		result = psyco_read_array_item(po, vi, index);
	return result;
}

inline vinfo_t* read_array_item_var(PsycoObject* po, vinfo_t* vi, int baseindex,
				    vinfo_t* varindex, bool byte) {
	/* returns a new reference */
	return psyco_read_array_item_var(po, vi, varindex,
					 baseindex*sizeof(long), byte ? 0 : 2);
}

#if 0
--- Disabled: not safe, could make the PsycoObject grow unboundedly
inline vinfo_t* get_array_item_var(PsycoObject* po, vinfo_t* vi, int baseindex,
                                   vinfo_t* varindex, bool byte) {
	/* returns a new reference */
	if (is_compiletime(varindex->source)) {
		vinfo_t* r = get_array_item(po, vi,
		    CompileTime_Get(varindex->source)->value + baseindex);
		if (r != NULL)
			vinfo_incref(r);
		return r;
	}
	else
		return read_array_item_var(po, vi, baseindex, varindex, byte);
}
#endif

inline vinfo_t* read_immut_array_item_var(PsycoObject* po, vinfo_t* vi,
                                          int baseindex, vinfo_t* varindex,
                                          bool byte) {
	/* returns a new reference */
	if (is_compiletime(varindex->source)) {
		vinfo_t* r = vinfo_getitem(vi,
		    CompileTime_Get(varindex->source)->value + baseindex);
		if (r != NULL) {
			vinfo_incref(r);
			return r;
		}
	}
	return read_array_item_var(po, vi, baseindex, varindex, byte);
}

inline void set_array_item(PsycoObject* po, vinfo_t* vi,
			   int index, vinfo_t* newitem) {
	/* CONSUMES a reference on 'newitem' */
	vinfo_t* item = vinfo_needitem(vi, index);
	vinfo_xdecref(item, po);
	vi->array->items[index] = newitem;
}

inline bool write_array_item(PsycoObject* po, vinfo_t* vi,
			     int index, vinfo_t* newitem) {
	/* does not consume any reference */
	return psyco_write_array_item(po, newitem, vi, index);
}

inline bool write_array_item_var(PsycoObject* po, vinfo_t* vi, int baseindex,
                                 vinfo_t* varindex, vinfo_t* newitem) {
	/* does not consume any reference */
	return psyco_write_array_item_var(po, newitem, vi, varindex,
                                          baseindex*sizeof(long));
}


#if 0
/* XXX to do: references from the code buffers. This is tricky because
   we can have quite indirect references, or references to a subobject of
   a Python object, or to a field only of a Python object, etc... */
inline long reference_from_code(PsycoObject* po, CompileTimeSource source)
{
	--- DISABLED ---
	source_known_t* sk = CompileTime_Get(source);
	if ((sk->refcount1_flags & SkFlagPyObj) != 0) {
		/* XXX we get a reference from the code.
		   Implement freeing such references
		   together with code buffers. */
		sk_incref(sk);
	}
	return sk->value;
}
#endif


/*****************************************************************/
 /***   Calling C functions                                     ***/

/* A generic way of emitting the call to a C function.
   For maximal performance you can also directly use the macros
   CALL_C_FUNCTION()&co. in encoding.h.

   'arguments' is a string describing the following arguments
   of psyco_generic_call(). Each argument to the C function to call
   comes from a 'vinfo_t*' structure, excepted when it is known to be
   compile-time, in which case a 'long' or 'PyObject*' can be passed
   instead. In 'arguments' the characters are interpreted as follows:

      l      an immediate 'long' or 'PyObject*' value
      v      a 'vinfo_t*' value
      r      a run-time 'vinfo_t*' value passed as a reference
      a      a 'vinfo_array_t*' used by the C function as output buffer
      A      same as 'a', but the C function creates new references

   'r' means that the C function wants to get a reference to a
   single-word value (typically it is an output argument). The
   run-time value is pushed in the stack if it is just in a
   register. Incompatible with CfPure.

   'a' means that the C function gets a pointer to a buffer capable of
   containing as many words as specified by the array count.
   psyco_generic_call() fills the array with run-time vinfo_ts
   representing the output values. Incompatible with CfPure.
*/
EXTERNFN vinfo_t* psyco_generic_call(PsycoObject* po, void* c_function,
                                     int flags, const char* arguments, ...);

/* if the C function has no side effect it can be called at compile-time
   if all its arguments are compile-time. Use CfPure in this case. */
#define CfPure           0x10

/* if the C function returns a long or a PyObject* but not a new reference */
#define CfReturnNormal   0x00   /* default */

/* if the C function returns a new reference */
#define CfReturnRef      0x01

/* if the C function returns a flag (true/false). In this case,
   typecast the return value of psyco_generic_call() to condition_code_t.
   psyco_flag_call() does it for you. */
#define CfReturnFlag     0x02
#define psyco_flag_call  (condition_code_t) psyco_generic_call

/* if the C function does not return anything (incompatible with CfPure)
   or if you are not interested in getting the result in a vinfo_t.
   psyco_generic_call() returns anything non-NULL (unless there is an error)
   in this case. */
#define CfNoReturnValue  0x03

#define CfReturnMask     0x0F
/* See also the Python-specific flags CfPyErrXxx defined in pycheader.h. */


/* a faster variant for the commonly-used(?) form of psyco_generic_call()
   with no argument and CfNoReturnValue */
inline void psyco_call_void(PsycoObject* po, void* c_function) {
	BEGIN_CODE
	SAVE_REGS_FN_CALLS;
	CALL_C_FUNCTION(c_function,  0);
	END_CODE
}


/* To emit the call to other code blocks emitted by Psyco. 'argsources' gives
   the run-time sources for the arguments, as specified by
   psyco_build_frame(). */
EXTERNFN vinfo_t* psyco_call_psyco(PsycoObject* po, CodeBufferObject* codebuf,
                                   Source argsources[]);

/*****************************************************************/
 /***   Emit common instructions                                ***/

EXTERNFN condition_code_t integer_non_null(PsycoObject* po, vinfo_t* vi);

/* Instructions with an 'ovf' parameter will check for overflow
   if 'ovf' is true. They return NULL if an overflow is detected. */
EXTERNFN
vinfo_t* integer_add  (PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf);
EXTERNFN
vinfo_t* integer_add_i(PsycoObject* po, vinfo_t* v1, long value2);
EXTERNFN
vinfo_t* integer_sub  (PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf);
EXTERNFN
vinfo_t* integer_sub_i(PsycoObject* po, vinfo_t* v1, long value2);
EXTERNFN
vinfo_t* integer_or   (PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* integer_and  (PsycoObject* po, vinfo_t* v1, vinfo_t* v2);
EXTERNFN
vinfo_t* integer_not  (PsycoObject* po, vinfo_t* v1);
EXTERNFN
vinfo_t* integer_neg  (PsycoObject* po, vinfo_t* v1, bool ovf);
EXTERNFN
vinfo_t* integer_abs  (PsycoObject* po, vinfo_t* v1, bool ovf);
/* Comparison: 'py_op' is one of Python's rich comparison numbers
   Py_LT, Py_LE, Py_EQ, Py_NE, Py_GT, Py_GE
   optionally together with COMPARE_UNSIGNED */
EXTERNFN condition_code_t integer_cmp  (PsycoObject* po, vinfo_t* v1,
					vinfo_t* v2, int py_op);
EXTERNFN condition_code_t integer_cmp_i(PsycoObject* po, vinfo_t* v1,
					long value2, int py_op);
#define COMPARE_UNSIGNED  8

/* For sequence indices: 'vn' is the length of the sequence, 'vi' an
   index. Check that vi is in range(0,vn). Increase 'vi' by 'vn' if
   needed to put it in the correct range. */
#if 0
                      (not used)
EXTERNFN vinfo_t* integer_seqindex(PsycoObject* po, vinfo_t* vi,
				   vinfo_t* vn, bool ovf);
#endif

/* make a run-time copy of a vinfo_t */
EXTERNFN vinfo_t* make_runtime_copy(PsycoObject* po, vinfo_t* v);


/*****************************************************************/
 /***   code termination                                        ***/

/* write a return, clearing the stack as necessary, and releases 'po'. */
EXTERNFN code_t* psyco_finish_return(PsycoObject* po, NonVirtualSource retval);

/* write codes that calls the C function 'fn' and jumps to its
   return value. Save registers before calling psyco_finish_call_proxy().
   Set 'restore' to 1 if you used TEMP_SAVE_REGS_FN_CALLS,
   or 0 if you used SAVE_REGS_FN_CALLS.
   The arguments passed to 'fn' will be a pointer to a constant structure
   at the end of the code, plus any others previously specified by calls
   to CALL_SET_ARG_xxx(). Set 'nb_args' to one plus your own arguments.
   The constant structure is at the end of the code, and
   psyco_finish_call_proxy() returns a pointer to it. */
EXTERNFN
void* psyco_jump_proxy(PsycoObject* po, void* fn, int restore, int nb_args);

#if 0   /* disabled */
/* emergency code for out-of-memory conditions in which do not
   have a code buffer available for psyco_finish_err_xxx().
   A temporary buffer of the size EMERGENCY_PROXY_SIZE is enough. */
#define EMERGENCY_PROXY_SIZE    11
code_t* psyco_emergency_jump(PsycoObject* po, code_t* code);
#endif


/*****************************************************************/
 /***   run-time switches                                       ***/

typedef struct c_promotion_s {
  source_virtual_t header;
  struct fixed_switch_s* fs;
  long kflags;
} c_promotion_t;

typedef struct fixed_switch_s {  /* private structure */
  int switchcodesize;   /* size of 'switchcode' */
  code_t* switchcode;
  int count;
  struct fxcase_s* fxc;
  long* fixtargets;
  long zero;            /* for 'array' pointers from vinfo_t structures */
  struct c_promotion_s fixed_promotion; /* pseudo-exception meaning 'fix me' */
} fixed_switch_t;

/* initialization of a fixed_switch_t structure */
EXTERNFN int psyco_build_run_time_switch(fixed_switch_t* rts, long kflags,
                                         long values[], int count);

/* Look for a (known) value in a prepared switch.
   Return -1 if not found. */
EXTERNFN int psyco_switch_lookup(fixed_switch_t* rts, long value);

/* Write the code that does a 'switch' on the prepared 'values'.
   The register 'reg' contains the value to switch on. All jump targets
   are initially at the end of the written code; see
   psyco_fix_switch_target(). */
EXTERNFN code_t* psyco_write_run_time_switch(fixed_switch_t* rts,
                                             code_t* code, char reg);

/* Fix the target corresponding to the given case ('item' is a value
   returned by psyco_switch_lookup()). 'code' is the *end* of the
   switch code, as returned by psyco_write_run_time_switch(). */
EXTERNFN void psyco_fix_switch_case(fixed_switch_t* rts, code_t* code,
                                    int item, code_t* newtarget);

/* The pseudo-exceptions meaning 'promote me' but against no particular
   fixed_switch_t. The second one has the SkFlagPyObj flag. */
EXTERNVAR c_promotion_t psyco_nonfixed_promotion;
EXTERNVAR c_promotion_t psyco_nonfixed_pyobj_promotion;

/* Check if the given virtual source is a promotion exception */
EXTERNFN bool psyco_vsource_is_promotion(VirtualTimeSource source);


/* is the given run-time vinfo_t known to be none of the values
   listed in rts? */
inline bool known_to_be_default(vinfo_t* vi, fixed_switch_t* rts) {
	return vi->array == NullArrayAt(rts->zero);
}


#endif /* _PROCESSOR_H */
