 /***************************************************************/
/***            Psyco equivalent of dictobject.h               ***/
 /***************************************************************/

#ifndef _PSY_DICTOBJECT_H
#define _PSY_DICTOBJECT_H


#include "pobject.h"
#include "pabstract.h"


EXTERNFN vinfo_t* PsycoDict_New(PsycoObject* po);


EXTERNFN void psy_dictobject_init(void);

#endif /* _PSY_LISTOBJECT_H */
