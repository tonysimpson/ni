#include "pstructmember.h"
#include "pstringobject.h"
#include "pintobject.h"


DEFINEFN
vinfo_t* PsycoMember_GetOne(PsycoObject* po, vinfo_t* addr, PyMemberDef* l)
{
	condition_code_t cc;
	vinfo_t* v;
	vinfo_t* w1;
	vinfo_t* w2;
        vinfo_t* zero;
	if (l->flags & READ_RESTRICTED)
		goto fallback;

	/* XXX add (some of) the missing types */
	switch (l->type) {
	case T_UBYTE:  /* read a byte, extend it unsigned */
		zero = psyco_vi_Zero();
		v = psyco_read_array_item_var(po, addr, zero,
					      l->offset, 0);
		vinfo_decref(zero, po);
		if (v != NULL)
			v = PsycoInt_FROM_LONG(v);
		break;
	case T_INT:
	case T_UINT:
	case T_LONG:
	case T_ULONG:  /* read a long */
		zero = psyco_vi_Zero();
		v = psyco_read_array_item_var(po, addr, zero,
					      l->offset, SIZE_OF_LONG_BITS);
		vinfo_decref(zero, po);
		if (v != NULL)
			v = PsycoInt_FROM_LONG(v);
		break;
#if 0
		XXX implement PsycoFloat_FromFloat()
	case T_FLOAT:  /* read a long, turn it into a float */
		zero = psyco_vi_Zero();
		w1 = psyco_read_array_item_var(po, addr, zero,
					      l->offset, SIZE_OF_LONG_BITS);
		vinfo_decref(zero, po);
		if (w1 == NULL)
			return NULL;
		v = PsycoFloat_FromFloat(w1);
		vinfo_decref(w1, po);
		break;
#endif
	case T_DOUBLE:  /* read two longs, turn them into a double */
		zero = psyco_vi_Zero();
		w1 = psyco_read_array_item_var(po, addr, zero,
					       l->offset, SIZE_OF_LONG_BITS);
		if (w1 == NULL) {
			vinfo_decref(zero, po);
			return NULL;
		}
		w2 = psyco_read_array_item_var(po, addr, zero,
					       l->offset + SIZEOF_LONG,
					       SIZE_OF_LONG_BITS);
		vinfo_decref(zero, po);
		if (w2 == NULL) {
			vinfo_decref(w1, po);
			return NULL;
		}
		v = PsycoFloat_FROM_DOUBLE(w1, w2);
		break;
	case T_CHAR:  /* read a byte, turn it into a char */
		zero = psyco_vi_Zero();
		w1 = psyco_read_array_item_var(po, addr, zero,
					       l->offset, 0);
		vinfo_decref(zero, po);
		if (w1 == NULL)
			return NULL;
		v = PsycoCharacter_New(w1);
		vinfo_decref(w1, po);
		break;
	case T_OBJECT:  /* read a long, turn it into a PyObject with reference */
		zero = psyco_vi_Zero();
		v = psyco_read_array_item_var(po, addr, zero,
					      l->offset, SIZE_OF_LONG_BITS);
		vinfo_decref(zero, po);
		if (v == NULL)
			return NULL;
		cc = integer_non_null(po, v);
		if (cc == CC_ERROR) {
			vinfo_decref(v, po);
			return NULL;
		}

		if (runtime_condition_t(po, cc)) {
			/* 'v' contains a non-NULL value */
			need_reference(po, v);
		}
		else {
			/* 'v' contains NULL */
			vinfo_decref(v, po);
			v = psyco_vi_None();
		}
		break;
	case T_OBJECT_EX:  /* same as T_OBJECT, exception if NULL */
		zero = psyco_vi_Zero();
		v = psyco_read_array_item_var(po, addr, zero,
					      l->offset, SIZE_OF_LONG_BITS);
		vinfo_decref(zero, po);
		if (v == NULL)
			return NULL;
		cc = integer_non_null(po, v);
		if (cc == CC_ERROR) {
			vinfo_decref(v, po);
			return NULL;
		}

		if (runtime_condition_t(po, cc)) {
			/* 'v' contains a non-NULL value */
			need_reference(po, v);
		}
		else {
			/* 'v' contains NULL */
			vinfo_decref(v, po);
			PycException_SetString(po, PyExc_AttributeError,
					       l->name);
			return NULL;
		}
		break;
	default:
		goto fallback;
	}
	return v;

  fallback:
	/* call the Python implementation for cases we cannot handle directy */
	return psyco_generic_call(po, PyMember_GetOne,
				  CfReturnRef|CfPyErrIfNull, "vl", addr, l);
}
