 /***************************************************************/
/***            Psyco equivalent of dictobject.h               ***/
 /***************************************************************/

#ifndef _PSY_DICTOBJECT_H
#define _PSY_DICTOBJECT_H


#include "pobject.h"
#include "pabstract.h"


/*  Definition hacks for Python version <2.2b1.
 */

#if !HAVE_struct_dictobject

/* WARNING: these definitions are taken from Python 2.1. There are
   *** incompatible *** with Python 2.2. For 2.2 the structure is
   publically visible, so there is no problem. Hopefully there are
   not too many intermediary versions out there that define
   2.2-style structures without making them public. */
   
typedef struct {
	long me_hash;      /* cached hash code of me_key */
	PyObject *me_key;
	PyObject *me_value;
#ifdef USE_CACHE_ALIGNED
	long	aligner;
#endif
} PyDictEntry;
typedef struct _dictobject PyDictObject;
struct _dictobject {
	PyObject_HEAD
	int ma_fill;  /* # Active + # Dummy */
	int ma_used;  /* # Active */
	int ma_size;  /* total # slots in ma_table */
	int ma_poly;  /* appopriate entry from polys vector */
	PyDictEntry *ma_table;
	PyDictEntry *(*ma_lookup)(PyDictObject *mp, PyObject *key, long hash);
};
#endif  /* !HAVE_struct_dictobject */


EXTERNFN vinfo_t* PsycoDict_New(PsycoObject* po);
EXTERNFN vinfo_t* PsycoDict_Copy(PsycoObject* po, vinfo_t* orig);


#endif /* _PSY_LISTOBJECT_H */
