 /***************************************************************/
/***           Psyco equivalent of methodobject.h              ***/
 /***************************************************************/

#ifndef _PSY_METHODOBJECT_H
#define _PSY_METHODOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define CFUNC_M_ML          QUARTER(offsetof(PyCFunctionObject, m_ml))
#define CFUNC_M_SELF        QUARTER(offsetof(PyCFunctionObject, m_self))
#define CFUNC_SIZE          (CFUNC_M_SELF+1)


EXTERNFN vinfo_t* PsycoCFunction_Call(PsycoObject* po, vinfo_t* func,
                                      vinfo_t* tuple, vinfo_t* kw);


 /***************************************************************/
  /***   Virtual-time object builder                           ***/

/* not-yet-computed C method objects, with a m_ml and m_self field.
   Usually not computed at all, but if it needs be, will call
   PyCFunction_New(). */
EXTERNVAR source_virtual_t psyco_computed_cfunction;

inline vinfo_t* PsycoCFunction_New(PsycoObject* po, PyMethodDef* ml,
                                   vinfo_t* self)
{
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_cfunction));
	result->array = array_new(CFUNC_SIZE);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyCFunction_Type)));
	result->array->items[CFUNC_M_ML] =
		vinfo_new(CompileTime_New((long) ml));
	vinfo_incref(self);
	result->array->items[CFUNC_M_SELF] = self;
	return result;
}


#endif /* _PSY_METHODOBJECT_H */
