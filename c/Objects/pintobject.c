#include "pintobject.h"


DEFINEFN
vinfo_t* PsycoInt_AsLong(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* result;
	PyNumberMethods *nb;
	PyTypeObject* tp;
	
	tp = Psyco_NeedType(po, v);
	if (tp == NULL)
		return NULL;

	if (PsycoInt_Check(tp)) {
		result = PsycoInt_AS_LONG(po, v);
		vinfo_incref(result);
		return result;
	}

	if ((nb = tp->tp_as_number) == NULL || nb->nb_int == NULL) {
		PycException_SetString(po, PyExc_TypeError,
				       "an integer is required");
		return NULL;
	}

	v = Psyco_META1(po, nb->nb_int,
			CfReturnRef|CfPyErrIfNull,
			"v", v);
	if (v == NULL)
		return NULL;
	
	/* silently assumes the result is an integer object */
	result = PsycoInt_AS_LONG(po, v);
	vinfo_incref(result);
	vinfo_decref(v, po);
	return result;
}

static bool compute_int(PsycoObject* po, vinfo_t* intobj, bool forking)
{
	vinfo_t* newobj;
	vinfo_t* x;
	if (forking) return true;
	
	/* get the field 'ob_ival' from the Python object 'intobj' */
	x = vinfo_getitem(intobj, iINT_OB_IVAL);
	if (x == NULL)
		return false;

	/* call PyInt_FromLong() */
	newobj = psyco_generic_call(po, PyInt_FromLong,
				    CfPure|CfReturnRef|CfPyErrIfNull, "v", x);
	if (newobj == NULL)
		return false;

	/* move the resulting non-virtual Python object back into 'intobj' */
	vinfo_move(po, intobj, newobj);
	return true;
}


DEFINEVAR source_virtual_t psyco_computed_int;


 /***************************************************************/
  /*** integer objects meta-implementation                     ***/

static vinfo_t* pint_nonzero(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	condition_code_t cc = integer_non_null(po, ival);
	if (cc == CC_ERROR)
		return NULL;
	return psyco_vinfo_condition(po, cc);
}

static vinfo_t* pint_pos(PsycoObject* po, vinfo_t* intobj)
{
	if (Psyco_KnownType(intobj) == &PyInt_Type) {
		vinfo_incref(intobj);
		return intobj;
	}
	else {
		vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
		if (ival == NULL)
			return NULL;
		return PsycoInt_FromLong(ival);
	}
}

static vinfo_t* pint_neg(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* result;
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	if (ival == NULL)
		return NULL;
	result = integer_neg(po, ival, true);
	if (result != NULL)
		return PsycoInt_FROM_LONG(result);
	
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_negative,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "v", intobj);
}

static vinfo_t* pint_invert(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* result;
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	if (ival == NULL)
		return NULL;
	result = integer_not(po, ival);
	if (result != NULL)
		result = PsycoInt_FROM_LONG(result);
	return result;
}

static vinfo_t* pint_abs(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* result;
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	if (ival == NULL)
		return NULL;
	result = integer_abs(po, ival, true);
	if (result != NULL)
		return PsycoInt_FROM_LONG(result);
	
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_absolute,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "v", intobj);
}


#define CONVERT_TO_LONG(vobj, vlng)				\
	switch (Psyco_VerifyType(po, vobj, &PyInt_Type)) {	\
	case true:   /* vobj is a PyIntObject */		\
		vlng = PsycoInt_AS_LONG(po, vobj);		\
		if (vlng == NULL)				\
			return NULL;				\
		break;						\
	case false:  /* vobj is not a PyIntObject */		\
		return psyco_vi_NotImplemented();		\
	default:     /* error or promotion */			\
		return NULL;					\
	}

static vinfo_t* pint_add(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_add(po, a, b, true);
	if (x != NULL)
		return PsycoInt_FROM_LONG(x);
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_add,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "vv", v, w);
}

static vinfo_t* pint_sub(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_sub(po, a, b, true);
	if (x != NULL)
		return PsycoInt_FROM_LONG(x);
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_subtract,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "vv", v, w);
}

static vinfo_t* pint_base2op(PsycoObject* po, vinfo_t* v, vinfo_t* w,
			     vinfo_t*(*op)(PsycoObject*,vinfo_t*,vinfo_t*))
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = op (po, a, b);
	if (x != NULL)
		x = PsycoInt_FROM_LONG(x);
	return x;
}

static vinfo_t* pint_or(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	return pint_base2op(po, v, w, integer_or);
}

static vinfo_t* pint_xor(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	return pint_base2op(po, v, w, integer_xor);
}

static vinfo_t* pint_and(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	return pint_base2op(po, v, w, integer_and);
}

static vinfo_t* pint_mul(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	PyTypeObject* tp;

	tp = Psyco_NeedType(po, v);
	if (tp == NULL)
		return NULL;

	if (!PsycoInt_Check(tp) &&
	    tp->tp_as_sequence &&
	    tp->tp_as_sequence->sq_repeat) {
		/* sequence * int */
		a = PsycoInt_AsLong(po, w);
		if (a == NULL)
			return NULL;
		x = Psyco_META2(po, tp->tp_as_sequence->sq_repeat,
				CfReturnRef|CfPyErrIfNull,
				"vv", v, a);
		vinfo_decref(a, po);
		return x;
	}
	
	tp = Psyco_NeedType(po, w);
	if (tp == NULL)
		return NULL;

	if (!PsycoInt_Check(tp) &&
	    tp->tp_as_sequence &&
	    tp->tp_as_sequence->sq_repeat) {
		/* int * sequence */
		a = PsycoInt_AsLong(po, v);
		if (a == NULL)
			return NULL;
		x = Psyco_META2(po, tp->tp_as_sequence->sq_repeat,
				CfReturnRef|CfPyErrIfNull,
				"vv", w, a);
		vinfo_decref(a, po);
		return x;
	}
	
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_mul(po, a, b, true);
	if (x != NULL)
		return PsycoInt_FROM_LONG(x);
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_multiply,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "vv", v, w);
}

#if PY_VERSION_HEX>=0x02040000
# warning "left shifts must be able to return longs in Python 2.4"
#endif
static vinfo_t* pint_lshift(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	return pint_base2op(po, v, w, integer_lshift);
}

static vinfo_t* pint_rshift(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	return pint_base2op(po, v, w, integer_rshift);
}




/* Careful, most operations might return a long if they overflow.
   Only list here the ones that cannot. Besides, all these operations
   should sooner or later be implemented in Psyco. XXX */
/*DEF_KNOWN_RET_TYPE_2(pint_lshift, PyInt_Type.tp_as_number->nb_lshift,
  CfReturnRef|CfPyErrNotImplemented, &PyInt_Type)
  DEF_KNOWN_RET_TYPE_2(pint_rshift, PyInt_Type.tp_as_number->nb_rshift,
  CfReturnRef|CfPyErrNotImplemented, &PyInt_Type)*/

INITIALIZATIONFN
void psy_intobject_init(void)
{
	PyNumberMethods *m = PyInt_Type.tp_as_number;
	Psyco_DefineMeta(m->nb_nonzero,  pint_nonzero);
	
	Psyco_DefineMeta(m->nb_positive, pint_pos);
	Psyco_DefineMeta(m->nb_negative, pint_neg);
	Psyco_DefineMeta(m->nb_invert,   pint_invert);
	Psyco_DefineMeta(m->nb_absolute, pint_abs);
	
	Psyco_DefineMeta(m->nb_add,      pint_add);
	Psyco_DefineMeta(m->nb_subtract, pint_sub);
	Psyco_DefineMeta(m->nb_or,       pint_or);
	Psyco_DefineMeta(m->nb_xor,      pint_xor);
	Psyco_DefineMeta(m->nb_and,      pint_and);
	Psyco_DefineMeta(m->nb_multiply, pint_mul);
	Psyco_DefineMeta(m->nb_lshift,   pint_lshift);
	Psyco_DefineMeta(m->nb_rshift,   pint_rshift);
	
	psyco_computed_int.compute_fn = &compute_int;
}
