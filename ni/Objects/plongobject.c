#include "plongobject.h"

DEFINEFN
vinfo_t *PsycoLong_AsLong(PsycoObject *po, vinfo_t *v) {
  /* XXX implement me */
  return psyco_generic_call(po, PyLong_AsLong,
                            CfReturnTypeLong | CfPyErrCheckMinus1, "v", v);
}

DEFINEFN
bool PsycoLong_AsDouble(PsycoObject *po, vinfo_t *v, vinfo_t **vd) {
  /* XXX implement me */
  if ((*vd = psyco_generic_call(po, PyLong_AsDouble,
                                CfPyErrCheck | CfReturnTypeDouble, "v", v)) ==
      NULL) {
    return false;
  }
  return true;
}

DEFINEFN
vinfo_t *PsycoLong_FromUnsignedLong(PsycoObject *po, vinfo_t *v) {
  vinfo_t *result = psyco_generic_call(po, PyLong_FromUnsignedLong,
                                       CfCommonNewRefPyObject, "v", v);
  if (result != NULL)
    Psyco_AssertType(po, result, &PyLong_Type);
  return result;
}

DEFINEFN
vinfo_t *PsycoLong_FromLong(PsycoObject *po, vinfo_t *v) {
  vinfo_t *result = psyco_generic_call(po, PyLong_FromLong,
                                       CfCommonNewRefPyObject, "v", v);
  if (result != NULL)
    Psyco_AssertType(po, result, &PyLong_Type);
  return result;
}

DEFINEFN 
vinfo_t *PsycoLong_FROM_LONG(PsycoObject *po, vinfo_t *v) {
  vinfo_t *result = PsycoLong_FromLong(po, v);
  if(result != NULL) {
    vinfo_decref(v, po);
  }
  return result;
}

#define RETLONG(arity, cname, slotname)                                        \
  DEF_KNOWN_RET_TYPE_##arity(cname, PyLong_Type.tp_as_number->slotname,        \
                             (arity) == 1 ? (CfCommonNewRefPyObject)           \
                                          : (CfCommonCheckNotImplemented),     \
                             &PyLong_Type)

#define RETFLOAT(arity, cname, slotname)                                       \
  DEF_KNOWN_RET_TYPE_##arity(cname, PyLong_Type.tp_as_number->slotname,        \
                             (arity) == 1 ? (CfCommonNewRefPyObject)           \
                                          : (CfCommonCheckNotImplemented),     \
                             &PyFloat_Type)

RETLONG(2, plong_add, nb_add)
RETLONG(2, plong_sub, nb_subtract)
RETLONG(2, plong_mul, nb_multiply)
RETLONG(2, plong_mod, nb_remainder)
RETLONG(1, plong_neg, nb_negative)
RETLONG(1, plong_pos, nb_positive)
RETLONG(1, plong_abs, nb_absolute)
RETLONG(1, plong_invert, nb_invert)
RETLONG(2, plong_lshift, nb_lshift)
RETLONG(2, plong_rshift, nb_rshift)
RETLONG(2, plong_and, nb_and)
RETLONG(2, plong_xor, nb_xor)
RETLONG(2, plong_or, nb_or)
RETLONG(2, plong_div, nb_floor_divide)
RETFLOAT(2, plong_true_divide, nb_true_divide)

#undef RETLONG
#undef RETFLOAT

static vinfo_t *plong_pow(PsycoObject *po, vinfo_t *v1, vinfo_t *v2,
                          vinfo_t *v3) {
  /* XXX tony: this is done rather then using a RET* macro because
  nb_power can return PyLongObject or PyFloatObject? Is this true.
   */
  return psyco_generic_call(po, PyLong_Type.tp_as_number->nb_power,
                            CfCommonCheckNotImplemented, "vvv", v1, v2, v3);
}

INITIALIZATIONFN
void psy_longobject_init(void) {
  PyNumberMethods *m = PyLong_Type.tp_as_number;

  Psyco_DefineMeta(m->nb_add, plong_add);
  Psyco_DefineMeta(m->nb_subtract, plong_sub);
  Psyco_DefineMeta(m->nb_multiply, plong_mul);
  Psyco_DefineMeta(m->nb_true_divide, plong_true_divide);
  Psyco_DefineMeta(m->nb_remainder, plong_mod);
  Psyco_DefineMeta(m->nb_power, plong_pow);
  Psyco_DefineMeta(m->nb_negative, plong_neg);
  Psyco_DefineMeta(m->nb_positive, plong_pos);
  Psyco_DefineMeta(m->nb_absolute, plong_abs);
  Psyco_DefineMeta(m->nb_invert, plong_invert);
  Psyco_DefineMeta(m->nb_lshift, plong_lshift);
  Psyco_DefineMeta(m->nb_rshift, plong_rshift);
  Psyco_DefineMeta(m->nb_and, plong_and);
  Psyco_DefineMeta(m->nb_xor, plong_xor);
  Psyco_DefineMeta(m->nb_or, plong_or);
  Psyco_DefineMeta(m->nb_floor_divide, plong_div);
}
