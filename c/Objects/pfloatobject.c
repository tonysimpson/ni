#include "pfloatobject.h"

static int
cimpl_fp_cmp(double a, double b) {
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

static int
cimpl_fp_add(double a, double b, double* result) {
    PyFPE_START_PROTECT("add", return -1)
    *result = a + b;
    PyFPE_END_PROTECT(*result)
    return 0;
}

static int
cimpl_fp_sub(double a, double b, double* result) {
    PyFPE_START_PROTECT("subtract", return -1)
    *result = a - b;
    PyFPE_END_PROTECT(*result)
    return 0;
}


static int
cimpl_fp_mul(double a, double b, double* result) {
    PyFPE_START_PROTECT("multiply", return -1)
    *result = a * b;
    PyFPE_END_PROTECT(*result)
    return 0;
}

static int
cimpl_fp_div(double a, double b, double* result) {
    if (b == 0.0) {
        PyErr_SetString(PyExc_ZeroDivisionError, "float division");
        return -1;
    }
    PyFPE_START_PROTECT("divide", return -1)
    *result = a / b;
    PyFPE_END_PROTECT(*result)
    return 0;
}

static int
cimpl_fp_nonzero(double a) {
	return (a != 0);
}

static void
cimpl_fp_neg(double a, double* result) {
    *result = -a;
}

static void
cimpl_fp_abs(double a, double* result) {
    *result = fabs(a);
}


static void 
cimpl_fp_from_long(long value, double* result) 
{
    *result = value; 
}


DEFINEFN
bool PsycoFloat_AsDouble(PsycoObject* po, vinfo_t* v, vinfo_t** result_1, vinfo_t** result_2)
{
    PyNumberMethods *nb;
    PyTypeObject* tp;
    
    tp = Psyco_NeedType(po, v);
    if (tp == NULL)
        return false;

    if (PsycoFloat_Check(tp)) {
        *result_1 = PsycoFloat_AS_DOUBLE_1(po, v);
        *result_2 = PsycoFloat_AS_DOUBLE_2(po, v);
        vinfo_incref(*result_1);
        vinfo_incref(*result_2);
        return true;
    }

    if ((nb = tp->tp_as_number) == NULL || nb->nb_float == NULL) {
        PycException_SetString(po, PyExc_TypeError,
                       "a float is required");
        return false;
    }

    v = Psyco_META1(po, nb->nb_float,
            CfReturnRef|CfPyErrIfNull,
            "v", v);
    if (v == NULL)
        return false;
    
    /* silently assumes the result is a float object */
    *result_1 = PsycoFloat_AS_DOUBLE_1(po, v);
    *result_2 = PsycoFloat_AS_DOUBLE_2(po, v);
    vinfo_incref(*result_1);
    vinfo_incref(*result_2);
    vinfo_decref(v, po);
    return true;
}

static bool compute_float(PsycoObject* po, vinfo_t* floatobj)
{
    vinfo_t* newobj;
    vinfo_t* first_half;
    vinfo_t* second_half;
    
    /* get the field 'ob_fval' from the Python object 'floatobj' */
    first_half = get_array_item(po, floatobj, FLOAT_OB_FVAL);
    second_half = get_array_item(po, floatobj, FLOAT_OB_FVAL+1);
    if (first_half == NULL || second_half == NULL)
        return false;

    /* call PyFloat_FromDouble() */
    newobj = psyco_generic_call(po, PyFloat_FromDouble, 
        CfPure|CfReturnRef|CfPyErrIfNull,
        "vv", first_half, second_half);

    if (newobj == NULL)
        return false;

    /* move the resulting non-virtual Python object back into 'floatobj' */
    vinfo_move(po, floatobj, newobj);
    return true;
}


DEFINEVAR source_virtual_t psyco_computed_float;


 /***************************************************************/
 /*** float objects meta-implementation                       ***/


#define CONVERT_TO_DOUBLE(vobj, v1, v2) \
    tp = Psyco_NeedType(po, vobj); \
    if (tp == NULL) \
        return NULL; \
    if (PsycoFloat_Check(tp)) { \
        v1 = PsycoFloat_AS_DOUBLE_1(po, vobj);       \
        v2 = PsycoFloat_AS_DOUBLE_2(po, vobj);       \
        if (v1 == NULL || v2 == NULL)             \
            return NULL;                \
        vinfo_incref(v1);          \
        vinfo_incref(v2);          \
    }                           \
    else if (PsycoInt_Check(tp)) { \
		vinfo_array_t* result = array_new(2); \
		psyco_generic_call(po, cimpl_fp_from_long, CfNoReturnValue, "va", PsycoInt_AS_LONG(po, vobj), result); \
		v1 = result->items[0]; \
		v2 = result->items[1]; \
		array_release(result); \
	} \
    else {                          \
        if (PycException_Occurred(po))          \
            return NULL;                \
        return psyco_vi_NotImplemented();          \
    }


static vinfo_t* pfloat_cmp(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    PyTypeObject* tp;
    /* We could probably assume that these are floats, but using CONVERT is easier */
    CONVERT_TO_DOUBLE(v, a1, a2);
    CONVERT_TO_DOUBLE(w, b1, b2);
    x = psyco_generic_call(po, cimpl_fp_cmp, CfPure|CfReturnNormal, "vvvv", a1, a2, b1, b2);
    vinfo_decref(b2, po);
    vinfo_decref(b1, po);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    return x;
}

static vinfo_t* pfloat_nonzero(PsycoObject* po, vinfo_t* v)
{
    vinfo_t *a1, *a2;
	/* Assume that this is a float */
    a1 = PsycoFloat_AS_DOUBLE_1(po, v);       
    a2 = PsycoFloat_AS_DOUBLE_2(po, v);       
    if (a1 == NULL || a2 == NULL)             
        return NULL;
    return psyco_generic_call(po, cimpl_fp_nonzero, CfPure|CfReturnNormal, "vv", a1, a2);
}

static vinfo_t* pfloat_pos(PsycoObject* po, vinfo_t* v)
{
    vinfo_t *a1, *a2, *x;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    x = PsycoFloat_FromDouble(a1, a2);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    return x;
}


static vinfo_t* pfloat_neg(PsycoObject* po, vinfo_t* v)
{
    vinfo_t *a1, *a2, *x;
    vinfo_array_t* result;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_neg, CfNoReturnValue,
                           "vva", a1, a2, result);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    if (x == NULL) {
           array_delete(result, po);
           return NULL;
    }
    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    array_release(result);
    return x;
}

static vinfo_t* pfloat_abs(PsycoObject* po, vinfo_t* v)
{
    vinfo_t *a1, *a2, *x;
    vinfo_array_t* result;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_abs, CfNoReturnValue,
                           "vva", a1, a2, result);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    if (x == NULL) {
           array_delete(result, po);
           return NULL;
    }
    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    array_release(result);
    return x;
}

static vinfo_t* pfloat_add(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    CONVERT_TO_DOUBLE(w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_add, CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    vinfo_decref(b2, po);
    vinfo_decref(b1, po);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    if (x == NULL) {
           array_delete(result, po);
           return NULL;
    }
    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    array_release(result);
    return x;
}

static vinfo_t* pfloat_sub(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    CONVERT_TO_DOUBLE(w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_sub, CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    vinfo_decref(b2, po);
    vinfo_decref(b1, po);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    if (x == NULL) {
           array_delete(result, po);
           return NULL;
    }
    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    array_release(result);
    return x;
}

static vinfo_t* pfloat_mul(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    CONVERT_TO_DOUBLE(w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_mul, CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    vinfo_decref(b2, po);
    vinfo_decref(b1, po);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    if (x == NULL) {
           array_delete(result, po);
           return NULL;
    }
    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    array_release(result);
    return x;
}

static vinfo_t* pfloat_div(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    PyTypeObject* tp;
    CONVERT_TO_DOUBLE(v, a1, a2);
    CONVERT_TO_DOUBLE(w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_div, CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    vinfo_decref(b2, po);
    vinfo_decref(b1, po);
    vinfo_decref(a2, po);
    vinfo_decref(a1, po);
    if (x == NULL) {
           array_delete(result, po);
           return NULL;
    }
    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    array_release(result);
    return x;
}


DEFINEFN
void psy_floatobject_init()
{
    PyNumberMethods *m = PyFloat_Type.tp_as_number;
    Psyco_DefineMeta(m->nb_nonzero,  pfloat_nonzero);
    
    Psyco_DefineMeta(m->nb_positive, pfloat_pos);
    Psyco_DefineMeta(m->nb_negative, pfloat_neg);
    Psyco_DefineMeta(m->nb_absolute, pfloat_abs);
    
    Psyco_DefineMeta(m->nb_add,      pfloat_add);
    Psyco_DefineMeta(m->nb_subtract, pfloat_sub);
    Psyco_DefineMeta(m->nb_multiply, pfloat_mul);
    Psyco_DefineMeta(m->nb_divide,   pfloat_div);

    Psyco_DefineMeta(PyFloat_Type.tp_compare, pfloat_cmp);

    psyco_computed_float.compute_fn = &compute_float;
}
