#include "pobject.h"
#include "pintobject.h"
#include "pstringobject.h"


DEFINEVAR fixed_switch_t psyfs_int;
DEFINEVAR fixed_switch_t psyfs_tuple_list;


DEFINEFN
condition_code_t PsycoObject_IsTrue(PsycoObject* po, vinfo_t* vi)
{
	PyTypeObject* tp;
	tp = (PyTypeObject*) Psyco_NeedType(po, vi);
	if (tp == NULL)
		return CC_ERROR;

	if (tp == Py_None->ob_type)
		return CC_ALWAYS_FALSE;
	
	else if (tp->tp_as_number != NULL &&
		 tp->tp_as_number->nb_nonzero != NULL)
		return Psyco_flag_META1(po, tp->tp_as_number->nb_nonzero,
					CfReturnFlag, "v", vi);
	
	else if (tp->tp_as_mapping != NULL &&
		 tp->tp_as_mapping->mp_length != NULL)
		return Psyco_flag_META1(po, tp->tp_as_mapping->mp_length,
					CfReturnFlag, "v", vi);
	
	else if (tp->tp_as_sequence != NULL &&
		 tp->tp_as_sequence->sq_length != NULL)
		return Psyco_flag_META1(po, tp->tp_as_sequence->sq_length,
					CfReturnFlag, "v", vi);

	else
		return CC_ALWAYS_TRUE;
}

DEFINEFN
vinfo_t* PsycoObject_Repr(PsycoObject* po, vinfo_t* vi)
{
	/* XXX implement me */
	vinfo_t* vstr = psyco_generic_call(po, PyObject_Repr,
					   CfReturnRef|CfPyErrIfNull,
					   "v", vi);
	if (vstr != NULL) {
		/* the result is a string */
		set_array_item(po, vstr, OB_TYPE,
		       vinfo_new(CompileTime_New((long)(&PyString_Type))));
	}
	return vstr;
}

DEFINEFN
vinfo_t* PsycoObject_GetAttr(PsycoObject* po, vinfo_t* o, vinfo_t* attr_name)
{
	PyTypeObject* tp;
	
	switch (Psyco_TypeSwitch(po, attr_name, &psyfs_string_unicode)) {

	case 0:  /* PyString_Type */
		break;

#ifdef Py_USING_UNICODE
	case 1:  /* PyUnicode_Type */
		goto generic;
#endif
	default:
		if (!PycException_Occurred(po)) {
			PycException_SetString(po, PyExc_TypeError,
					       "attribute name must be string");
		}
		return NULL;
	}

	tp = (PyTypeObject*) Psyco_NeedType(po, o);
	if (tp == NULL)
		return NULL;
	if (tp->tp_getattro != NULL)
		return Psyco_META2(po, tp->tp_getattro,
				   CfReturnRef|CfPyErrIfNull,
				   "vv", o, attr_name);
	if (tp->tp_getattr != NULL)
		return Psyco_META2(po, tp->tp_getattr,
				   CfReturnRef|CfPyErrIfNull,
				   "vv", o,
                                   PsycoString_AS_STRING(po, attr_name));

   generic:
	/* when the above fails */
	return psyco_generic_call(po, PyObject_GetAttr,
				  CfReturnRef|CfPyErrIfNull,
				  "vv", o, attr_name);
}

DEFINEFN
bool PsycoObject_SetAttr(PsycoObject* po, vinfo_t* o,
                         vinfo_t* attr_name, vinfo_t* v)
{
	/* XXX implement me */
	if (v != NULL)
		return psyco_flag_call(po, PyObject_SetAttr,
				       CfReturnFlag|CfPyErrIfNonNull,
				       "vvv", o, attr_name, v) != CC_ERROR;
	else
		return psyco_flag_call(po, PyObject_SetAttr,
				       CfReturnFlag|CfPyErrIfNonNull,
				       "vvl", o, attr_name, NULL) != CC_ERROR;
}


/* Macro to get the tp_richcompare field of a type if defined */
#define RICHCOMPARE(t) (PyType_HasFeature((t), Py_TPFLAGS_HAVE_RICHCOMPARE) \
                         ? (t)->tp_richcompare : NULL)

/* Map rich comparison operators to their swapped version, e.g. LT --> GT */
static int swapped_op[] = {Py_GT, Py_GE, Py_EQ, Py_NE, Py_LT, Py_LE};

static vinfo_t* try_rich_compare(PsycoObject* po, vinfo_t* v, vinfo_t* w, int op)
{
	bool swap;
	PyTypeObject* vtp = Psyco_FastType(v);
	PyTypeObject* wtp = Psyco_FastType(w);
	richcmpfunc fv = RICHCOMPARE(vtp);
	richcmpfunc fw = RICHCOMPARE(wtp);
	vinfo_t* res;

	swap = (vtp != wtp &&
		PyType_IsSubtype(wtp, vtp) &&
		fw != NULL);
	if (swap) {
		res = Psyco_META3(po, fw, CfReturnRef|CfPyErrNotImplemented,
				  "vvl", w, v, swapped_op[op]);
		if (res != psyco_viNotImplemented)
			return res;
		vinfo_decref(res, po);
	}
	if (fv != NULL) {
		res = Psyco_META3(po, fv, CfReturnRef|CfPyErrNotImplemented,
				  "vvl", v, w, op);
		if (res != psyco_viNotImplemented)
			return res;
		vinfo_decref(res, po);
	}
	if (!swap && fw != NULL) {
		return Psyco_META3(po, fw, CfReturnRef|CfPyErrNotImplemented,
				   "vvl", w, v, swapped_op[op]);
	}
	res = psyco_viNotImplemented;
	vinfo_incref(res);
	return res;
}

inline vinfo_t* convert_3way_to_object(PsycoObject* po, int op, vinfo_t* c)
{
	condition_code_t cc = integer_cmp_i(po, c, 0, op);
	if (cc == CC_ERROR)
		return NULL;
	return PsycoInt_FROM_LONG(psyco_vinfo_condition(po, cc));
}

inline vinfo_t* try_3way_to_rich_compare(PsycoObject* po, vinfo_t* v,
					 vinfo_t* w, int op)
{
	/* XXX implement me (some day) */
	return psyco_generic_call(po, PyObject_RichCompare,
				  CfReturnRef|CfPyErrIfNull,
				  "vvl", v, w, (long) op);
}

DEFINEFN vinfo_t* PsycoObject_RichCompare(PsycoObject* po, vinfo_t* v,
					  vinfo_t* w, int op)
{
	PyTypeObject* vtp;
	PyTypeObject* wtp;
	vinfo_t* res;
	cmpfunc f;
	extra_assert(Py_LT <= op && op <= Py_GE);
	
	/* XXX try to detect circular data structures */

	vtp = (PyTypeObject*) Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	wtp = (PyTypeObject*) Psyco_NeedType(po, w);
	if (wtp == NULL)
		return NULL;

	/* If the types are equal, don't bother with coercions etc. 
	   Instances are special-cased in try_3way_compare, since
	   a result of 2 does *not* mean one value being greater
	   than the other. */
	if (vtp == wtp
	    && (f = vtp->tp_compare) != NULL
	    && !PyType_TypeCheck(vtp, &PyInstance_Type)) {
		vinfo_t* c;
		richcmpfunc f1;
		if (vtp == &PyInt_Type) {
			/* Special-case integers because they don't use
			   rich comparison, and the plain 3-way comparisons
			   are too low-level for serious optimizations
			   (they would require building the value -1, 0 or
			   1 in a register and then testing it). */
			return PsycoIntInt_RichCompare(po, v, w, op);
		}
		if ((f1 = RICHCOMPARE(vtp)) != NULL) {
			/* If the type has richcmp, try it first.
			   try_rich_compare would try it two-sided,
			   which is not needed since we've a single
			   type only. */
			res = Psyco_META3(po, f1,
					  CfReturnRef|CfPyErrNotImplemented,
					  "vvl", v, w, (long) op);
			if (res != psyco_viNotImplemented)
				return res;
			vinfo_decref(res, po);
		}
		c = Psyco_META2(po, f, CfReturnNormal|CfPyErrCheck,
				"vv", v, w);
		if (c == NULL)
			return NULL;
		return convert_3way_to_object(po, op, c);
	}

	res = try_rich_compare(po, v, w, op);
	if (res != psyco_viNotImplemented)
		return res;
	vinfo_decref(res, po);

	return try_3way_to_rich_compare(po, v, w, op);
}

DEFINEFN
condition_code_t PsycoObject_RichCompareBool(PsycoObject* po,
                                             vinfo_t* v,
                                             vinfo_t* w, int op)
{
	condition_code_t cc;
	vinfo_t* result = PsycoObject_RichCompare(po, v, w, op);
	if (result == NULL)
		return CC_ERROR;
        cc = PsycoObject_IsTrue(po, result);
        vinfo_decref(result, po);
	return cc;
}
