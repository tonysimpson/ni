#include "prangeobject.h"
#include "../Objects/plongobject.h"
#include "../Python/pycinternal.h"
#include "piterobject.h"

/***************************************************************/
/***   Virtual xrange objects                                ***/

/*  This is private in rangeobject.c
 */
struct rangeobject {
  PyObject_HEAD long start;
  long step;
  long len;
};

/* only step == 1 currently supported */
#define RANGEOBJECT_start DEF_FIELD(struct rangeobject, long, start, OB_type)
#define RANGEOBJECT_step                                                       \
  DEF_FIELD(struct rangeobject, long, step, RANGEOBJECT_start)
#define RANGEOBJECT_len                                                        \
  DEF_FIELD(struct rangeobject, long, len, RANGEOBJECT_step)
#define iRANGE_START FIELD_INDEX(RANGEOBJECT_start)
#define iRANGE_STEP FIELD_INDEX(RANGEOBJECT_step)
#define iRANGE_LEN FIELD_INDEX(RANGEOBJECT_len)
#define RANGEOBJECT_TOTAL FIELDS_TOTAL(RANGEOBJECT_len)

DEFINEVAR source_virtual_t psyco_computed_range;

static PyObject *cimpl_xrange_new(long start, long len) {
  struct rangeobject *obj;
  obj = PyObject_New(struct rangeobject, &PyRange_Type);
  if (obj == NULL)
    return NULL;
  obj->start = start;
  obj->len = len;
  obj->step = 1;
  return (PyObject *)obj;
}

static bool compute_xrange(PsycoObject *po, vinfo_t *v) {
  vinfo_t *vstart;
  vinfo_t *vlen;
  vinfo_t *newobj;

  /* get the fields from the Python object 'v' */
  vstart = vinfo_getitem(v, iRANGE_START);
  if (vstart == NULL)
    return false;

  vlen = vinfo_getitem(v, iRANGE_LEN);
  if (vlen == NULL)
    return false;

  /* call PyRange_New() */
  newobj =
      psyco_generic_call(po, cimpl_xrange_new, CfPure | CfCommonNewRefPyObject,
                         "vv", vstart, vlen);
  if (newobj == NULL)
    return false;

  /* move the resulting non-virtual Python object back into 'v' */
  vinfo_move(po, v, newobj);
  return true;
}

static PyObject *direct_compute_xrange(vinfo_t *v, char *data) {
  long start, len;
  start = direct_read_vinfo(vinfo_getitem(v, iRANGE_START), data);
  len = direct_read_vinfo(vinfo_getitem(v, iRANGE_LEN), data);
  if (PyErr_Occurred())
    return NULL;
  return cimpl_xrange_new(start, len);
}

DEFINEFN
vinfo_t *PsycoRange_NEW(PsycoObject *po, vinfo_t *start, vinfo_t *len) {
  vinfo_t *r = vinfo_new(VirtualTime_New(&psyco_computed_range));
  r->array = array_new(RANGEOBJECT_TOTAL);
  r->array->items[iOB_TYPE] = vinfo_new(CompileTime_New((long)(&PyRange_Type)));
  r->array->items[iRANGE_START] = start;
  r->array->items[iRANGE_STEP] = psyco_vi_One();
  r->array->items[iRANGE_LEN] = len;
  return r;
}

/***************************************************************/
/*** xrange objects meta-implementation                      ***/

static vinfo_t *prange_length(PsycoObject *po, vinfo_t *vi) {
  return psyco_get_field(po, vi, RANGEOBJECT_len);
}

static vinfo_t *prange_item(PsycoObject *po, vinfo_t *a, vinfo_t *i) {
  condition_code_t cc;
  vinfo_t *vstart;
  vinfo_t *vstep;
  vinfo_t *vlen;
  vinfo_t *vdelta;
  vinfo_t *result;

  vlen = psyco_get_const(po, a, RANGEOBJECT_len);
  if (vlen == NULL)
    return NULL;

  cc = integer_cmp(po, i, vlen, Py_GE | COMPARE_UNSIGNED);
  if (cc == CC_ERROR)
    return NULL;

  if (runtime_condition_f(po, cc)) {
    PycException_SetString(po, PyExc_IndexError,
                           "xrange object index out of range");
    return NULL;
  }
  assert_nonneg(i);

  vstep = psyco_get_const(po, a, RANGEOBJECT_step);
  if (vstep == NULL)
    return NULL;

  vstart = psyco_get_const(po, a, RANGEOBJECT_start);
  if (vstart == NULL)
    return NULL;

  vdelta = integer_mul(po, vstep, i, false);
  if (vdelta == NULL)
    return NULL;

  result = integer_add(po, vstart, vdelta, false);
  vinfo_decref(vdelta, po);
  if (result == NULL)
    return NULL;
  return PsycoLong_FROM_LONG(po, result);
}

INITIALIZATIONFN
void psy_rangeobject_init(void) {
  PySequenceMethods *m = PyRange_Type.tp_as_sequence;
  Psyco_DefineMeta(m->sq_length, prange_length);
  Psyco_DefineMeta(m->sq_item, prange_item);
  if (PyRange_Type.tp_iter != NULL) /* Python >= 2.3 */
    Psyco_DefineMeta(PyRange_Type.tp_iter, PsycoSeqIter_New);
  INIT_SVIRTUAL(psyco_computed_range, compute_xrange, direct_compute_xrange, 0,
                0, 0);
}
