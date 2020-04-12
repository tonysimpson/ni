/***************************************************************/
/***            Psyco equivalent of longobject.h               ***/
/***************************************************************/

#ifndef _PSY_LONGOBJECT_H
#define _PSY_LONGOBJECT_H

#include "pabstract.h"
#include "pobject.h"

/* XXX tony: We should implement a virtual-time computed version of
PyLong */

#define PsycoLong_Check(tp) PyType_TypeCheck(tp, &PyLong_Type)

EXTERNFN vinfo_t *PsycoLong_AsLong(PsycoObject *po, vinfo_t *v);
EXTERNFN bool PsycoLong_AsDouble(PsycoObject *po, vinfo_t *v, vinfo_t **vd);
EXTERNFN vinfo_t *PsycoLong_FromUnsignedLong(PsycoObject *po, vinfo_t *v);
EXTERNFN vinfo_t *PsycoLong_FromLong(PsycoObject *po, vinfo_t *v);
EXTERNFN vinfo_t *PsycoLong_FROM_LONG(PsycoObject *po, vinfo_t *v); /* consumes v */

#endif /* _PSY_LONGOBJECT_H */
