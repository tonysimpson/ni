 /***************************************************************/
/***            Psyco equivalent of iterobject.h               ***/
 /***************************************************************/

#ifndef _PSY_ITEROBJECT_H
#define _PSY_ITEROBJECT_H


#include "pobject.h"
#include "pabstract.h"


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
inline vinfo_t* PsycoSeqIter_NEW(vinfo_t* seq)
{
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_seqiter));
	result->array = array_new(SEQITER_IT_MAX+1);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PySeqIter_Type)));
	/* don't use psyco_viZero here because it is better without
	   SkFlagFixed */
	result->array->items[SEQITER_IT_INDEX] =
		vinfo_new(CompileTime_New(0));
	result->array->items[SEQITER_IT_SEQ] = seq;
	return result;
}
inline vinfo_t* PsycoSeqIter_New(vinfo_t* seq)
{
	vinfo_incref(seq);
	return PsycoSeqIter_NEW(seq);
}


EXTERNFN void psy_iterobject_init();

#endif /* _PSY_ITEROBJECT_H */
