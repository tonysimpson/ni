 /***************************************************************/
/***            Psyco equivalent of tupleobject.h              ***/
 /***************************************************************/

#ifndef _PSY_TUPLEOBJECT_H
#define _PSY_TUPLEOBJECT_H


#include "pobject.h"
#include "pabstract.h"


#define TUPLE_OB_ITEM       QUARTER(offsetof(PyTupleObject, ob_item))

/* The following macro reads an item from a Psyco tuple without any
   checks. Be sure the item has already been loaded in the array of
   the vinfo_t. This should only be used after a successful call to
   PsycoTuple_Load(). */
#define PsycoTuple_GET_ITEM(vtuple, index)  \
		((vtuple)->array->items[TUPLE_OB_ITEM + (index)])


/***************************************************************/
/* virtual tuples.
   If 'source' is not NULL it gives the content of the tuple.
   If 'source' is NULL you have to initialize it yourself. */
EXTERNFN vinfo_t* PsycoTuple_New(int count, vinfo_t** source);

/* get the (possibly virtual) array of items in the tuple,
   returning the length of the tuple or -1 if it fails (items not known).
   The items are then found in tuple->array->items[TUPLE_OB_ITEM+i].
   Never sets a PycException. */
EXTERNFN int PsycoTuple_Load(vinfo_t* tuple);


#endif /* _PSY_TUPLEOBJECT_H */
