#include "pabstract.h"
#include "pintobject.h"
#include "plongobject.h"
#include "pstringobject.h"
#include "piterobject.h"
#include "ptupleobject.h"


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
{	/* 'kw' may be NULL */
	ternaryfunc call;
	PyTypeObject* tp = Psyco_NeedType(po, callable_object);
	if (tp == NULL)
		return NULL;

	if ((call = tp->tp_call) != NULL) {
		vinfo_t* result;
		if (kw == NULL)
			kw = psyco_vi_Zero();
		else
			vinfo_incref(kw);
		result = Psyco_META3(po, call, CfReturnRef|CfPyErrIfNull,
				     "vvv", callable_object, args, kw);
		vinfo_decref(kw, po);
		return result;
	}
	PycException_SetFormat(po, PyExc_TypeError,
			       "object of type '%.100s' is not callable",
			       tp->tp_name);
	return NULL;
}

DEFINEFN
vinfo_t* PsycoEval_CallObjectWithKeywords(PsycoObject* po,
					  vinfo_t* callable_object,
					  vinfo_t* args, vinfo_t* kw)
{
	vinfo_t* result;
	
	if (args == NULL)
		args = PsycoTuple_New(0, NULL);
	else if (Psyco_TypeSwitch(po, args, &psyfs_tuple) != 0) {
		goto use_proxy;
	}
	else
		vinfo_incref(args);

	if (kw != NULL && Psyco_TypeSwitch(po, kw, &psyfs_dict) != 0) {
		vinfo_decref(args, po);
		goto use_proxy;
	}

	result = PsycoObject_Call(po, callable_object, args, kw);
	vinfo_decref(args, po);
	return result;

   use_proxy:
	if (PsycoErr_Occurred(po))
		return NULL;
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
		PyTypeObject* ktp = Psyco_NeedType(po, key);
		if (ktp == NULL)
			return NULL;
		if (PsycoInt_Check(ktp))
			return PsycoSequence_GetItem(po, o,
						     PsycoInt_AS_LONG(po, key));
		else if (PsycoLong_Check(ktp)) {
			vinfo_t* key_value = PsycoLong_AsLong(po, key);
			if (key_value == NULL)
				return NULL;
			return PsycoSequence_GetItem(po, o, key_value);
		}
		return type_error(po, "sequence index must be integer");
	}

	return type_error(po, "unsubscriptable object");
}

DEFINEFN
bool PsycoObject_SetItem(PsycoObject* po, vinfo_t* o, vinfo_t* key,
			 vinfo_t* value)
{
	/* XXX implement me */
	if (value != NULL)
		return psyco_generic_call(po, PyObject_SetItem,
                                          CfNoReturnValue|CfPyErrIfNonNull,
                                          "vvv", o, key, value) != NULL;
	else
		return psyco_generic_call(po, PyObject_DelItem,
                                          CfNoReturnValue|CfPyErrIfNonNull,
                                          "vv", o, key) != NULL;
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
	/* XXX implement me */
	if (value != NULL)
		return psyco_generic_call(po, PySequence_SetItem,
                                          CfNoReturnValue|CfPyErrIfNonNull,
                                          "vvv", o, i, value) != NULL;
	else
		return psyco_generic_call(po, PySequence_DelItem,
                                          CfNoReturnValue|CfPyErrIfNonNull,
                                          "vv", o, i) != NULL;
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
	/* XXX implement me */
	return psyco_generic_call(po, PySequence_Contains,
				  CfReturnNormal|CfPyErrIfNeg,
				  "vv", seq, ob);
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
		if (slotw && PyType_IsSubtype(wtp, vtp)) {
			x = Psyco_META2(po, slotw,
					CfReturnRef|CfPyErrNotImplemented,
					"vv", v, w);
			if (IS_IMPLEMENTED(x))
				return x;  /* may be NULL */
			vinfo_decref(x, po); /* can't do it */
			slotw = NULL;
		}
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


/* XXX implement the in-place operators */
DEFINEFN
vinfo_t* PsycoNumber_InPlaceAdd(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceAdd,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceOr(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceOr,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceXor(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceXor,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceAnd(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceAnd,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceLshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceLshift,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceRshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceRshift,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceSubtract(PsycoObject* po, vinfo_t* v1, vinfo_t*v2) {
	return psyco_generic_call(po, PyNumber_InPlaceSubtract,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceMultiply(PsycoObject* po, vinfo_t* v1, vinfo_t*v2) {
	return psyco_generic_call(po, PyNumber_InPlaceMultiply,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceDivide(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) {
	return psyco_generic_call(po, PyNumber_InPlaceDivide,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlaceRemainder(PsycoObject* po, vinfo_t* v1,vinfo_t*v2) {
	return psyco_generic_call(po, PyNumber_InPlaceRemainder,
                                  CfReturnRef|CfPyErrIfNull,
				  "vv", v1, v2);
}

DEFINEFN
vinfo_t* PsycoNumber_InPlacePower(PsycoObject* po, vinfo_t* v1, vinfo_t* v2,
				  vinfo_t* v3) {
	return psyco_generic_call(po, PyNumber_InPlacePower,
                                  CfReturnRef|CfPyErrIfNull,
				  "vvv", v1, v2, v3);
}


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
			return PsycoSeqIter_New(vi);
		PycException_SetString(po, PyExc_TypeError,
				       "iteration over non-sequence");
		return NULL;
	}
	else {
		return psyco_generic_call(po, f, CfReturnRef|CfPyErrIfNull,
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
