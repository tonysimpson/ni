 /***************************************************************/
/***             Psyco equivalent of object.h                  ***/
 /***************************************************************/

#ifndef _PSY_OBJECT_H
#define _PSY_OBJECT_H


#include "../Python/pycompiler.h"


#define OB_REFCOUNT         QUARTER(offsetof(PyObject, ob_refcnt))
#define OB_TYPE             QUARTER(offsetof(PyObject, ob_type))
#define VAR_OB_SIZE         QUARTER(offsetof(PyVarObject, ob_size))


/* common type checkers, rewritten because in Psyco we manipulate type
   objects directly and Python's usual macros insist on taking a regular
   PyObject* whose type is checked. */
#if NEW_STYLE_TYPES
# define PyType_TypeCheck(tp1, tp)  	\
	((tp1) == (tp) || PyType_IsSubtype((tp1), (tp)))
#else
# define PyType_TypeCheck(tp1, tp)    ((tp1) == (tp))
#endif

#define PsycoIter_Check(tp) \
    (PyType_HasFeature(tp, Py_TPFLAGS_HAVE_ITER) && \
     (tp)->tp_iternext != NULL)

#define PsycoSequence_Check(tp) \
	((tp)->tp_as_sequence && (tp)->tp_as_sequence->sq_item != NULL)


/* Return the type of an object, or NULL in case of exception (typically
   a promotion exception). */
inline PyTypeObject* Psyco_NeedType(PsycoObject* po, vinfo_t* vi) {
	 vinfo_t* vtp = get_array_item(po, vi, OB_TYPE);
	 if (vtp == NULL)
		 return NULL;
	 return (PyTypeObject*) psyco_pyobj_atcompiletime(po, vtp);
}
inline PyTypeObject* Psyco_FastType(vinfo_t* vi) {
	/* only call this when you know the type has already been
	   loaded by a previous Psyco_NeedType() */
	vinfo_t* vtp = vinfo_getitem(vi, OB_TYPE);
	extra_assert(vtp != NULL && is_compiletime(vtp->source));
	return (PyTypeObject*) (CompileTime_Get(vtp->source)->value);
}
/* Check for the type of an object. Returns the index in the set 'fs' or
   -1 if not in the set (or if exception). Used this is better than
   Psyco_NeedType() if you are only interested in some types, not all of them. */
inline int Psyco_TypeSwitch(PsycoObject* po, vinfo_t* vi, fixed_switch_t* fs) {
	vinfo_t* vtp = get_array_item(po, vi, OB_TYPE);
	 if (vtp == NULL)
		 return -1;
	 return psyco_switch_index(po, vtp, fs);
}


EXTERNFN condition_code_t PsycoObject_IsTrue(PsycoObject* po, vinfo_t* vi);
EXTERNFN vinfo_t* PsycoObject_Repr(PsycoObject* po, vinfo_t* vi);

/* Note: DelAttr() is SetAttr() with 'v' set to NULL (and not some vinfo_t
   that would happend to contain zero). */
EXTERNFN vinfo_t* PsycoObject_GetAttr(PsycoObject* po, vinfo_t* o,
                                      vinfo_t* attr_name);
EXTERNFN bool PsycoObject_SetAttr(PsycoObject* po, vinfo_t* o,
                                  vinfo_t* attr_name, vinfo_t* v);

EXTERNFN vinfo_t* PsycoObject_RichCompare(PsycoObject* po, vinfo_t* v,
					  vinfo_t* w, int op);
EXTERNFN condition_code_t PsycoObject_RichCompareBool(PsycoObject* po,
						      vinfo_t* v,
						      vinfo_t* w, int op);


/* a quick way to specify the type of the object returned by an operation
   when it is known, without having to go into all the details of the
   operation itself (be careful, you must be *sure* of the return type): */
#define DEF_KNOWN_RET_TYPE_1(cname, op, flags, knowntype)			\
static vinfo_t* cname(PsycoObject* po, vinfo_t* v1) {				\
	vinfo_t* result = psyco_generic_call(po, op, flags, "v", v1);		\
	if (result != NULL) {							\
		set_array_item(po, result, OB_TYPE,				\
			       vinfo_new(CompileTime_New((long)(&knowntype))));	\
	}									\
	return result;								\
}
#define DEF_KNOWN_RET_TYPE_2(cname, op, flags, knowntype)			\
static vinfo_t* cname(PsycoObject* po, vinfo_t* v1,				\
				      vinfo_t* v2) {				\
	vinfo_t* result = psyco_generic_call(po, op, flags, "vv", v1, v2);	\
	if (result != NULL) {							\
		set_array_item(po, result, OB_TYPE,				\
			       vinfo_new(CompileTime_New((long)(&knowntype))));	\
	}									\
	return result;								\
}


/* definition of commonly used "fixed switches", i.e. sets of
   values (duplicating fixed switch structures for the same set
   of value can inccur a run-time performance loss in some cases) */

/* the variable names list the values in order.
   'int' means '&PyInt_Type' etc. */
EXTERNVAR fixed_switch_t psyfs_int;
EXTERNVAR fixed_switch_t psyfs_int_long;
EXTERNVAR fixed_switch_t psyfs_tuple_list;
EXTERNVAR fixed_switch_t psyfs_string_unicode;


inline void psy_object_init(void)
{
	long values[2];
        int cnt;

	values[0] = (long)(&PyInt_Type);
	psyco_build_run_time_switch(&psyfs_int, SkFlagFixed, values, 1);

        values[0] = (long)(&PyInt_Type);
	values[1] = (long)(&PyLong_Type);
        psyco_build_run_time_switch(&psyfs_int_long, SkFlagFixed, values, 2);

	values[0] = (long)(&PyTuple_Type);
	values[1] = (long)(&PyList_Type);
        psyco_build_run_time_switch(&psyfs_tuple_list, SkFlagFixed, values, 2);

	values[0] = (long)(&PyString_Type);
#ifdef Py_USING_UNICODE
	values[1] = (long)(&PyUnicode_Type);
        cnt = 2;
#else
        cnt = 1;
#endif
        psyco_build_run_time_switch(&psyfs_string_unicode, SkFlagFixed,
                                    values, cnt);
}

#endif /* _PSY_OBJECT_H */
