 /***************************************************************/
/***            Psyco equivalent of listobject.h               ***/
 /***************************************************************/

#ifndef _PSY_LISTOBJECT_H
#define _PSY_LISTOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define LIST_OB_ITEM        QUARTER(offsetof(PyListObject, ob_item))


EXTERNFN vinfo_t* PsycoList_New(PsycoObject* po, int size);


EXTERNFN void psy_listobject_init(void);

#endif /* _PSY_LISTOBJECT_H */
