#include "piterobject.h"


static vinfo_t* piter_getiter(PsycoObject* po, vinfo_t* v)
{
	vinfo_incref(v);
	return v;
}

static vinfo_t* piter_next(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* seq;
	vinfo_t* index;
	vinfo_t* result;
	vinfo_t* index_plus_1;
	
	seq = get_array_item(po, v, SEQITER_IT_SEQ);
	if (seq == NULL)
		return NULL;

	index = get_array_item(po, v, SEQITER_IT_INDEX);
	if (index == NULL)
		return NULL;
	index_plus_1 = integer_add_i(po, index, 1);
	if (index_plus_1 == NULL)
		return NULL;

	result = PsycoSequence_GetItem(po, seq, index);

	set_array_item(po, v, SEQITER_IT_INDEX, index_plus_1);

	if (result == NULL) {
		condition_code_t cc;
		cc = PycException_Matches(po, PyExc_IndexError);
		if (cc != CC_ERROR && runtime_condition_t(po, cc)) {
			vinfo_incref(psyco_viNone);
			PycException_SetVInfo(po, PyExc_StopIteration,
					      psyco_viNone);
		}
	}
	return result;
}


static bool compute_seqiter(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* seq;
	vinfo_t* index;
	vinfo_t* newobj;

	index = get_array_item(po, v, SEQITER_IT_INDEX);
	if (index == NULL)
		return false;

	seq = get_array_item(po, v, SEQITER_IT_SEQ);
	if (seq == NULL)
		return false;

	newobj = psyco_generic_call(po, PySeqIter_New,
				    CfReturnRef|CfPyErrIfNull, "v", seq);
	if (newobj == NULL)
		return false;

	/* Put the current index into the seq iterator.
	   This is done by putting the value directly in the
	   seqiterobject structure; it could be done by calling
	   PyIter_Next() n times but obviously that's not too
	   good a solution */
	if (!psyco_knowntobe(index, 0)) {
		if (!write_array_item(po, v, SEQITER_IT_INDEX, index)) {
			vinfo_decref(newobj, po);
			return false;
		}
	}

	/* Remove the SEQITER_IT_INDEX entry from v->array because it
	   is a mutable field now, and could be changed at any time by
	   anybody .*/
	v->array->items[SEQITER_IT_INDEX] = NULL;
	vinfo_decref(index, po);

	vinfo_move(po, v, newobj);
	return true;
}

DEFINEVAR source_virtual_t psyco_computed_seqiter;


DEFINEFN
void psy_iterobject_init()
{
	psyco_computed_seqiter.compute_fn = &compute_seqiter;
        Psyco_DefineMeta(PySeqIter_Type.tp_iter, &piter_getiter);
        Psyco_DefineMeta(PySeqIter_Type.tp_iternext, &piter_next);
}
