#include "pabstract.h"
#include "pintobject.h"
#include "plongobject.h"
#include "pstringobject.h"
#include "piterobject.h"
#include "ptupleobject.h"
#include "pmethodobject.h"
#include "pclassobject.h"
#include "pfuncobject.h"


/*** This file is translated from the original 'abstract.c', see comments
     in the original file for the general ideas about the algorithms. ***/

/* Shorthand to return certain errors */

static vinfo_t* type_error(PsycoObject* po, const char *msg)
{
	PycException_SetString(po, PyExc_TypeError, msg);
	return NULL;
}


DEFINEFN
vinfo_t* PsycoObject_Call(PsycoObject* po, vinfo_t* callable_object,
                          vinfo_t* args, vinfo_t* kw)
{	/* 'kw' may not be NULL */
	ternaryfunc call;
	PyTypeObject* tp = Psyco_NeedType(po, callable_object);
	if (tp == NULL)
		return NULL;

	if ((call = tp->tp_call) != NULL) {
		return Psyco_META3(po, call, CfReturnRef|CfPyErrIfNull,
				   "vvv", callable_object, args, kw);
	}

#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
	PycException_SetFormat(po, PyExc_TypeError,
			       "object of type '%.100s' is not callable",
			       tp->tp_name);
	return NULL;

#else /* !NEW_STYLE_TYPES */
	/* In older Python versions, the common callable types have no
           tp_call slot! See these versions' call_object() function in
           ceval.c. */

	if (tp == &PyMethod_Type)
		return pinstancemethod_call(po, callable_object, args, kw);
	if (tp == &PyFunction_Type)
		return pfunction_call(po, callable_object, args, kw);
	if (tp == &PyCFunction_Type)
		return PsycoCFunction_Call(po, callable_object, args, kw);
	
	return psyco_generic_call(po, PyEval_CallObjectWithKeywords,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", callable_object, args, kw);
#endif /* NEW_STYLE_TYPES */
}

DEFINEFN
vinfo_t* PsycoEval_CallObjectWithKeywords(PsycoObject* po,
					  vinfo_t* callable_object,
					  vinfo_t* args, vinfo_t* kw)
{
	vinfo_t* result;
	
	if (args == NULL)
		args = PsycoTuple_New(0, NULL);
	else {
		switch (Psyco_VerifyType(po, args, &PyTuple_Type)) {
		case true:  /* args is a tuple */
			vinfo_incref(args);
			break;
		case false:  /* args is not a tuple */
			goto use_proxy;
		default:     /* error or promotion */
			return NULL;
		}
	}
	if (kw == NULL)
		kw = psyco_vi_Zero();
	else {
		switch (Psyco_VerifyType(po, kw, &PyDict_Type)) {
		case true:   /* kw is a dict */
			vinfo_incref(kw);
			break;
		case false:  /* kw is not a dict */
			vinfo_decref(args, po);
			goto use_proxy;
		default:     /* error or promotion */
			return NULL;
		}
	}

	result = PsycoObject_Call(po, callable_object, args, kw);
	vinfo_decref(kw, po);
	vinfo_decref(args, po);
	return result;

   use_proxy:
	return psyco_generic_call(po, PyEval_CallObjectWithKeywords,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", callable_object, args, kw);
}


DEFINEFN
vinfo_t* PsycoObject_GetItem(PsycoObject* po, vinfo_t* o, vinfo_t* key)
{
	PyMappingMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, o);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_mapping;
	if (m && m->mp_subscript)
		return Psyco_META2(po, m->mp_subscript,
				   CfReturnRef|CfPyErrIfNull, "vv", o, key);

	if (tp->tp_as_sequence) {
		/* TypeSwitch */
		PyTypeObject* ktp = Psyco_NeedType(po, key);
		if (ktp == NULL)
			return NULL;

		if (PyType_TypeCheck(ktp, &PyInt_Type)) {
			return PsycoSequence_GetItem(po, o,
						     PsycoInt_AS_LONG(po, key));
		}
		if (PyType_TypeCheck(ktp, &PyLong_Type)) {
			vinfo_t* key_value = PsycoLong_AsLong(po, key);
			if (key_value == NULL)
				return NULL;
			return PsycoSequence_GetItem(po, o, key_value);
		}
		type_error(po, "sequence index must be integer");
		return false;
	}

	type_error(po, "unsubscriptable object");
	return false;
}

DEFINEFN
bool PsycoObject_SetItem(PsycoObject* po, vinfo_t* o, vinfo_t* key,
			 vinfo_t* value)
{
	PyMappingMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, o);
	if (tp == NULL)
		return false;

	m = tp->tp_as_mapping;
	if (m && m->mp_ass_subscript) {
		char* vargs = (value!=NULL) ? "vvv" : "vvl";
		return Psyco_META3(po, m->mp_ass_subscript,
				   CfNoReturnValue|CfPyErrIfNonNull,
				   vargs, o, key, value) != NULL;
	}

	if (tp->tp_as_sequence) {
		/* TypeSwitch */
		PyTypeObject* ktp = Psyco_NeedType(po, key);
		if (ktp == NULL)
			return false;

		if (PyType_TypeCheck(ktp, &PyInt_Type)) {
			return PsycoSequence_SetItem(po, o,
						     PsycoInt_AS_LONG(po, key),
						     value);
		}
		if (PyType_TypeCheck(ktp, &PyLong_Type)) {
			vinfo_t* key_value = PsycoLong_AsLong(po, key);
			if (key_value == NULL)
				return false;
			return PsycoSequence_SetItem(po, o, key_value, value);
		}
		if (tp->tp_as_sequence->sq_ass_item) {
			type_error(po, "sequence index must be integer");
			return false;
		}
	}

	type_error(po, (value!=NULL) ?
		   "object does not support item assignment" :
		   "object does not support item deletion");
	return false;
}

DEFINEFN
vinfo_t* PsycoObject_Size(PsycoObject* po, vinfo_t* vi)
{
	PySequenceMethods *m;
	void* f;
	PyTypeObject* tp = Psyco_NeedType(po, vi);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_sequence;
	if (m && m->sq_length)
		f = m->sq_length;
	else {
		PyMappingMethods *m2 = tp->tp_as_mapping;
		if (m2 && m2->mp_length)
			f = m2->mp_length;
		else
			return type_error(po, "len() of unsized object");
	}
	
	return Psyco_META1(po, f, CfReturnNormal|CfPyErrIfNeg, "v", vi);
}

DEFINEFN
vinfo_t* psyco_generic_immut_ob_size(PsycoObject* po, vinfo_t* vi)
{
	vinfo_t* result = get_array_item(po, vi, VAR_OB_SIZE);
	if (result != NULL)
		vinfo_incref(result);
	return result;
}

DEFINEFN
vinfo_t* psyco_generic_mut_ob_size(PsycoObject* po, vinfo_t* vi)
{
	return read_array_item(po, vi, VAR_OB_SIZE);
}

DEFINEFN
vinfo_t* PsycoSequence_GetItem(PsycoObject* po, vinfo_t* o, vinfo_t* i)
{
	PySequenceMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, o);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_sequence;
	if (m && m->sq_item) {
		vinfo_t* result;
		vinfo_t* i2 = i;
		if (m->sq_length) {
			condition_code_t cc = integer_cmp_i(po, i, 0, Py_LT);
			if (cc == CC_ERROR)
				return NULL;
			if (runtime_condition_f(po, cc)) {
				vinfo_t* l = Psyco_META1(po, m->sq_length,
						CfReturnNormal|CfPyErrIfNeg,
							 "v", o);
				if (l == NULL)
					return NULL;
				i2 = integer_add(po, i, l, false);
				vinfo_decref(l, po);
				if (i2 == NULL)
					return NULL;
                                i = NULL;
			}
		}
		result = Psyco_META2(po, m->sq_item, CfReturnRef|CfPyErrIfNull,
				     "vv", o, i2);
		if (i == NULL)
			vinfo_decref(i2, po);
		return result;
	}

	return type_error(po, "unindexable object");
}

DEFINEFN
bool PsycoSequence_SetItem(PsycoObject* po, vinfo_t* o, vinfo_t* i,
			   vinfo_t* value)
{
	PySequenceMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, o);
	if (tp == NULL)
		return false;

	m = tp->tp_as_sequence;
	if (m && m->sq_ass_item) {
		bool result;
		char* vargs;
		vinfo_t* i1 = NULL;
		if (m->sq_length) {
			condition_code_t cc = integer_cmp_i(po, i, 0, Py_LT);
			if (cc == CC_ERROR)
				return false;
			if (runtime_condition_f(po, cc)) {
				vinfo_t* l = Psyco_META1(po, m->sq_length,
						CfReturnNormal|CfPyErrIfNeg,
						"v", o);
				if (l == NULL)
					return false;
				i = i1 = integer_add(po, i, l, false);
				vinfo_decref(l, po);
				if (i1 == NULL)
					return false;
			}
		}
		vargs = (value!=NULL) ? "vvv" : "vvl";
		result = Psyco_META3(po, m->sq_ass_item,
				     CfNoReturnValue|CfPyErrIfNonNull,
				     vargs, o, i, value) != NULL;
		vinfo_xdecref(i1, po);
		return result;
	}

	type_error(po, (value!=NULL) ?
		   "object doesn't support item assignment" :
		   "object doesn't support item deletion");
	return false;
}

DEFINEFN
vinfo_t* PsycoSequence_GetSlice(PsycoObject* po, vinfo_t* o,
				vinfo_t* ilow, vinfo_t* ihigh)
{
	/* XXX implement me */
	return psyco_generic_call(po, PySequence_GetSlice,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", o, ilow, ihigh);
}

DEFINEFN
bool PsycoSequence_SetSlice(PsycoObject* po, vinfo_t* o,
			    vinfo_t* ilow, vinfo_t* ihigh, vinfo_t* value)
{
	/* XXX implement me */
	if (value != NULL)
		return psyco_generic_call(po, PySequence_SetSlice,
                                          CfNoReturnValue|CfPyErrIfNonNull,
                                          "vvvv", o, ilow, ihigh, value)!=NULL;
	else
		return psyco_generic_call(po, PySequence_DelSlice,
                                          CfNoReturnValue|CfPyErrIfNonNull,
                                          "vvv", o, ilow, ihigh) != NULL;
}

DEFINEFN
vinfo_t* PsycoSequence_Contains(PsycoObject* po, vinfo_t* seq, vinfo_t* ob)
{
	PyTypeObject* tp = Psyco_NeedType(po, seq);
	if (tp == NULL)
		return false;

	if (PyType_HasFeature(tp, Py_TPFLAGS_HAVE_SEQUENCE_IN)) {
		PySequenceMethods *sqm = tp->tp_as_sequence;
	        if (sqm != NULL && sqm->sq_contains != NULL)
			return Psyco_META2(po, sqm->sq_contains,
					   CfReturnNormal|CfPyErrIfNeg,
					   "vv", seq, ob);
	}

	/* XXX implement me */
#if HAVE_GENERATORS
	return psyco_generic_call(po, _PySequence_IterSearch,
				  CfReturnNormal|CfPyErrIfNeg,
				  "vvl", seq, ob, PY_ITERSEARCH_CONTAINS);
#else
	return psyco_generic_call(po, PySequence_Contains,
				  CfReturnNormal|CfPyErrIfNeg,
				  "vv", seq, ob);
#endif
}

DEFINEFN
vinfo_t* PsycoSequence_Tuple(PsycoObject* po, vinfo_t* seq)
{
	/* XXX implement me */
	vinfo_t* v = psyco_generic_call(po, PySequence_Tuple,
					CfReturnRef|CfPyErrIfNull,
					"v", seq);
	if (v == NULL)
		return NULL;

	/* the result is a tuple */
	set_array_item(po, v, OB_TYPE,
		       vinfo_new(CompileTime_New((long)(&PyTuple_Type))));
	return v;
}


DEFINEFN
vinfo_t* PsycoNumber_Positive(PsycoObject* po, vinfo_t* vi)
{
	PyNumberMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, vi);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_number;
	if (m && m->nb_positive)
		return Psyco_META1(po, m->nb_positive,
				   CfReturnRef|CfPyErrIfNull, "v", vi);

	return type_error(po, "bad operand type for unary +");
}

DEFINEFN
vinfo_t* PsycoNumber_Negative(PsycoObject* po, vinfo_t* vi)
{
	PyNumberMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, vi);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_number;
	if (m && m->nb_negative)
		return Psyco_META1(po, m->nb_negative,
				   CfReturnRef|CfPyErrIfNull, "v", vi);

	return type_error(po, "bad operand type for unary -");
}

DEFINEFN
vinfo_t* PsycoNumber_Invert(PsycoObject* po, vinfo_t* vi)
{
	PyNumberMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, vi);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_number;
	if (m && m->nb_invert)
		return Psyco_META1(po, m->nb_invert,
				   CfReturnRef|CfPyErrIfNull, "v", vi);

	return type_error(po, "bad operand type for unary ~");
}

DEFINEFN
vinfo_t* PsycoNumber_Absolute(PsycoObject* po, vinfo_t* vi)
{
	PyNumberMethods *m;
	PyTypeObject* tp = Psyco_NeedType(po, vi);
	if (tp == NULL)
		return NULL;

	m = tp->tp_as_number;
	if (m && m->nb_absolute)
		return Psyco_META1(po, m->nb_absolute,
				   CfReturnRef|CfPyErrIfNull, "v", vi);

	return type_error(po, "bad operand type for abs()");
}

#ifdef Py_TPFLAGS_CHECKTYPES
# define NEW_STYLE_NUMBER(otp) PyType_HasFeature((otp), \
				Py_TPFLAGS_CHECKTYPES)
#else
# define NEW_STYLE_NUMBER(otp)     0
#endif


#define NB_SLOT(x) offsetof(PyNumberMethods, x)
#define NB_BINOP(nb_methods, slot) \
		((binaryfunc*)(& ((char*)nb_methods)[slot] ))
#define NB_TERNOP(nb_methods, slot) \
		((ternaryfunc*)(& ((char*)nb_methods)[slot] ))

#define IS_IMPLEMENTED(x)   \
  ((x) == NULL || ((x)->source != CompileTime_NewSk(&psyco_skNotImplemented)))


/* the 'cimpl_xxx()' functions are called at run-time, to do things
   we give up to write at the meta-level in the PsycoXxx() functions. */
static PyObject* cimpl_oldstyle_binary_op1(PyObject* v, PyObject* w,
					   const int op_slot)
{
	int err = PyNumber_CoerceEx(&v, &w);
	if (err < 0) {
		return NULL;
	}
	if (err == 0) {
		PyNumberMethods *mv = v->ob_type->tp_as_number;
		if (mv) {
			binaryfunc slot;
			slot = *NB_BINOP(mv, op_slot);
			if (slot) {
				PyObject *x = slot(v, w);
				Py_DECREF(v);
				Py_DECREF(w);
				return x;
			}
		}
		/* CoerceEx incremented the reference counts */
		Py_DECREF(v);
		Py_DECREF(w);
	}
	Py_INCREF(Py_NotImplemented);
	return Py_NotImplemented;
}

static vinfo_t* binary_op1(PsycoObject* po, vinfo_t* v, vinfo_t* w,
			   const int op_slot)
{
	vinfo_t* x;
	binaryfunc slotv = NULL;
	binaryfunc slotw = NULL;
	
	PyTypeObject* vtp;
	PyTypeObject* wtp;
	vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	wtp = Psyco_NeedType(po, w);
	if (wtp == NULL)
		return NULL;

	if (vtp->tp_as_number != NULL && NEW_STYLE_NUMBER(vtp))
		slotv = *NB_BINOP(vtp->tp_as_number, op_slot);
	if (wtp != vtp &&
	    wtp->tp_as_number != NULL && NEW_STYLE_NUMBER(wtp)) {
		slotw = *NB_BINOP(wtp->tp_as_number, op_slot);
		if (slotw == slotv)
			slotw = NULL;
	}
	if (slotv) {
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
		if (slotw && PyType_IsSubtype(wtp, vtp)) {
			x = Psyco_META2(po, slotw,
					CfReturnRef|CfPyErrNotImplemented,
					"vv", v, w);
			if (IS_IMPLEMENTED(x))
				return x;  /* may be NULL */
			vinfo_decref(x, po); /* can't do it */
			slotw = NULL;
		}
#endif
		x = Psyco_META2(po, slotv,
				CfReturnRef|CfPyErrNotImplemented, "vv", v, w);
		if (IS_IMPLEMENTED(x))
			return x;
		vinfo_decref(x, po); /* can't do it */
	}
	if (slotw) {
		x = Psyco_META2(po, slotw, CfReturnRef|CfPyErrNotImplemented,
				"vv", v, w);
		if (IS_IMPLEMENTED(x))
			return x;
		vinfo_decref(x, po); /* can't do it */
	}
	if (!NEW_STYLE_NUMBER(vtp) || !NEW_STYLE_NUMBER(wtp)) {
		/* no optimization for the part specific to old-style numbers */
		return psyco_generic_call(po, cimpl_oldstyle_binary_op1,
                                          CfReturnRef|CfPyErrNotImplemented,
					  "vvl", v, w, op_slot);
	}
	return psyco_vi_NotImplemented();
}

static vinfo_t* binary_op(PsycoObject* po, vinfo_t* v, vinfo_t* w,
			  const int op_slot, const char *op_name)
{
	vinfo_t* result = binary_op1(po, v, w, op_slot);
	if (!IS_IMPLEMENTED(result)) {
		vinfo_decref(result, po);
		PycException_SetFormat(po, PyExc_TypeError,
			"unsupported operand type(s) for %s: '%s' and '%s'",
				       op_name,
				       Psyco_FastType(v)->tp_name,
				       Psyco_FastType(w)->tp_name);
		return NULL;
	}
	return result;
}

#define BINARY_FUNC(func, op, op_name)					\
DEFINEFN vinfo_t* func(PsycoObject* po, vinfo_t* v, vinfo_t* w) {	\
	return binary_op(po, v, w, NB_SLOT(op), op_name);		\
}

BINARY_FUNC(PsycoNumber_Or, nb_or, "|")
BINARY_FUNC(PsycoNumber_Xor, nb_xor, "^")
BINARY_FUNC(PsycoNumber_And, nb_and, "&")
BINARY_FUNC(PsycoNumber_Lshift, nb_lshift, "<<")
BINARY_FUNC(PsycoNumber_Rshift, nb_rshift, ">>")
BINARY_FUNC(PsycoNumber_Subtract, nb_subtract, "-")
BINARY_FUNC(PsycoNumber_Multiply, nb_multiply, "*")
BINARY_FUNC(PsycoNumber_Divide, nb_divide, "/")
BINARY_FUNC(PsycoNumber_Divmod, nb_divmod, "divmod()")

#ifdef BINARY_FLOOR_DIVIDE
     /* XXX tp_flags test -- not done in Python either, check future releases */
BINARY_FUNC(PsycoNumber_FloorDivide, nb_floor_divide, "//")
BINARY_FUNC(PsycoNumber_TrueDivide, nb_true_divide, "/")
#endif


DEFINEFN
vinfo_t* PsycoNumber_Add(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* result = binary_op1(po, v, w, NB_SLOT(nb_add));
	if (!IS_IMPLEMENTED(result)) {
		PySequenceMethods* m;
		vinfo_decref(result, po);
		m = Psyco_FastType(v)->tp_as_sequence;
		if (m && m->sq_concat) {
			result = Psyco_META2(po, m->sq_concat,
					     CfReturnRef|CfPyErrIfNull,
					     "vv", v, w);
		}
                else {
			PycException_SetFormat(po, PyExc_TypeError,
			    "unsupported operand types for +: '%s' and '%s'",
					       Psyco_FastType(v)->tp_name,
					       Psyco_FastType(w)->tp_name);
			result = NULL;
                }
	}
	return result;
}

DEFINEFN
vinfo_t* PsycoNumber_Remainder(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	if (PsycoString_Check(vtp))
		return psyco_generic_call(po, PyString_Format,
                                          CfReturnRef|CfPyErrIfNull,
					  "vv", v, w);
#ifdef Py_USING_UNICODE
	else if (PsycoUnicode_Check(vtp))
		return psyco_generic_call(po, PyUnicode_Format,
                                          CfReturnRef|CfPyErrIfNull,
					  "vv", v, w);
#endif
	return binary_op(po, v, w, NB_SLOT(nb_remainder), "%");
}

DEFINEFN
vinfo_t* PsycoNumber_Power(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, vinfo_t*v3)
{
	/* XXX implement the ternary operators */
	return psyco_generic_call(po, PyNumber_Power, CfReturnRef|CfPyErrIfNull,
				  "vvv", v1, v2, v3);
}


#define HASINPLACE(tp) PyType_HasFeature((tp), Py_TPFLAGS_HAVE_INPLACEOPS)

static vinfo_t* binary_iop(PsycoObject* po, vinfo_t* v, vinfo_t* w,
			   const int iop_slot, const int op_slot,
			   const char *op_name)
{
	PyNumberMethods *mv;
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	
	mv = vtp->tp_as_number;
	if (mv != NULL && HASINPLACE(vtp)) {
		binaryfunc slot = *NB_BINOP(mv, iop_slot);
		if (slot) {
			vinfo_t* x = Psyco_META2(po, slot,
					CfReturnRef|CfPyErrNotImplemented,
					"vv", v, w);
			if (IS_IMPLEMENTED(x))
				return x;
			vinfo_decref(x, po);
		}
	}
	return binary_op(po, v, w, op_slot, op_name);
}


#define INPLACE_BINOP(func, iop, op, op_name)					\
DEFINEFN vinfo_t* func(PsycoObject* po, vinfo_t* v, vinfo_t* w) {		\
	return binary_iop(po, v, w, NB_SLOT(iop), NB_SLOT(op), op_name);	\
}

INPLACE_BINOP(PsycoNumber_InPlaceOr, nb_inplace_or, nb_or, "|=")
INPLACE_BINOP(PsycoNumber_InPlaceXor, nb_inplace_xor, nb_xor, "^=")
INPLACE_BINOP(PsycoNumber_InPlaceAnd, nb_inplace_and, nb_and, "&=")
INPLACE_BINOP(PsycoNumber_InPlaceLshift, nb_inplace_lshift, nb_lshift, "<<=")
INPLACE_BINOP(PsycoNumber_InPlaceRshift, nb_inplace_rshift, nb_rshift, ">>=")
INPLACE_BINOP(PsycoNumber_InPlaceSubtract, nb_inplace_subtract, nb_subtract,"-=")
INPLACE_BINOP(PsycoNumber_InPlaceDivide, nb_inplace_divide, nb_divide, "/=")

#ifdef BINARY_FLOOR_DIVIDE
     /* XXX tp_flags test -- not done in Python either, check future releases */
INPLACE_BINOP(PsycoNumber_InPlaceFloorDivide, nb_inplace_floor_divide,
              nb_floor_divide, "//=")
INPLACE_BINOP(PsycoNumber_InPlaceTrueDivide, nb_inplace_true_divide,
              nb_true_divide, "/=")
#endif

DEFINEFN
vinfo_t* PsycoNumber_InPlaceAdd(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	if (vtp->tp_as_sequence != NULL) {
		binaryfunc f = NULL;
		if (HASINPLACE(vtp))
			f = vtp->tp_as_sequence->sq_inplace_concat;
		if (f == NULL)
			f = vtp->tp_as_sequence->sq_concat;
		if (f != NULL)
			return Psyco_META2(po, f, CfReturnRef|CfPyErrIfNull,
					   "vv", v, w);
	}
	return binary_iop(po, v, w, NB_SLOT(nb_inplace_add),
			  NB_SLOT(nb_add), "+=");
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceMultiply(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	intargfunc g;
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	
	if (HASINPLACE(vtp) && vtp->tp_as_sequence != NULL &&
	    (g = vtp->tp_as_sequence->sq_inplace_repeat)) {
		vinfo_t* result;
		vinfo_t* n;
		/* TypeSwitch */
		PyTypeObject* wtp = Psyco_NeedType(po, w);
		if (wtp == NULL)
			return NULL;

		if (PyType_TypeCheck(wtp, &PyInt_Type)) {
			n = PsycoInt_AsLong(po, w);
		}
		else if (PyType_TypeCheck(wtp, &PyLong_Type)) {
			n = PsycoLong_AsLong(po, w);
		}
		else {
			type_error(po, "can't multiply sequence to non-int");
			return NULL;
		}
		if (n == NULL)
			return NULL;
		result = Psyco_META2(po, g, CfReturnRef|CfPyErrIfNull,
				     "vv", v, n);
		vinfo_decref(n, po);
		return result;
	}
	return binary_iop(po, v, w, NB_SLOT(nb_inplace_multiply),
			  NB_SLOT(nb_multiply), "*=");
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceRemainder(PsycoObject* po, vinfo_t* v ,vinfo_t* w)
{
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;
	if (PsycoString_Check(vtp))
		return psyco_generic_call(po, PyString_Format,
                                          CfReturnRef|CfPyErrIfNull,
					  "vv", v, w);
#ifdef Py_USING_UNICODE
	else if (PsycoUnicode_Check(vtp))
		return psyco_generic_call(po, PyUnicode_Format,
                                          CfReturnRef|CfPyErrIfNull,
					  "vv", v, w);
#endif
	return binary_iop(po, v, w, NB_SLOT(nb_inplace_remainder),
			  NB_SLOT(nb_remainder), "%");
}

DEFINEFN
vinfo_t* PsycoNumber_InPlacePower(PsycoObject* po, vinfo_t* v1, vinfo_t* v2,
				  vinfo_t* v3) {
	/* XXX implement the ternary operators */
	return psyco_generic_call(po, PyNumber_InPlacePower,
                                  CfReturnRef|CfPyErrIfNull,
				  "vvv", v1, v2, v3);
}


#if HAVE_GENERATORS
DEFINEFN
vinfo_t* PsycoObject_GetIter(PsycoObject* po, vinfo_t* vi)
{
	getiterfunc f;
	PyTypeObject* t = Psyco_NeedType(po, vi);
	if (t == NULL)
		return NULL;
	if (PyType_HasFeature(t, Py_TPFLAGS_HAVE_ITER))
		f = t->tp_iter;
	else
		f = NULL;
	if (f == NULL) {
		if (PsycoSequence_Check(t))
			return PsycoSeqIter_New(po, vi);
		PycException_SetString(po, PyExc_TypeError,
				       "iteration over non-sequence");
		return NULL;
	}
	else {
		return Psyco_META1(po, f, CfReturnRef|CfPyErrIfNull,
                                   "v", vi);
	}
}

DEFINEFN
vinfo_t* PsycoIter_Next(PsycoObject* po, vinfo_t* iter)
{
	PyTypeObject* tp = Psyco_NeedType(po, iter);
	if (tp == NULL)
		return NULL;
	if (!PsycoIter_Check(tp)) {
		PycException_SetFormat(po, PyExc_TypeError,
				       "'%.100s' object is not an iterator",
				       tp->tp_name);
		return NULL;
	}
	return Psyco_META1(po, tp->tp_iternext, CfReturnRef|CfPyErrIterNext,
			   "v", iter);
}
#endif /* HAVE_GENERATORS */
