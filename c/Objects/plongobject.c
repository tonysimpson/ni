#include "plongobject.h"


DEFINEFN
vinfo_t* PsycoLong_AsLong(PsycoObject* po, vinfo_t* v)
{
	/* XXX implement me */
	return psyco_generic_call(po, PyLong_AsLong,
				  CfReturnNormal|CfPyErrCheckMinus1,
				  "v", v);
}


static int cimpl_lng_as_double(PyObject* o, double* result)
{
	*result = PyLong_AsDouble(o);
	return (*result == -1.0) ? -1 : 0;
}

DEFINEFN
bool PsycoLong_AsDouble(PsycoObject* po, vinfo_t* v, vinfo_t** vd1, vinfo_t** vd2)
{
	/* XXX implement me */
	vinfo_array_t* result;
	result = array_new(2);
	if (psyco_generic_call(po, cimpl_lng_as_double,
				  CfPyErrCheckMinus1,
				  "va", v, result) == NULL) {
		array_delete(result, po);
		return false;
	}
        *vd1 = result->items[0];
        *vd2 = result->items[1]; 
	array_release(result);
	return true;
}


DEF_KNOWN_RET_TYPE_1(plong_pos, PyLong_Type.tp_as_number->nb_positive,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_1(plong_neg, PyLong_Type.tp_as_number->nb_negative,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_1(plong_invert, PyLong_Type.tp_as_number->nb_invert,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_1(plong_abs, PyLong_Type.tp_as_number->nb_absolute,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_2(plong_add, PyLong_Type.tp_as_number->nb_add,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_2(plong_sub, PyLong_Type.tp_as_number->nb_subtract,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_2(plong_or, PyLong_Type.tp_as_number->nb_or,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)
DEF_KNOWN_RET_TYPE_2(plong_and, PyLong_Type.tp_as_number->nb_and,
		     CfReturnRef|CfPyErrIfNull, PyLong_Type)

DEFINEFN
void psy_longobject_init()
{
	PyNumberMethods *m = PyLong_Type.tp_as_number;
	
	Psyco_DefineMeta(m->nb_positive, plong_pos);
	Psyco_DefineMeta(m->nb_negative, plong_neg);
	Psyco_DefineMeta(m->nb_invert,   plong_invert);
	Psyco_DefineMeta(m->nb_absolute, plong_abs);
	
	Psyco_DefineMeta(m->nb_add,      plong_add);
	Psyco_DefineMeta(m->nb_subtract, plong_sub);
	Psyco_DefineMeta(m->nb_or,       plong_or);
	Psyco_DefineMeta(m->nb_and,      plong_and);
}
