#include "plistobject.h"
#include "pintobject.h"
#include "../Python/pbltinmodule.h"


DEFINEFN
vinfo_t* PsycoList_New(PsycoObject* po, int size)
{
	/* no virtual lists (yet?) */
	vinfo_t *v = psyco_generic_call(po, PyList_New,
					CfReturnRef|CfPyErrIfNull, "l", size);
	if (v == NULL)
		return NULL;

	/* the result is a list */
	set_array_item(po, v, OB_TYPE,
		       vinfo_new(CompileTime_New((long)(&PyList_Type))));
	return v;
}


 /***************************************************************/
  /*** list objects meta-implementation                        ***/

static vinfo_t* plist_item(PsycoObject* po, vinfo_t* a, vinfo_t* i)
{
	condition_code_t cc;
	vinfo_t* vlen;
	vinfo_t* ob_item;
	vinfo_t* result;

	vlen = get_array_item(po, a, VAR_OB_SIZE);
	if (vlen == NULL)
		return NULL;
	
	cc = integer_cmp(po, i, vlen, Py_GE|COMPARE_UNSIGNED);
	if (cc == CC_ERROR)
		return NULL;

	if (runtime_condition_f(po, cc)) {
		PycException_SetString(po, PyExc_IndexError,
				       "list index out of range");
		return NULL;
	}

	if (a->source == VirtualTime_New(&psyco_computed_range)) {
		/* optimize range().__getitem__() */
		/* XXX no support for 'step' right now,
		   so that the return value is simply 'start+i'. */
		vinfo_t* vstart = get_array_item(po, a, RANGE_START);
		if (vstart == NULL)
			return NULL;
		result = integer_add(po, i, vstart, false);
		if (result == NULL)
			return NULL;
		return PsycoInt_FROM_LONG(result);
	}

	ob_item = read_array_item(po, a, LIST_OB_ITEM);
	if (ob_item == NULL)
		return NULL;

	result = read_array_item_var(po, ob_item, 0, i, false);
	vinfo_decref(ob_item, po);
	return result;
}


DEFINEFN
void psy_listobject_init()
{
	PySequenceMethods *m = PyList_Type.tp_as_sequence;
	Psyco_DefineMeta(m->sq_length, psyco_generic_mut_ob_size);
	Psyco_DefineMeta(m->sq_item, plist_item);
}
