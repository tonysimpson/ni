 /***************************************************************/
/***            Psyco equivalent of iterobject.h               ***/
 /***************************************************************/

#ifndef _PSY_ITEROBJECT_H
#define _PSY_ITEROBJECT_H


#include "pobject.h"
#include "pabstract.h"

#if HAVE_GENERATORS


/* this structure not exported by iterobject.h */
typedef struct {
	PyObject_HEAD
	long      it_index;
	PyObject *it_seq;
} seqiterobject;

#define SEQITER_IT_INDEX    QUARTER(offsetof(seqiterobject, it_index))
#define SEQITER_IT_SEQ      QUARTER(offsetof(seqiterobject, it_seq))
#define SEQITER_IT_MAX      SEQITER_IT_SEQ


/*********************************************************************/
 /* Virtual sequence iterators. Created if needed by PySetIter_New(). */
EXTERNVAR source_virtual_t psyco_computed_seqiter;

/* !! consumes a ref on 'seq' */
EXTERNFN vinfo_t* PsycoSeqIter_NEW(PsycoObject* po, vinfo_t* seq);

inline vinfo_t* PsycoSeqIter_New(PsycoObject* po, vinfo_t* seq)
{
	vinfo_incref(seq);
	return PsycoSeqIter_NEW(po, seq);
}


#endif /* HAVE_GENERATORS */
#endif /* _PSY_ITEROBJECT_H */
