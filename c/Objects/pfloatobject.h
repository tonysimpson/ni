 /***************************************************************/
/***             Psyco equivalent of floatobject.h               ***/
 /***************************************************************/

#ifndef _PSY_FLOATOBJECT_H
#define _PSY_FLOATOBJECT_H

#include "pobject.h"
#include "pabstract.h"
#include "pintobject.h"

#define FLOAT_OB_FVAL       QUARTER(offsetof(PyFloatObject, ob_fval))

#define PsycoFloat_Check(tp) PyType_TypeCheck(tp, &PyFloat_Type)


/***************************************************************/
/***   Virtual-time object builder                           ***/

/* not-yet-computed integers; it will call PyFloat_FromDouble */
EXTERNVAR source_virtual_t psyco_computed_float;

/* !! consumes a references to vdouble. PsycoFloat_FromLong() does not. */
inline vinfo_t* PsycoFloat_FROM_DOUBLE(vinfo_t* vdouble1, vinfo_t* vdouble2)
{
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_float));
	result->array = array_new(FLOAT_OB_FVAL+2);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyFloat_Type)));
	result->array->items[FLOAT_OB_FVAL] = vdouble1;
	result->array->items[FLOAT_OB_FVAL+1] = vdouble2;
	return result;
}

inline vinfo_t* PsycoFloat_FromDouble(vinfo_t* vdouble1, vinfo_t* vdouble2)
{
	vinfo_incref(vdouble1);
	vinfo_incref(vdouble2);
	return PsycoFloat_FROM_DOUBLE(vdouble1, vdouble2);
}

inline vinfo_t* PsycoFloat_AS_DOUBLE_1(PsycoObject* po, vinfo_t* v)
{	/* no type check; does not return a new reference. */
	return get_array_item(po, v, FLOAT_OB_FVAL);
}

inline vinfo_t* PsycoFloat_AS_DOUBLE_2(PsycoObject* po, vinfo_t* v)
{	/* no type check; does not return a new reference. */
	return get_array_item(po, v, FLOAT_OB_FVAL+1);
}

/* return a new ref */
EXTERNFN bool PsycoFloat_AsDouble(PsycoObject* po, vinfo_t* v, vinfo_t** vd1, vinfo_t** vd2);

/*EXTERNFN condition_code_t float_cmp(PsycoObject* po, vinfo_t* a1, vinfo_t* a2, 
  vinfo_t* b1, vinfo_t* b2, int op);*/

EXTERNFN void psy_floatobject_init(void);

#endif /* _PSY_FLOATOBJECT_H */
