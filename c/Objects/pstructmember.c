#include "pstructmember.h"
#include "pstringobject.h"


DEFINEFN
vinfo_t* PsycoMember_GetOne(PsycoObject* po, vinfo_t* addr, PyMemberDef* l)
{
	vinfo_t* v;
	vinfo_t* w1;
	vinfo_t* w2;
	if (l->flags & READ_RESTRICTED)
		goto fallback;

	/* XXX add (some of) the missing types */
	switch (l->type) {
	case T_UBYTE:  /* read a byte, extend it unsigned */
		v = psyco_read_array_item_var(po, addr, psyco_viZero,
					      l->offset, 0);
		break;
	case T_INT:
	case T_UINT:
	case T_LONG:
	case T_ULONG:  /* read a long */
		v = psyco_read_array_item_var(po, addr, psyco_viZero,
					      l->offset, SIZE_OF_LONG_BITS);
		break;
#if 0
		XXX implement pfloatobject.c first
	case T_FLOAT:  /* read a long, turn it into a float */
		w1 = psyco_read_array_item_var(po, addr, psyco_viZero,
					      l->offset, SIZE_OF_LONG_BITS);
		if (w1 == NULL)
			return NULL;
		v = PsycoFloat_FromFloat(w1);
		vinfo_decref(w1, po);
		break;
	case T_DOUBLE:  /* read two longs, turn them into a double */
		w1 = psyco_read_array_item_var(po, addr, psyco_viZero,
					       l->offset, SIZE_OF_LONG_BITS);
		if (w1 == NULL)
			return NULL;
		w2 = psyco_read_array_item_var(po, addr, psyco_viZero,
					       l->offset + SIZEOF_LONG,
					       SIZE_OF_LONG_BITS);
		if (w2 == NULL) {
			vinfo_decref(w1, po);
			return NULL;
		}
		v = PsycoFloat_FromDouble(w1, w2);
		vinfo_decref(w2, po);
		vinfo_decref(w1, po);
		break;
#endif
	case T_CHAR:  /* read a byte, turn it into a char */
		w1 = psyco_read_array_item_var(po, addr, psyco_viZero,
					       l->offset, 0);
		if (w1 == NULL)
			return NULL;
		v = PsycoCharacter_New(w1);
		vinfo_decref(w1, po);
		break;
	case T_OBJECT:  /* read a long, turn it into a PyObject with reference */
		v = psyco_read_array_item_var(po, addr, psyco_viZero,
					      l->offset, SIZE_OF_LONG_BITS);
		if (v == NULL)
			return NULL;
		if (runtime_condition_t(po, integer_non_null(po, v))) {
			need_reference(po, v);
		}
		else {
			v = psyco_viNone;
			vinfo_incref(v);
		}
		break;
	case T_OBJECT_EX:  /* same as T_OBJECT, exception if NULL */
		v = psyco_read_array_item_var(po, addr, psyco_viZero,
					      l->offset, SIZE_OF_LONG_BITS);
		if (v == NULL)
			return NULL;
		if (runtime_condition_t(po, integer_non_null(po, v))) {
			need_reference(po, v);
		}
		else {
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
