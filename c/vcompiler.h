 /***************************************************************/
/***           Structures used by the compiler part            ***/
 /***************************************************************/

#ifndef _VCOMPILER_H
#define _VCOMPILER_H

#include "psyco.h"
#include "encoding.h"
#include "Python/pycheader.h"


EXTERNFN void psyco_compiler_init(void);


/*****************************************************************/
 /***   Definition of the "sources" of vinfo_t structures       ***/


/* A "source" defines the stage of the variable (run-time,
   compile-time or virtual-time), and gives information about
   the value of the variable */
typedef long Source;   /* Implemented as a bitfield 32-bit integer. */

/* the next typedefs are for documentation purposes only, as the C compiler
   will not make any difference between them all */
typedef long NonVirtualSource;
typedef long RunTimeSource;
typedef long CompileTimeSource;
typedef long VirtualTimeSource;

#define RunTime         0
#define CompileTime     1    /* a.k.a. "Known value" */
#define VirtualTime     2
#define TimeMask        (CompileTime | VirtualTime)

inline bool is_runtime(Source s)     { return (s & TimeMask) == RunTime; }
inline bool is_compiletime(Source s) { return (s & CompileTime) != 0; }
inline bool is_virtualtime(Source s) { return (s & VirtualTime) != 0; }
inline long gettime(Source s)        { return s & TimeMask; }
#define CHKTIME(src, time)           extra_assert(gettime(src) == (time))


/************************ Run-time sources *******************************
 *
 * If the last two bits of 'source' are 'RunTime', we have a run-time value.
 * The rest of 'source' encodes both the position of the value in the stack
 * (or StackNone) and the register holding this value (or REG_NONE).
 *
 **/

/* flags */
#define RunTime_StackMask    0x07FFFFFC
#define RunTime_StackMax     RunTime_StackMask
#define RunTime_StackNone    0
#define RunTime_RegMask      0xF0000000
#define RunTime_NoRef        0x08000000
#define RunTime_FlagsMask    RunTime_NoRef

/* construction */
inline RunTimeSource RunTime_NewStack(int stack_position, reg_t reg, bool ref) {
	long result = RunTime + stack_position + ((long) reg << 28);
	if (!ref)
		result += RunTime_NoRef;
	return (RunTimeSource) result;
}
inline RunTimeSource RunTime_New(reg_t reg, bool ref) {
	return RunTime_NewStack(RunTime_StackNone, reg, ref);
}

/* field inspection */
inline bool has_rtref(Source s) {
	return (s & (TimeMask|RunTime_NoRef)) == RunTime;
}
inline reg_t getreg(RunTimeSource s)     { return (reg_t)(s >> 28); }
inline bool is_reg_none(RunTimeSource s) { return s < 0; }
inline int getstack(RunTimeSource s)     { return s & RunTime_StackMask; }

/* mutation */
inline RunTimeSource remove_rtref(RunTimeSource s) { return s | RunTime_NoRef; }
inline RunTimeSource add_rtref(RunTimeSource s)    { return s & ~RunTime_NoRef; }
inline RunTimeSource set_rtreg_to(RunTimeSource s, reg_t newreg) {
	return (s & ~RunTime_RegMask) | ((long) newreg << 28);
}
inline RunTimeSource set_rtreg_to_none(RunTimeSource s) {
	return s | ((long) REG_NONE << 28);
}
inline RunTimeSource set_rtstack_to(RunTimeSource s, int stack) {
	extra_assert(getstack(s) == RunTime_StackNone);
	return s | stack;
}
inline RunTimeSource set_rtstack_to_none(RunTimeSource s) {
	return s & ~RunTime_StackMask;
}


/************************ Compile-time sources *******************************
 *
 * if the last two bits of 'source' are 'CompileTime',
 * the rest of 'source' points to a 'source_known_t' structure:
 *
 **/
typedef struct {
	long refcount1_flags; /* flags and reference counter */
	long value;           /* compile-time known value */
} source_known_t;

/* flags for source_known_t::refcount1_flags: */

/* flag added when producing code that relies
   essentially on this value to be constant */
#define SkFlagFixed   0x01

/* value is a PyObject* and holds a reference */
#define SkFlagPyObj   0x02

/* first unused flag */
#define SkFlagEnd     0x04
#define SkFlagMask    (SkFlagEnd-1)

/* refcounting */
EXTERNFN void sk_release(source_known_t *sk);
inline void sk_incref(source_known_t *sk) { sk->refcount1_flags += SkFlagEnd; }
inline void sk_decref(source_known_t *sk) {
	if ((sk->refcount1_flags -= SkFlagEnd)<0) sk_release(sk);
}

/* construction */
EXTERNVAR void** sk_linked_list;
EXTERNFN void* sk_malloc_block(void);
inline source_known_t* sk_new(long v, long flags) {
	source_known_t* sk;
	if (sk_linked_list == NULL)
		sk = (source_known_t*) sk_malloc_block();
	else {
		sk = (source_known_t*) sk_linked_list;
		sk_linked_list = *(void**) sk;
	}
	sk->refcount1_flags = flags;
	sk->value = v;
	return sk;
}
inline void sk_delete(source_known_t* sk) {
	*(void**) sk = sk_linked_list;
	sk_linked_list = (void**) sk;
}


/* Compile-time sources */
/* construction */
inline CompileTimeSource CompileTime_NewSk(source_known_t* newsource) {
	extra_assert((((long) newsource) & TimeMask) == 0);
	return (CompileTimeSource) (((char*) newsource) + CompileTime);
}
inline CompileTimeSource CompileTime_New(long value) {
	return CompileTime_NewSk(sk_new(value, 0));
}

/* inspection */
inline source_known_t* CompileTime_Get(CompileTimeSource s) {
	return (source_known_t*)(((char*) s) - CompileTime);
}
inline CompileTimeSource set_ct_value(CompileTimeSource s, long v) {
	source_known_t* sk = CompileTime_Get(s);
        extra_assert((sk->refcount1_flags & SkFlagPyObj) == 0);
	if (sk->refcount1_flags < SkFlagEnd) {
		sk->value = v; /* reuse object when only one reference */
		return s;
	}
	else {
		sk_decref(sk);
		return CompileTime_NewSk(sk_new(v, sk->refcount1_flags &
						SkFlagMask));
	}
}


/************************ Virtual-time sources *******************************
 *
 * if the last two bits of 'source' are VIRTUAL_TIME,
 * the rest of 'source' points to a 'source_virtual_t' structure.
 * See psyco.h for the definition of source_virtual_t.
 *
 **/

/* construction */
inline VirtualTimeSource VirtualTime_New(source_virtual_t* sv) {
	extra_assert((((long) sv) & TimeMask) == 0);
	return (VirtualTimeSource) (((char*) sv) + VirtualTime);
}

/* inspection */
inline source_virtual_t* VirtualTime_Get(VirtualTimeSource s) {
	return (source_virtual_t*)(((char*) s) - VirtualTime);
}


EXTERNVAR source_virtual_t psyco_vsource_not_important;

#define SOURCE_NOT_IMPORTANT     VirtualTime_New(&psyco_vsource_not_important)
#define SOURCE_DUMMY             RunTime_New(REG_NONE, false)
#define SOURCE_DUMMY_WITH_REF    RunTime_New(REG_NONE, true)
#define SOURCE_ERROR             ((Source) -1)



 /***************************************************************/
/***      Definition of the fundamental vinfo_t structure      ***/
 /***************************************************************/


/* 'array' fields are never NULL, but point to a fraction of vinfo_array_t
 * in which 'count' is 0, like 'NullArray'.
 * This allows 'array->count' to always return a sensible value.
 * Invariant: the array is dynamically allocated if and only if 'array->count'
 * is greater than 0.
 */
EXTERNVAR const long psyco_zero;
#define NullArrayAt(zero_variable)  ((vinfo_array_t*)(&(zero_variable)))
#define NullArray                   NullArrayAt(psyco_zero)


struct vinfo_array_s {
	int count;
	vinfo_t* items[NB_LOCALS]; /* variable-sized when not in a PsycoObject */
};

/* construction */
EXTERNFN vinfo_array_t* array_grow1(vinfo_array_t* array, int ncount);
inline void array_release(vinfo_array_t* array) {
	if (array->count > 0) PyCore_FREE(array);
}
inline vinfo_array_t* array_new(int ncount) {
	if (ncount > 0)
		return array_grow1(NullArray, ncount);
	else
		return NullArray;
}


/* 'vinfo_t' defines for the compiler the stage of a
   variable and where it is found. It is a wrapper around a 'Source'.
   For pointers to structures, 'array' is used to decompose the structure
   fields into 'vinfo_t'-described variables which can in turn
   be at various stages. */
struct vinfo_s {
	int refcount;           /* reference counter */
	Source source;
	vinfo_array_t* array;   /* substructure variables or a NullArray */
	vinfo_t* tmp;           /* internal use of the dispatcher */
};

/* allocation */
EXTERNVAR void** vinfo_linked_list;
EXTERNVAR void* vinfo_malloc_block(void);
/* private! Do not use */
#ifdef PSYCO_NO_LINKED_LISTS
# define VINFO_FREE_1(vi)   PyCore_FREE(vi)
#else
# define VINFO_FREE_1(vi)   (*(void**) vi = vinfo_linked_list,  \
                             vinfo_linked_list = (void**) vi)
#endif


/* construction */
inline vinfo_t* vinfo_new(Source src) {
	vinfo_t* vi;
	if (vinfo_linked_list == NULL)
		vi = (vinfo_t*) vinfo_malloc_block();
	else {
		vi = (vinfo_t*) vinfo_linked_list;
		vinfo_linked_list = *(void**) vi;
	}
	vi->refcount = 1;
	vi->source = src;
	vi->array = NullArray;
	return vi;
}

/* copy constructor */
EXTERNFN vinfo_t* vinfo_copy(vinfo_t* vi);

/* refcounting */
EXTERNFN void vinfo_release(vinfo_t* vi, PsycoObject* po);
inline void vinfo_incref(vinfo_t* vi) { ++vi->refcount; }
inline void vinfo_decref(vinfo_t* vi, PsycoObject* po) {
	if (!--vi->refcount) vinfo_release(vi, po);
}
inline void vinfo_xdecref(vinfo_t* vi, PsycoObject* po) {
	if (vi != NULL) vinfo_decref(vi, po);
}

/* promoting out of virtual-time */
inline NonVirtualSource vinfo_compute(vinfo_t* vi, PsycoObject* po) {
	if (is_virtualtime(vi->source)) {
		if (!VirtualTime_Get(vi->source)->compute_fn(po, vi))
			return SOURCE_ERROR;
		extra_assert(!is_virtualtime(vi->source));
	}
	return (NonVirtualSource) vi->source;
}

/* sub-array (see also processor.h, get_array_item()&co.) */
inline void vinfo_array_grow(vinfo_t* vi, int ncount) {
	if (ncount > vi->array->count)
		vi->array = array_grow1(vi->array, ncount);
}
inline vinfo_t* vinfo_getitem(vinfo_t* vi, int index) {
	if (index < vi->array->count)
		return vi->array->items[index];
	else
		return NULL;
}
inline vinfo_t* vinfo_needitem(vinfo_t* vi, int index) {
	vinfo_array_grow(vi, index+1);
	return vi->array->items[index];
}
inline void vinfo_setitem(PsycoObject* po, vinfo_t* vi, int index,
                          vinfo_t* newitem) {
	/* consumes a reference to 'newitem' */
	vinfo_array_grow(vi, index+1);
	vinfo_xdecref(vi->array->items[index], po);
	vi->array->items[index] = newitem;
}


/* array management */
EXTERNFN void clear_tmp_marks(vinfo_array_t* array);
#ifdef ALL_CHECKS
EXTERNFN void assert_cleared_tmp_marks(vinfo_array_t* array);
#else
inline void assert_cleared_tmp_marks(vinfo_array_t* array) { }   /* nothing */
#endif
EXTERNFN bool array_contains(vinfo_array_t* array, vinfo_t* vi);
EXTERNFN void duplicate_array(vinfo_array_t* target, vinfo_array_t* source);
inline void deallocate_array(vinfo_array_t* array, PsycoObject* po) {
	int i = array->count;
	while (i--) vinfo_xdecref(array->items[i], po);
}
inline void array_delete(vinfo_array_t* array, PsycoObject* po) {
	deallocate_array(array, po);
	array_release(array);
}


/*****************************************************************/
 /***   PsycoObject: state of the compiler                      ***/


struct PsycoObject_s {
  /* used to be a Python object, hence the name */

  /* first, the description of variable stages. This is the data against
     which state matches and synchronizations are performed. */
  int stack_depth;         /* the size of data currently pushed in the stack */
  vinfo_array_t vlocals;           /* all the 'vinfo_t' variables            */
  vinfo_t* reg_array[REG_TOTAL];   /* the 'vinfo_t' currently stored in regs */
  vinfo_t* ccreg;                  /* processor condition codes (aka flags)  */

  /* next, compiler private variables for producing and optimizing code. */
  reg_t last_used_reg;          /* the most recently used register            */
  int arguments_count;          /* # run-time arguments given to the function */
  int respawn_cnt;                  /* see psyco_prepare_respawn()           */
  CodeBufferObject* respawn_proxy;  /* see psyco_prepare_respawn()           */
  code_t* code;                /* where the emitted code goes                */
  code_t* codelimit;           /* do not write code past this limit          */
  pyc_data_t pr;               /* private language-dependent data            */
};

/* run-time vinfo_t creation */
inline vinfo_t* new_rtvinfo(PsycoObject* po, reg_t reg, bool ref) {
	vinfo_t* vi = vinfo_new(RunTime_New(reg, ref));
	REG_NUMBER(po, reg) = vi;
	return vi;
}

/* move 'vsource->source' into 'vtarget->source'. Must be the last reference
   to 'vsource', which is freed. 'vsource' must have no array, and
   'vtarget->source' must hold no reference to anything. In short, this
   function must not be used except by virtual-time computers. */
inline void vinfo_move(PsycoObject* po, vinfo_t* vtarget, vinfo_t* vsource)
{
	Source src = vtarget->source = vsource->source;
	if (is_runtime(src) && !is_reg_none(src))
		REG_NUMBER(po, getreg(src)) = vtarget;
	VINFO_FREE_1(vsource);
}


/*****************************************************************/
 /***   Compiler language-independent functions                 ***/

/* compilation */

/* Main compiling function. Emit machine code corresponding to the state
   'po'. The compiler produces its code into 'code' and the return value is
   the end of the written code. 'po' is freed.

   Be sure to call po->vlocals.clear_tmp_marks() before this function.

   'continue_compilation' is normally false. When compile() is called
   during the compilation of 'po', 'continue_compilation' is true and
   psyco_compile() may return NULL to tell the caller to continue the
   compilation of 'po' itself. The sole purpose of this is to reduce the
   depth of recursion of the C stack.
*/
EXTERNFN code_t* psyco_compile(PsycoObject* po, bool continue_compilation);

/* Conditional compilation: the state 'po' is compiled to be executed only if
   'condition' holds. In general this creates a coding pause for it to be
   compiled later. It always makes a copy of 'po' so that the original can be
   used to compile the other case ('not condition'). 'condition' must not be
   CC_ALWAYS_xxx here.
*/
EXTERNFN void psyco_compile_cond(PsycoObject* po, condition_code_t condition);

/* Simplified interface to compile() without using a previously
   existing code buffer. Return a new code buffer. */
EXTERNFN CodeBufferObject* psyco_compile_code(PsycoObject* po);

/* Prepare a 'coding pause', i.e. a short amount of code (proxy) that will be
   called only if the execution actually reaches it to go on with compilation.
   'this' is the PsycoObject corresponding to the proxy.
   'condition' may not be CC_ALWAYS_FALSE.
   The (possibly conditional) jump to the proxy is encoded in 'calling_code'.
   When the execution reaches the proxy, 'resume_fn' is called and the proxy
   destroys itself and replaces the original jump to it by a jump to the newly
   compiled code. */
typedef code_t* (*resume_fn_t)(PsycoObject* po, void* extra);
EXTERNFN void psyco_coding_pause(PsycoObject* po, condition_code_t jmpcondition,
				 resume_fn_t resume_fn,
				 void* extra, int extrasize);

/* management functions; see comments in compiler.c */
#ifdef ALL_CHECKS
EXTERNFN void psyco_assert_coherent(PsycoObject* po);
#else
inline void psyco_assert_coherent(PsycoObject* po) { }   /* nothing */
#endif

/* construction */
inline PsycoObject* PsycoObject_New(void) {
	PsycoObject* po = (PsycoObject*) PyCore_MALLOC(sizeof(PsycoObject));
        if (po == NULL)
		OUT_OF_MEMORY();
	memset(po, 0, sizeof(PsycoObject));
	return po;
}
EXTERNFN PsycoObject* psyco_duplicate(PsycoObject* po);  /* internal */
inline PsycoObject* PsycoObject_Duplicate(PsycoObject* po) {
	clear_tmp_marks(&po->vlocals);
	return psyco_duplicate(po);
}
EXTERNFN void PsycoObject_Delete(PsycoObject* po);


#endif /* _VCOMPILER_H */
