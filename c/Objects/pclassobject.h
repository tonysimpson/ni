 /***************************************************************/
/***            Psyco equivalent of classobject.h              ***/
 /***************************************************************/

#ifndef _PSY_CLASSOBJECT_H
#define _PSY_CLASSOBJECT_H


#include "pobject.h"
#include "pabstract.h"


/* Instance methods */
#define METHOD_IM_FUNC         QUARTER(offsetof(PyMethodObject, im_func))
#define METHOD_IM_SELF         QUARTER(offsetof(PyMethodObject, im_self))
#define METHOD_IM_CLASS        QUARTER(offsetof(PyMethodObject, im_class))
#define METHOD_SIZE            (METHOD_IM_CLASS+1)


 /***************************************************************/
  /***   Virtual-time object builder                           ***/

/* not-yet-computed instance method objects. Usually not computed at all,
   but if it needs be, will call PyMethod_New(). */
EXTERNVAR source_virtual_t psyco_computed_method;

EXTERNFN
vinfo_t* PsycoMethod_New(PyObject* func, vinfo_t* self, PyObject* cls);


EXTERNFN void psy_classobject_init(void);


#endif /* _PSY_CLASSOBJECT_H */
