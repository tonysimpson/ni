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

	vstart = get_array_item(po, rangelst, RANGE_START);
	if (vstart == NULL)
		return false;

	vlen = get_array_item(po, rangelst, RANGE_LEN);
	if (vlen == NULL)
		return false;

	newobj = psyco_generic_call(po, cimpl_range1,
				    CfReturnRef|CfPyErrIfNull,
				    "vv", vstart, vlen);
	if (newobj == NULL)
		return false;

	/* remove the RANGE_xxx entries from v->array because they are
	   no longer relevant */
	rangelst->array->items[RANGE_START] = NULL;
	vinfo_decref(vstart, po);
	rangelst->array->items[RANGE_LEN]   = NULL;
	vinfo_decref(vlen, po);
	
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
	if (runtime_condition_t(po, cc))
		return integer_sub(po, hi, lo, false);
	else
		return psyco_vi_Zero();
}

static vinfo_t* pbuiltin_range_or_xrange(PsycoObject* po,
					 vinfo_t* vargs, PyTypeObject* ntype)
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
	default: {
		void* fn = (ntype == &PyList_Type) ? cimpl_range : cimpl_xrange;
		return psyco_generic_call(po, fn,
					  CfReturnRef|CfPyErrIfNull,
					  "lv", NULL, vargs);
	    }
	}
	ilen = get_len_of_range(po, ilow, ihigh);
	if (ilen == NULL) goto End;
	
	result = vinfo_new(VirtualTime_New(&psyco_computed_range));
	result->array = array_new(/*RANGE_STEP*/RANGE_START+1);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyList_Type)));
	/* XXX implement xrange() completely and replace '&PyList_Type'
	   above by 'ntype' */
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
	return pbuiltin_range_or_xrange(po, vargs, &PyList_Type);
}

static vinfo_t* pbuiltin_xrange(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	return pbuiltin_range_or_xrange(po, vargs, &PyRange_Type);
}

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
	vinfo_t* zero;
	vinfo_t* vlen;
	vinfo_t* result;
	condition_code_t cc;

	switch (Psyco_TypeSwitch(po, vobj, &psyfs_string_unicode)) {

	case 0:   /* PyString_Type */
		vlen = PsycoString_GET_SIZE(po, vobj);
		if (vlen == NULL)
			return NULL;
		cc = integer_cmp_i(po, vlen, 1, Py_NE);
		if (cc == CC_ERROR)
			return NULL;
		if (runtime_condition_f(po, cc))
			goto use_proxy;

		zero = psyco_vi_Zero();
		result = read_immut_array_item_var(po, vobj, STR_OB_SVAL,
                                                   zero, true);
		vinfo_decref(zero, po);
		if (result == NULL)
			return NULL;
		return PsycoInt_FROM_LONG(result);

#ifdef Py_USING_UNICODE
/* 	case 1:   * PyUnicode_Type * */
/* 		...; */
/* 		break; */
#endif
	default:
		if (PycException_Occurred(po))
			return NULL;
	}
	
   use_proxy:
	return psyco_generic_call(po, cimpl_ord, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vobj);
}

static vinfo_t* pbuiltin_id(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	return PsycoInt_FromLong(vobj);
}

static vinfo_t* pbuiltin_len(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	vinfo_t* result = PsycoObject_Size(po, vobj);
	if (result != NULL)
		result = PsycoInt_FROM_LONG(result);
	return result;
}

static vinfo_t* pbuiltin_abs(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	return PsycoNumber_Absolute(po, vobj);
}

static vinfo_t* pbuiltin_apply(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* alist = NULL;
	vinfo_t* kwdict = NULL;
	vinfo_t* retval;
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */

	switch (tuplesize) {
	case 3:
		kwdict = PsycoTuple_GET_ITEM(vargs, 2);
		if (Psyco_TypeSwitch(po, kwdict, &psyfs_dict) != 0) {
			/* 'kwdict' is not a dictionary */
			break;
		}
		/* fall through */
	case 2:
		alist = PsycoTuple_GET_ITEM(vargs, 1);
		if (Psyco_TypeSwitch(po, alist, &psyfs_tuple) != 0) {
			/* 'alist' is not a tuple */
			/* XXX implement PsycoSequence_Tuple() */
			break;
		}
		/* fall through */
	case 1:
		retval = PsycoEval_CallObjectWithKeywords(po,
					PsycoTuple_GET_ITEM(vargs, 0),
					alist, kwdict);
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
	psyco_computed_range.compute_fn = &compute_range;

#define DEFMETA(name, flags)							\
    cimpl_ ## name = Psyco_DefineModuleFn(md, #name, flags, &pbuiltin_ ## name)

	DEFMETA( range,		METH_VARARGS );
	DEFMETA( xrange,	METH_VARARGS );
	DEFMETA( chr,		METH_VARARGS );
	DEFMETA( ord,		METH_O       );
	DEFMETA( id,		METH_O       );
	DEFMETA( len,		METH_O       );
	DEFMETA( abs,		METH_O       );
	DEFMETA( apply,		METH_VARARGS );
	DEFMETA( divmod,	METH_VARARGS );
#undef META
	Py_XDECREF(md);
}
