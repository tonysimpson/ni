#include "piterobject.h"
#if HAVE_GENERATORS


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
	
	seq = get_array_item(po, v, SEQITER_IT_SEQ);
	if (seq == NULL)
		return NULL;

	index = read_array_item(po, v, SEQITER_IT_INDEX);
	if (index == NULL)
		return NULL;

	result = PsycoSequence_GetItem(po, seq, index);
	if (result == NULL) {
		vinfo_t* matches = PycException_Matches(po, PyExc_IndexError);
		if (runtime_NON_NULL_t(po, matches) == true) {
			PycException_SetVInfo(po, PyExc_StopIteration,
					      psyco_vi_None());
		}
	}
	else {
		/* very remotely potential incompatibility: when exhausted,
		   the internal iterator index is not incremented. Python
		   is not consistent in this respect. This could be an
		   issue if an iterator of a mutable object is not
		   immediately deleted when exhausted. Well, I guess that
		   muting an object we iterate over is generally considered
		   as DDIWWY (Don't Do It -- We Warned You.) */
		vinfo_t* index_plus_1 = integer_add_i(po, index, 1);
		if (index_plus_1 == NULL) {
			vinfo_decref(result, po);
			result = NULL;
		}
		else {
			write_array_item(po, v, SEQITER_IT_INDEX, index_plus_1);
			vinfo_decref(index_plus_1, po);
		}
	}
	vinfo_decref(index, po);
	return result;
}


static bool compute_seqiter(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* seq;
	vinfo_t* index;
	vinfo_t* newobj;

	index = vinfo_getitem(v, SEQITER_IT_INDEX);
	if (index == NULL)
		return false;

	seq = vinfo_getitem(v, SEQITER_IT_SEQ);
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


INITIALIZATIONFN
void psy_iterobject_init(void)
{
	psyco_computed_seqiter.compute_fn = &compute_seqiter;
        Psyco_DefineMeta(PySeqIter_Type.tp_iter, &piter_getiter);
        Psyco_DefineMeta(PySeqIter_Type.tp_iternext, &piter_next);
}


#else /* !HAVE_GENERATORS */

INITIALIZATIONFN
void psy_iterobject_init(void)
{
}

#endif /* HAVE_GENERATORS */
