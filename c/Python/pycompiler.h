 /***************************************************************/
/***          Language-dependent part of the compiler          ***/
 /***************************************************************/

#ifndef _PYCOMPILER_H
#define _PYCOMPILER_H


#include "pycheader.h"
#include "../vcompiler.h"
#include "../processor.h"
#include "../dispatcher.h"


#ifdef PyString_CheckExact
# define NEW_STYLE_TYPES       1    /* Python >=2.2b1 */
#else
# define NEW_STYLE_TYPES       0
# define PyString_CheckExact   PyString_Check
#endif

#if !NEW_STYLE_TYPES
# error "Sorry, Psyco requires Python 2.2."
/* Some parts could work without NEW_STYLE_TYPES but this is largely untested
   and would require a number of fixes in the source. */
#endif


#define MAX3(a,b,c)  ((a)>(b)?((a)>(c)?(a):(c)):(b)>(c)?(b):(c))


/*****************************************************************/
 /***   Common constants                                        ***/

/* the following known values have the SkFlagFixed set */
#define DEF_SK_AND_VI(name)                             \
EXTERNVAR source_known_t psyco_sk##name;                \
inline vinfo_t* psyco_vi_##name(void) {                 \
  sk_incref(&psyco_sk##name);                           \
  return vinfo_new(CompileTime_NewSk(&psyco_sk##name)); \
}

DEF_SK_AND_VI(None)    /* known value 'Py_None' */
DEF_SK_AND_VI(Zero)    /* known value 0 */
DEF_SK_AND_VI(One)     /* known value 1 */
DEF_SK_AND_VI(NotImplemented)   /* 'Py_NotImplemented' */

     /* the macro defines psyco_vi_None(), psyco_vi_Zero(),
        psyco_vi_One() and psyco_vi_NotImplemented(). */

#undef DEF_SK_AND_VI


/*****************************************************************/
 /***   Compile-time Pseudo exceptions                          ***/


/*****
 * A pseudo-exception is the compile-time equivalent of a Python exception.
 * They are encoded in the fields 'exc' and 'val' of pyc_data_t. For real
 * Python the translation is immediate: 'exc' describes the PyObject*
 * pointing to the Python exception class, and 'val' is the associated value.
 *
 * For the other pseudo-exceptions, like special events breaking the Python
 * main loop (returns, break, continue), 'exc' is a virtual value using
 * one of the following non-computable virtual sources.
 *
 * 'ERtPython' is particular: it is the virtual equivalent of "the exception
 * currently set at run-time". Use PycException_Fetch() to emit the actual
 * call to PyErr_Fetch().
 */
EXTERNVAR source_virtual_t ERtPython;  /* Exception raised by Python */
EXTERNVAR source_virtual_t EReturn;    /* 'return' statement */
EXTERNVAR source_virtual_t EBreak;     /* 'break' statement */
EXTERNVAR source_virtual_t EContinue;  /* 'continue' statement */


/* Check whether a pseudo-exception is currently set */
inline bool PycException_Occurred(PsycoObject* po) {
	return po->pr.exc != NULL;
}


/* raise an arbitrary pseudo-exception (consumes the references) */
EXTERNFN void PycException_Clear(PsycoObject* po);
inline void PycException_Raise(PsycoObject* po, vinfo_t* exc, vinfo_t* val) {
	if (PycException_Occurred(po))
		PycException_Clear(po);
	po->pr.exc = exc;
	po->pr.val = val;
}

/* for Python exceptions detected at compile-time */
EXTERNFN void PycException_SetString(PsycoObject* po,
				     PyObject* e, const char* text);
EXTERNFN void PycException_SetFormat(PsycoObject* po,
				     PyObject* e, const char* fmt, ...);
 /* consumes a reference on 'v': */
EXTERNFN void PycException_SetObject(PsycoObject* po, PyObject* e, PyObject* v);
 /* consumes a reference on 'v': */
EXTERNFN void PycException_SetVInfo(PsycoObject* po, PyObject* e, vinfo_t* v);

/* checking for the Python class of an exception */
EXTERNFN vinfo_t* PycException_Matches(PsycoObject* po, PyObject* e);

inline bool PycException_Is(PsycoObject* po, source_virtual_t* sv) {
	return po->pr.exc->source == VirtualTime_New(sv);
}
inline bool PycException_IsPython(PsycoObject* po) {
	Source src = po->pr.exc->source;
	if (is_virtualtime(src)) {
		return !(src == VirtualTime_New(&EReturn) ||
			 src == VirtualTime_New(&EBreak) ||
			 src == VirtualTime_New(&EContinue) ||
			 psyco_vsource_is_promotion(src));
	}
	else
		return true;
};

/* fetch a Python exception set at compile-time (that is, now) and turn into
   a pseudo-exception (typically to be re-raised at run-time). */
EXTERNFN void psyco_virtualize_exception(PsycoObject* po);

/* fetch a Python exception set at run-time (that is, a ERtPython) and turn into
   a pseudo-exception. This is a no-op if !PycException_Is(po, &ERtPython). */
EXTERNFN void PycException_Fetch(PsycoObject* po);


/*****************************************************************/
 /***   Promotion                                               ***/

/* Raise a pseudo-exception meaning "promote 'vi' from run-time to
   compile-time". If 'promotion->fs' is not NULL, only promote if the
   run-time value turns out to be in the given set.
*/
EXTERNFN void PycException_Promote(PsycoObject* po,
                                   vinfo_t* vi, c_promotion_t* promotion);

/* A powerful function: it appears to return the value of the
   variable 'vi', even if it is a run-time variable. Implemented
   by raising a EPromotion exception if needed. Returns -1 in
   this case; use PycException_Occurred() to know if it is really
   an exception or a plain normal -1. */
inline long psyco_atcompiletime(PsycoObject* po, vinfo_t *vi) {
	NonVirtualSource src = vinfo_compute(vi, po);
	if (src == SOURCE_ERROR)
		return -1;
	if (is_runtime(src)) {
		PycException_Promote(po, vi, &psyco_nonfixed_promotion);
		return -1;
	}
	else {
		source_known_t* sk = CompileTime_Get(src);
		sk->refcount1_flags |= SkFlagFixed;
		return sk->value;
	}
}
/* the same if the value to promote is itself a PyObject* which can be
   used as key in the look-up dictionary */
inline PyObject* psyco_pyobj_atcompiletime(PsycoObject* po, vinfo_t *vi) {
	NonVirtualSource src = vinfo_compute(vi, po);
	if (src == SOURCE_ERROR)
		return NULL;
	if (is_runtime(src)) {
		PycException_Promote(po, vi, &psyco_nonfixed_pyobj_promotion);
		return NULL;
	}
	else {
		source_known_t* sk = CompileTime_Get(src);
		sk->refcount1_flags |= SkFlagFixed;
		return (PyObject*) sk->value;
	}
}

/* same as above, when the return value is used in a switch.
   In this case we must only promote the known values. So instead
   of writing 'switch (psyco_atcompiletime(po, vi))' you
   must write 'switch (psyco_switch_index(po, vi, fs))' */
inline int psyco_switch_index(PsycoObject* po, vinfo_t* vi, fixed_switch_t* fs) {
	NonVirtualSource src = vinfo_compute(vi, po);
	if (src == SOURCE_ERROR)
		return -1;
	if (is_runtime(src)) {
		if (!known_to_be_default(vi, fs))
			PycException_Promote(po, vi, &fs->fixed_promotion);
		return -1;
	}
	else
		return psyco_switch_lookup(fs, CompileTime_Get(src)->value);
}

/* lazy comparison. Returns true if 'vi' is non-NULL, compile-time, and has the
   given value, and false otherwise. */
inline bool psyco_knowntobe(vinfo_t* vi, long value) {
	return vi != NULL && is_compiletime(vi->source) &&
		CompileTime_Get(vi->source)->value == value;
}

/* comparison with a special PyObject* value, e.g. Py_NotImplemented; assume
   that virtual sources are never one of these special objects. The _f version
   assumes a generally false outcome, and the _t version a generally true one. */
/* --- disabled, use CfPyErrNotImplemented instead --- */
/* inline bool psyco_is_special_f(PsycoObject* po, vinfo_t* vi, */
/*                                PyObject* value) { */
/* 	return !is_virtualtime(vi->source) && */
/* 		runtime_condition_f(po, * integer_cmp_i does not fail here * */
/* 			    integer_cmp_i(po, vi, (long) value, Py_EQ)); */
/* } */
/* inline bool psyco_is_special_t(PsycoObject* po, vinfo_t* vi, PyObject* */
/*                                value) { */
/* 	return !is_virtualtime(vi->source) && */
/* 		runtime_condition_t(po, * integer_cmp_i does not fail here * */
/* 			    integer_cmp_i(po, vi, (long) value, Py_EQ)); */
/* } */


/*****************************************************************/
 /***   Exception utilities                                     ***/

/* Psyco meta-equivalent of PyErr_Occurred(). Not to be confused with
   PycException_Occurred(), which tells whether a Psyco-level exception
   is currently set. */
inline vinfo_t* psyco_PyErr_Occurred(PsycoObject* po) {
	if (PycException_Occurred(po) && PycException_IsPython(po)) {
		return psyco_vi_One();
	}
	else
		return psyco_generic_call(po, PyErr_Occurred, CfReturnNormal, "");
}


/*****************************************************************/
 /***   Meta functions                                          ***/

/* Each C function of the Python interpreter might be associated to a
   "meta" function from Psyco with the same signature but 'vinfo_t*' for
   the arguments and return value. The idea is that when such a meta
   function exists, Psyco can invoke it at compile-time to do (a part of)
   what the Python interpreter would do at run-time only. The code of the
   meta function typically ressembles that of the original function enough
   that we might dream about a language in which we never have to write
   the two versions (where Psyco's version could be derived automatically
   from the standard version).
*/

EXTERNVAR PyObject* Psyco_Meta_Dict;  /* key is a PyIntObject holding the
					 address of the C function, value is
					 a PyIntObject holding the address
					 of the corresponding Psyco function. */
EXTERNFN void Psyco_DefineMeta(void* c_function, void* psyco_function);
inline void* Psyco_Lookup(void* c_function) {
	PyObject* value;
	PyObject* key = PyInt_FromLong((long) c_function);
	if (key == NULL) OUT_OF_MEMORY();
	value = PyDict_GetItem(Psyco_Meta_Dict, key);
	Py_DECREF(key);
	if (value != NULL)
		return (void*) PyInt_AS_LONG(value);
	else
		return NULL;
}

/*** To map the content of a Python module to meta-implementations. ***/
/* Returns the module object (refcount *incremented*) or NULL if
   not found. It also prints some infos in verbose mode. */
EXTERNFN PyObject* Psyco_DefineMetaModule(char* modulename);
/* Returns an object from a module (refcount *incremented*) or NULL
   if not found. In verbose mode, 'not found' errors are printed. */
EXTERNFN PyObject* Psyco_GetModuleObject(PyObject* module, char* name,
                                         PyTypeObject* expected_type);
/* Maps a built-in object from a module to a meta-implementation.
   Returns a pointer to the C function itself. */
EXTERNFN PyCFunction Psyco_DefineModuleFn(PyObject* module, char* meth_name,
                                          int meth_flags, void* meta_fn);


/* the general-purpose calling routine: it looks for a meta implementation of
   'c_function' and call it if found; if not found, it encode a run-time call
   to 'c_function'. The 'flags' and 'arguments' are as in psyco_generic_call().
   The remaining arguments are given to the meta function or encoded in the
   run-time call; they should be compatible with the description given in
   'arguments'.
   This is a bit tricky, with one version of the macro per number of arguments,
   because the C processor is too limited and we want to avoid handling
   functions with '...' arguments all around
*/
#define Psyco_META1(po, c_function, flags, arguments, a1)		\
		Psyco_Meta1x(po, c_function, flags, arguments,		\
			     (long)(a1))
#define Psyco_META2(po, c_function, flags, arguments, a1, a2)		\
		Psyco_Meta2x(po, c_function, flags, arguments,		\
			     (long)(a1), (long)(a2))
#define Psyco_META3(po, c_function, flags, arguments, a1, a2, a3)	\
		Psyco_Meta3x(po, c_function, flags, arguments,		\
			     (long)(a1), (long)(a2), (long)(a3))
#define Psyco_META4(po, c_function, flags, arguments, a1, a2, a3, a4)	\
		Psyco_Meta4x(po, c_function, flags, arguments,		\
			     (long)(a1), (long)(a2), (long)(a3), (long)(a4))

#define Psyco_flag_META1	(condition_code_t) Psyco_META1
#define Psyco_flag_META2	(condition_code_t) Psyco_META2
#define Psyco_flag_META3	(condition_code_t) Psyco_META3
#define Psyco_flag_META4	(condition_code_t) Psyco_META4

EXTERNFN vinfo_t* Psyco_Meta1x(PsycoObject* po, void* c_function, int flags,
                               const char* arguments, long a1);
EXTERNFN vinfo_t* Psyco_Meta2x(PsycoObject* po, void* c_function, int flags,
                               const char* arguments, long a1, long a2);
EXTERNFN vinfo_t* Psyco_Meta3x(PsycoObject* po, void* c_function, int flags,
                               const char* arguments, long a1, long a2, long a3);
EXTERNFN vinfo_t* Psyco_Meta4x(PsycoObject* po, void* c_function, int flags,
                               const char* arguments,
                               long a1, long a2, long a3, long a4);


/******************************************************************/
 /*** pyc_data_t implementation and snapshots for the dispatcher ***/

/* construction for non-frozen snapshots */
EXTERNFN void pyc_data_build(PsycoObject* po, PyObject* merge_points);
inline void pyc_data_duplicate(pyc_data_t* target, pyc_data_t* source) {
	memcpy(target, source, sizeof(pyc_data_t));
	target->exc = NULL;
        target->val = NULL;
}
inline void pyc_data_release(pyc_data_t* pyc) {
	vinfo_xdecref(pyc->val, NULL);
	vinfo_xdecref(pyc->exc, NULL);
}

/* construction for frozen snapshots */
inline size_t frozen_size(pyc_data_t* pyc) {
	return offsetof(pyc_data_t, blockstack) + pyc->iblock*sizeof(PyTryBlock);
}
inline void frozen_copy(pyc_data_t* target, pyc_data_t* source) {
	memcpy(target, source, frozen_size(source));
}
inline pyc_data_t* pyc_data_new(pyc_data_t* original) {
	pyc_data_t* pyc = (pyc_data_t*) PyCore_MALLOC(frozen_size(original));
	if (pyc == NULL) OUT_OF_MEMORY();
	frozen_copy(pyc, original);
	return pyc;
}
inline void pyc_data_delete(pyc_data_t* pyc) {
	PyCore_FREE(pyc);
}


#endif /* _PYCOMPILER_H */
