 /***************************************************************/
/***            Psyco equivalent of longobject.h               ***/
 /***************************************************************/

#ifndef _PSY_LONGOBJECT_H
#define _PSY_LONGOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define PsycoLong_Check(tp) PyType_TypeCheck(tp, &PyLong_Type)


EXTERNFN vinfo_t* PsycoLong_AsLong(PsycoObject* po, vinfo_t* v);


EXTERNFN void psy_longobject_init();

#endif /* _PSY_LONGOBJECT_H */
