#include "plongobject.h"
#include "../Python/pycinternal.h"  /* for BINARY_FLOOR_DIVIDE */


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
		array_release(result);
		return false;
	}
        *vd1 = result->items[0];
        *vd2 = result->items[1]; 
	array_release(result);
	return true;
}


/* XXX this assumes that operations between longs always return a long.
   There are hints that this might change in future releases of Python. */
#define RETLONG(arity, cname, slotname)						\
	DEF_KNOWN_RET_TYPE_##arity(cname,					\
				   PyLong_Type.tp_as_number->slotname,		\
				   (arity)==1 ? (CfReturnRef|CfPyErrIfNull) :	\
				        (CfReturnRef|CfPyErrNotImplemented),	\
				   &PyLong_Type)

/* this only assumes that the result is a long if the other argument
   is also a long or an int (for example, str*long or long+float do
   not return longs) */
#define RETLONG_NUM(cname, slotname)					    \
static vinfo_t* cname(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)  {	    \
	vinfo_t* result = psyco_generic_call(po,			    \
				PyLong_Type.tp_as_number->slotname,	    \
				CfReturnRef|CfPyErrNotImplemented,	    \
				"vv", v1, v2);				    \
	if (result != NULL && !IS_NOTIMPLEMENTED(result)) {		    \
		PyTypeObject* vtp;					    \
		vtp = Psyco_KnownType(v1);				    \
		if (vtp == &PyLong_Type || vtp == &PyInt_Type) {	    \
			vtp = Psyco_KnownType(v2);			    \
			if (vtp == &PyLong_Type || vtp == &PyInt_Type) {    \
				Psyco_AssertType(po, result, &PyLong_Type); \
			}						    \
		}							    \
	}								    \
	return result;							    \
}


RETLONG_NUM(	plong_add,		nb_add)
RETLONG_NUM(	plong_sub,		nb_subtract)
RETLONG_NUM(	plong_mul,		nb_multiply)
RETLONG_NUM(	plong_classic_div,	nb_divide)
RETLONG_NUM(	plong_mod,		nb_remainder)
/*RETLONG(3,	plong_pow,		nb_power)*/
RETLONG(1,	plong_neg,		nb_negative)
RETLONG(1,	plong_pos,		nb_positive)
RETLONG(1,	plong_abs,		nb_absolute)
RETLONG(1,	plong_invert,		nb_invert)
RETLONG_NUM(	plong_lshift,		nb_lshift)
RETLONG_NUM(	plong_rshift,		nb_rshift)
RETLONG_NUM(	plong_and,		nb_and)
RETLONG_NUM(	plong_xor,		nb_xor)
RETLONG_NUM(	plong_or,		nb_or)
#ifdef BINARY_FLOOR_DIVIDE
RETLONG_NUM(	plong_div,		nb_floor_divide)
     /*RETFLOAT(2,	plong_true_divide,	nb_true_divide)  XXX-implement*/
#endif

#undef RETLONG
#undef RETLONG_NUM


INITIALIZATIONFN
void psy_longobject_init(void)
{
	PyNumberMethods *m = PyLong_Type.tp_as_number;

	Psyco_DefineMeta(m->nb_add,		plong_add);
	Psyco_DefineMeta(m->nb_subtract,	plong_sub);
	Psyco_DefineMeta(m->nb_multiply,	plong_mul);
	Psyco_DefineMeta(m->nb_divide,		plong_classic_div);
	Psyco_DefineMeta(m->nb_remainder,	plong_mod);
	/*Psyco_DefineMeta(m->nb_power,		plong_pow);*/
	Psyco_DefineMeta(m->nb_negative,	plong_neg);
	Psyco_DefineMeta(m->nb_positive,	plong_pos);
	Psyco_DefineMeta(m->nb_absolute,	plong_abs);
	Psyco_DefineMeta(m->nb_invert,		plong_invert);
	Psyco_DefineMeta(m->nb_lshift,		plong_lshift);
	Psyco_DefineMeta(m->nb_rshift,		plong_rshift);
	Psyco_DefineMeta(m->nb_and,		plong_and);
	Psyco_DefineMeta(m->nb_xor,		plong_xor);
	Psyco_DefineMeta(m->nb_or,		plong_or);
#ifdef BINARY_FLOOR_DIVIDE
	Psyco_DefineMeta(m->nb_floor_divide,	plong_div);
/* Psyco_DefineMeta(m->nb_true_divide,	plong_true_divide);  XXX-implement*/
#endif
}
