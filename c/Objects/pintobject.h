 /***************************************************************/
/***             Psyco equivalent of intobject.h               ***/
 /***************************************************************/

#ifndef _PSY_INTOBJECT_H
#define _PSY_INTOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define INT_OB_IVAL         QUARTER(offsetof(PyIntObject, ob_ival))


#define PsycoInt_Check(tp) PyType_TypeCheck(tp, &PyInt_Type)


 /***************************************************************/
  /***   Virtual-time object builder                           ***/

/* not-yet-computed integers; it will call PyInt_FromLong */
EXTERNVAR source_virtual_t psyco_computed_int;

/* !! consumes a reference to vlong. PsycoInt_FromLong() does not. */
inline vinfo_t* PsycoInt_FROM_LONG(vinfo_t* vlong)
{
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_int));
	result->array = array_new(INT_OB_IVAL+1);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyInt_Type)));
	result->array->items[INT_OB_IVAL] = vlong;
	return result;
}
inline vinfo_t* PsycoInt_FromLong(vinfo_t* vlong)
{
	vinfo_incref(vlong);
	return PsycoInt_FROM_LONG(vlong);
}

inline vinfo_t* PsycoInt_AS_LONG(PsycoObject* po, vinfo_t* v)
{	/* no type check; does not return a new reference. */
	return get_array_item(po, v, INT_OB_IVAL);
}

/* return a new ref */
EXTERNFN vinfo_t* PsycoInt_AsLong(PsycoObject* po, vinfo_t* v);


inline vinfo_t* PsycoIntInt_RichCompare(PsycoObject* po, vinfo_t* v,
					vinfo_t* w, int op)
{	/* only for two integer objects */
	vinfo_t* a;
	vinfo_t* b;
	condition_code_t cc;
	a = PsycoInt_AS_LONG(po, v);
	if (a == NULL) return NULL;
	b = PsycoInt_AS_LONG(po, w);
	if (b == NULL) return NULL;
	cc = integer_cmp(po, a, b, op);
	if (cc == CC_ERROR) return NULL;
	return PsycoInt_FROM_LONG(psyco_vinfo_condition(po, cc));
}


#endif /* _PSY_INTOBJECT_H */
