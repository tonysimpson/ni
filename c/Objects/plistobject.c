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

	vlen = read_array_item(po, a, VAR_OB_SIZE);
	if (vlen == NULL)
		return NULL;
	
	cc = integer_cmp(po, i, vlen, Py_GE|COMPARE_UNSIGNED);
        vinfo_decref(vlen, po);
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
	/* the list could be freed while the returned item is still in use */
	if (result != NULL)
		need_reference(po, result);
	return result;
}

static bool plist_ass_item(PsycoObject* po, vinfo_t* a, vinfo_t* i, vinfo_t* v)
{
	condition_code_t cc;
	vinfo_t* vlen;
	vinfo_t* ob_item;
	vinfo_t* old_value;
	bool ok;

	if (v == NULL) {
		/* XXX implement item deletion */
		return psyco_generic_call(po, PyList_Type.tp_as_sequence->
					  sq_ass_item,
					  CfNoReturnValue|CfPyErrIfNonNull,
					  "vvl", a, i, (long) NULL) != NULL;
	}

	vlen = read_array_item(po, a, VAR_OB_SIZE);
	if (vlen == NULL)
		return false;
	
	cc = integer_cmp(po, i, vlen, Py_GE|COMPARE_UNSIGNED);
        vinfo_decref(vlen, po);
	if (cc == CC_ERROR)
		return false;

	if (runtime_condition_f(po, cc)) {
		PycException_SetString(po, PyExc_IndexError,
				       "list assignment index out of range");
		return false;
	}

	ob_item = read_array_item(po, a, LIST_OB_ITEM);
	if (ob_item == NULL)
		return false;

	old_value = read_array_item_var(po, ob_item, 0, i, false);
	ok = (old_value != NULL) &&
		write_array_item_var_ref(po, ob_item, 0, i, v, false);

	vinfo_decref(ob_item, po);
	if (ok) psyco_decref_v(po, old_value);
	vinfo_xdecref(old_value, po);

	return ok;
}


INITIALIZATIONFN
void psy_listobject_init(void)
{
	PySequenceMethods *m = PyList_Type.tp_as_sequence;
	Psyco_DefineMeta(m->sq_length, psyco_generic_mut_ob_size);
	Psyco_DefineMeta(m->sq_item, plist_item);
	Psyco_DefineMeta(m->sq_ass_item, plist_ass_item);
}
