#include "pbltinmodule.h"
#include "../Objects/pintobject.h"
#include "../Objects/ptupleobject.h"
#include "../Objects/plistobject.h"
#include "../Objects/pstringobject.h"


static PyCFunction cimpl_range;
static PyCFunction cimpl_xrange;
static PyCFunction cimpl_chr;
static PyCFunction cimpl_ord;
static PyCFunction cimpl_id;
static PyCFunction cimpl_len;
static PyCFunction cimpl_abs;
static PyCFunction cimpl_apply;
static PyCFunction cimpl_divmod;


#if HAVE_METH_O
# define METH_O_WRAPPER(name, self, arg)    do { } while (0)  /* nothing */
#else
# define METH_O_WRAPPER(name, self, arg)    do {                                \
    if (PsycoTuple_Load(arg) != 1)                                              \
      return psyco_generic_call(po, cimpl_ ## name, CfReturnRef|CfPyErrIfNull,  \
                                "vv", self, arg);                               \
    arg = PsycoTuple_GET_ITEM(arg, 0);                                          \
} while (0)
#endif


/***************************************************************/
/* range().
   This is not for xrange(), which is not optimized by Psyco here.
   As a result range() is now more efficient than xrange() in common cases.
   This could be taken care of by short-circuiting the Psyco implementation
   of xrange() to that of range() itself and thus completely bypassing
   Python's own xrange types. */
DEFINEVAR source_virtual_t psyco_computed_range;

static PyObject* cimpl_range1(long start, long len)
{
  PyObject* lst = PyList_New(len);
  long i;
  if (lst != NULL)
    {
      for (i=0; i<len; i++)
        {
          PyObject* o = PyInt_FromLong(start);
          if (o == NULL)
            {
              Py_DECREF(lst);
              return NULL;
            }
          PyList_SET_ITEM(lst, i, o);
          start++;
        }
    }
  return lst;
}

static bool compute_range(PsycoObject* po, vinfo_t* rangelst)
{
	vinfo_t* newobj;
	vinfo_t* vstart;
	vinfo_t* vlen;

	vstart = vinfo_getitem(rangelst, RANGE_START);
	if (vstart == NULL)
		return false;

	vlen = vinfo_getitem(rangelst, RANGE_LEN);
	if (vlen == NULL)
		return false;

	newobj = psyco_generic_call(po, cimpl_range1,
				    CfReturnRef|CfPyErrIfNull,
				    "vv", vstart, vlen);
	if (newobj == NULL)
		return false;

	/* forget fields that were only relevant in virtual-time */
	vinfo_array_shrink(po, rangelst, LIST_TOTAL);
	
	/* move the resulting non-virtual Python object back into 'rangelst' */
	vinfo_move(po, rangelst, newobj);
	return true;
}


static vinfo_t* get_len_of_range(PsycoObject* po, vinfo_t* lo, vinfo_t* hi
				 /*, vinfo_t* step == 1 currently*/)
{
	/* translated from bltinmodule.c */
	condition_code_t cc = integer_cmp(po, lo, hi, Py_LT);
	if (cc == CC_ERROR)
		return NULL;
	if (runtime_condition_t(po, cc)) {
		vinfo_t* vresult = integer_sub(po, hi, lo, false);
		assert_nonneg(vresult);
		return vresult;
	}
	else
		return psyco_vi_Zero();
}

static vinfo_t* pbuiltin_range_or_xrange(PsycoObject* po, vinfo_t* vargs)
{
	vinfo_t* result = NULL;
	vinfo_t* ilen;
	vinfo_t* ilow = NULL;
	vinfo_t* ihigh = NULL;
	/*vinfo_t* istep = NULL;*/
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	
	switch (tuplesize) {
	case 1:
		ihigh = PsycoInt_AsLong(po, PsycoTuple_GET_ITEM(vargs, 0));
		if (ihigh == NULL) goto End;
		ilow = vinfo_new(CompileTime_New(0));
		break;
	/*case 3:
		istep = PsycoInt_AsLong(po, PsycoTuple_GET_ITEM(vargs, 2));
		if (istep == NULL) return NULL;*/
		/* fall through */
	case 2:
		ilow  = PsycoInt_AsLong(po, PsycoTuple_GET_ITEM(vargs, 0));
		if (ilow == NULL) goto End;
		ihigh = PsycoInt_AsLong(po, PsycoTuple_GET_ITEM(vargs, 1));
		if (ihigh == NULL) goto End;
		break;
	default:
		return NULL;
	}
	ilen = get_len_of_range(po, ilow, ihigh);
	if (ilen == NULL) goto End;
	
	result = vinfo_new(VirtualTime_New(&psyco_computed_range));
	result->array = array_new(RANGE_TOTAL);
	result->array->items[iOB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyList_Type)));
	/* XXX implement xrange() completely and replace '&PyList_Type'
	   above by '&PyRange_Type' for this case */
	result->array->items[RANGE_LEN] = ilen;
	result->array->items[RANGE_START] = ilow;
	ilow = NULL;

   End:
	/*vinfo_xdecref(istep, po);*/
	vinfo_xdecref(ihigh, po);
	vinfo_xdecref(ilow, po);
	return result;
}

static vinfo_t* pbuiltin_range(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
  vinfo_t* result = pbuiltin_range_or_xrange(po, vargs);
  if (result == NULL && !PycException_Occurred(po))
    result = psyco_generic_call(po, cimpl_range,
                                CfReturnRef|CfPyErrIfNull,
                                "lv", NULL, vargs);
  return result;
}

static vinfo_t* pbuiltin_xrange(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
  vinfo_t* result = pbuiltin_range_or_xrange(po, vargs);
  if (result == NULL && !PycException_Occurred(po))
    result = psyco_generic_call(po, cimpl_xrange,
                                CfReturnRef|CfPyErrIfNull,
                                "lv", NULL, vargs);
  return result;
}

#if NEW_STYLE_TYPES
static vinfo_t* prange_new(PsycoObject* po, vinfo_t* vtype,
			   vinfo_t* vargs, vinfo_t* vkw)
{
  /* for Python >= 2.3, where __builtin__.xrange is a type */
  vinfo_t* result = pbuiltin_range_or_xrange(po, vargs);
  if (result == NULL && !PycException_Occurred(po))
    result = psyco_generic_call(po, PyRange_Type.tp_new,
                                CfReturnRef|CfPyErrIfNull,
                                "lvl", &PyRange_Type,
                                vargs, NULL);
  return result;
}
#else
# define prange_new  NULL
#endif

static vinfo_t* pbuiltin_chr(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* intval;
	vinfo_t* result;
	condition_code_t cc;
	
	if (PsycoTuple_Load(vargs) != 1)
		goto use_proxy;
	intval = PsycoInt_AsLong(po, PsycoTuple_GET_ITEM(vargs, 0));
	if (intval == NULL)
		return NULL;

	cc = integer_cmp_i(po, intval, 255, Py_GT|COMPARE_UNSIGNED);
	if (cc == CC_ERROR) {
		vinfo_decref(intval, po);
		return NULL;
	}
	if (runtime_condition_f(po, cc)) {
		vinfo_decref(intval, po);
		goto use_proxy;
	}

	result = PsycoCharacter_New(intval);
	vinfo_decref(intval, po);
	return result;

   use_proxy:
	return psyco_generic_call(po, cimpl_chr, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

static vinfo_t* pbuiltin_ord(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	vinfo_t* result;
	METH_O_WRAPPER(ord, vself, vobj);

	if (!PsycoCharacter_Ord(po, vobj, &result))
		return NULL;
	
	if (result != NULL)
		return PsycoInt_FROM_LONG(result);
	
	return psyco_generic_call(po, cimpl_ord, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vobj);
}

static vinfo_t* pbuiltin_id(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	METH_O_WRAPPER(id, vself, vobj);
	return PsycoInt_FromLong(vobj);
}

static vinfo_t* pbuiltin_len(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	vinfo_t* result;
	METH_O_WRAPPER(len, vself, vobj);
	
	result = PsycoObject_Size(po, vobj);
	if (result != NULL)
		result = PsycoInt_FROM_LONG(result);
	return result;
}

static vinfo_t* pbuiltin_abs(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	METH_O_WRAPPER(abs, vself, vobj);
	return PsycoNumber_Absolute(po, vobj);
}

static vinfo_t* pbuiltin_apply(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* alist = NULL;
	vinfo_t* kwdict = NULL;
	vinfo_t* retval;
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	PyTypeObject* argt;
	vinfo_t* t = NULL;

	switch (tuplesize) {
	case 3:
		kwdict = PsycoTuple_GET_ITEM(vargs, 2);
		if (Psyco_VerifyType(po, kwdict, &PyDict_Type) != true) {
			/* 'kwdict' is not a dictionary */
			break;
		}
		/* fall through */
	case 2:
		alist = PsycoTuple_GET_ITEM(vargs, 1);
		argt = Psyco_NeedType(po, alist);
		if (argt == NULL)
			return NULL;
		if (!PyType_TypeCheck(argt, &PyTuple_Type)) {
			/* 'alist' is not a tuple */
			if (!PsycoSequence_Check(argt))
				break;  /* give up */
			t = PsycoSequence_Tuple(po, alist);
			if (t == NULL)
				break;  /* give up */
			alist = t;
		}
		/* fall through */
	case 1:
		retval = PsycoEval_CallObjectWithKeywords(po,
					PsycoTuple_GET_ITEM(vargs, 0),
					alist, kwdict);
		vinfo_xdecref(t, po);
		return retval;
	}

	if (PycException_Occurred(po))
		return NULL;
	return psyco_generic_call(po, cimpl_apply, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

static vinfo_t* pbuiltin_divmod(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	
	if (tuplesize == 2) {
		return PsycoNumber_Divmod(po,
					  PsycoTuple_GET_ITEM(vargs, 0),
					  PsycoTuple_GET_ITEM(vargs, 1));
	}
	return psyco_generic_call(po, cimpl_divmod, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}


/***************************************************************/


INITIALIZATIONFN
void psyco_bltinmodule_init(void)
{
	PyObject* md = Psyco_DefineMetaModule("__builtin__");
	/* ranges are lists, which are mutable;
           they must be forced out of virtual-time across function calls */
        INIT_SVIRTUAL(psyco_computed_range, compute_range, 0, NW_FORCE);

#define DEFMETA(name, flags)							\
    cimpl_ ## name = Psyco_DefineModuleFn(md, #name, flags, &pbuiltin_ ## name)

	DEFMETA( range,		METH_VARARGS );
	DEFMETA( chr,		METH_VARARGS );
	DEFMETA( ord,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( id,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( len,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( abs,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( apply,		METH_VARARGS );
	DEFMETA( divmod,	METH_VARARGS );
	cimpl_xrange = Psyco_DefineModuleC(md, "xrange", METH_VARARGS,
                                           &pbuiltin_xrange, prange_new);
#undef META
	Py_XDECREF(md);
}
