#include "pfloatobject.h"
#include "plongobject.h"

#if HAVE_FP_FN_CALLS


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

#if 0
/* Pow isn't ready yet, so this hasn't been tested */
static int
cimpl_fp_pow(double iv, double iw, double* result) {
    /* Sort out special cases here instead of relying on pow() */
    if (iw == 0) {          /* v**0 is 1, even 0**0 */
        *result = 1.0;
        return 0;
    }
    if (iv == 0.0) {  /* 0**w is error if w<0, else 0 */
        if (iw < 0.0) {
            PyErr_SetString(PyExc_ZeroDivisionError,
                "0.0 cannot be raised to a negative power");
            return -1;
        }
        *result = 0.0;
        return 0;
    }
    if (iv < 0.0 && iw != floor(iw)) {
        PyErr_SetString(PyExc_ValueError,
            "negative number cannot be raised to a fractional power");
        return -1;
    }
    errno = 0;
    PyFPE_START_PROTECT("pow", return NULL)
        *result = pow(iv, iw);
    PyFPE_END_PROTECT(*result)
#ifdef Py_SET_ERANGE_IF_OVERFLOW
        Py_SET_ERANGE_IF_OVERFLOW(*result);
#endif
    if (errno != 0) {
        /* XXX could it be another type of error? */
        PyErr_SetFromErrno(PyExc_OverflowError);
        return -1;
    }
    return 0;
}
#endif  /* 0 */

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


DEFINEFN void 
cimpl_fp_from_long(long value, double* result) 
{
    *result = value; 
}

DEFINEFN void
cimpl_fp_from_float(float value, double* result)
{
    *result = value;
}


DEFINEFN
vinfo_t* PsycoFloat_FromFloat(PsycoObject* po, vinfo_t* vfloat)
{
    vinfo_t* x;
    vinfo_array_t* result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_from_float, CfNoReturnValue|CfPure,
                           "va", vfloat, result);
    if (x != NULL) {
        x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
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
    first_half = vinfo_getitem(floatobj, iFLOAT_OB_FVAL+0);
    second_half = vinfo_getitem(floatobj, iFLOAT_OB_FVAL+1);
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


#define CONVERT_TO_DOUBLE_CORE(vobj, v1, v2, ERRORACTION)       \
    switch (psyco_convert_to_double(po, vobj, &v1, &v2)) {      \
    case true:                                                  \
        break;   /* fine */                                     \
    case false:                                                 \
        return ERRORACTION;  /* error or promotion */           \
    default:                                                    \
        ERRORACTION;                                            \
        return psyco_vi_NotImplemented();  /* cannot do it */   \
    }

#define CONVERT_TO_DOUBLE(vobj, v1, v2)     \
    CONVERT_TO_DOUBLE_CORE(vobj, v1, v2, return_null())  
    
#define CONVERT_TO_DOUBLE2(uobj, u1, u2, vobj, v1, v2)     \
    CONVERT_TO_DOUBLE_CORE(uobj, u1, u2, return_null());    \
    CONVERT_TO_DOUBLE_CORE(vobj, v1, v2, release_double(po, u1, u2))  
    
#define RELEASE_DOUBLE(v1, v2) \
    vinfo_decref(v1, po); \
    vinfo_decref(v2, po);

#define RELEASE_DOUBLE2(v1, v2, u1, u2) \
    vinfo_decref(v1, po); \
    vinfo_decref(v2, po); \
    vinfo_decref(u1, po); \
    vinfo_decref(u2, po); 
    
static vinfo_t*  release_double(PsycoObject* po, vinfo_t* u1, vinfo_t* u2) {
    vinfo_decref(u1, po);
    vinfo_decref(u2, po);
    return NULL;
}
    
static vinfo_t*  return_null() {
    return NULL;
}

    
DEFINEFN
int psyco_convert_to_double(PsycoObject* po, vinfo_t* vobj,
                            vinfo_t** pv1, vinfo_t** pv2)
{
    /* TypeSwitch */
    PyTypeObject* vtp = Psyco_NeedType(po, vobj);
    if (vtp == NULL)
        return false;

    if (PyType_TypeCheck(vtp, &PyInt_Type)) {
        vinfo_array_t* result = array_new(2);
        psyco_generic_call(po, cimpl_fp_from_long, CfNoReturnValue|CfPure,
                           "va", PsycoInt_AS_LONG(po, vobj), result);
        *pv1 = result->items[0];
        *pv2 = result->items[1];
        array_release(result);
        return true;
    }
    if (PyType_TypeCheck(vtp, &PyLong_Type)) {
        return PsycoLong_AsDouble(po, vobj, pv1, pv2);
    }
    if (PyType_TypeCheck(vtp, &PyFloat_Type)) {
        *pv1 = PsycoFloat_AS_DOUBLE_1(po, vobj);
        *pv2 = PsycoFloat_AS_DOUBLE_2(po, vobj);
        if (*pv1 == NULL || *pv2 == NULL)
            return false;
        vinfo_incref(*pv1);
        vinfo_incref(*pv2);
        return true;
    }
    return -1;  /* cannot do it */
}

static vinfo_t* pfloat_cmp(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    /* We could probably assume that these are floats, but using CONVERT is easier */
    CONVERT_TO_DOUBLE2(v, a1, a2, w, b1, b2);
    x = psyco_generic_call(po, cimpl_fp_cmp, CfPure|CfReturnNormal, "vvvv", a1, a2, b1, b2);
    RELEASE_DOUBLE2(a1, a2, b1, b2);
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
    CONVERT_TO_DOUBLE(v, a1, a2);
    x = PsycoFloat_FromDouble(a1, a2);
    RELEASE_DOUBLE(a1, a2);
    return x;
}


static vinfo_t* pfloat_neg(PsycoObject* po, vinfo_t* v)
{
    vinfo_t *a1, *a2, *x;
    vinfo_array_t* result;
    CONVERT_TO_DOUBLE(v, a1, a2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_neg, CfPure|CfNoReturnValue,
                           "vva", a1, a2, result);
    RELEASE_DOUBLE(a1, a2);
    if (x != NULL) {
	    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
}

static vinfo_t* pfloat_abs(PsycoObject* po, vinfo_t* v)
{
    vinfo_t *a1, *a2, *x;
    vinfo_array_t* result;
    CONVERT_TO_DOUBLE(v, a1, a2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_abs, CfPure|CfNoReturnValue,
                           "vva", a1, a2, result);
    RELEASE_DOUBLE(a1, a2);
    if (x != NULL) {
	    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
}

static vinfo_t* pfloat_add(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    CONVERT_TO_DOUBLE2(v, a1, a2, w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_add, CfPure|CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    RELEASE_DOUBLE2(a1, a2, b1, b2);
    if (x != NULL) {
	    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
}

static vinfo_t* pfloat_sub(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    CONVERT_TO_DOUBLE2(v, a1, a2, w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_sub, CfPure|CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    RELEASE_DOUBLE2(a1, a2, b1, b2);
    if (x != NULL) {
	    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
}

static vinfo_t* pfloat_mul(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    CONVERT_TO_DOUBLE2(v, a1, a2, w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_mul, CfPure|CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    RELEASE_DOUBLE2(a1, a2, b1, b2);
    if (x != NULL) {
	    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
}

static vinfo_t* pfloat_div(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
    vinfo_t *a1, *a2, *b1, *b2, *x;
    vinfo_array_t* result;
    CONVERT_TO_DOUBLE2(v, a1, a2, w, b1, b2);
    result = array_new(2);
    x = psyco_generic_call(po, cimpl_fp_div, CfPure|CfNoReturnValue|CfPyErrIfNonNull,
                           "vvvva", a1, a2, b1, b2, result);
    RELEASE_DOUBLE2(a1, a2, b1, b2);
    if (x != NULL) {
	    x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]);
    }
    array_release(result);
    return x;
}


INITIALIZATIONFN
void psy_floatobject_init(void)
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

    INIT_SVIRTUAL(psyco_computed_float, compute_float, 0, 0);
}

#else /* !HAVE_FP_FN_CALLS */
INITIALIZATIONFN
void psy_floatobject_init(void)
{
}
#endif /* !HAVE_FP_FN_CALLS */
