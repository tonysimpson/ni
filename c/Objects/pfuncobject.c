#include "pfuncobject.h"
#include "pclassobject.h"


 /***************************************************************/
  /***   Virtual functions                                     ***/

#define FUNC_CODE      QUARTER(offsetof(PyFunctionObject, func_code))
#define FUNC_GLOBALS   QUARTER(offsetof(PyFunctionObject, func_globals))
#define FUNC_DEFAULTS  QUARTER(offsetof(PyFunctionObject, func_defaults))

static source_virtual_t psyco_computed_function;

static bool compute_function(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* newobj;
	vinfo_t* fcode;
	vinfo_t* fglobals;
	vinfo_t* fdefaults;

	fcode = get_array_item(po, v, FUNC_CODE);
	if (fcode == NULL)
		return false;

	fglobals = get_array_item(po, v, FUNC_GLOBALS);
	if (fglobals == NULL)
		return false;

	fdefaults = get_array_item(po, v, FUNC_DEFAULTS);
	if (fdefaults == NULL)
		return false;

	newobj = psyco_generic_call(po, PyFunction_New,
				    CfReturnRef|CfPyErrIfNull,
				    "vv", fcode, fglobals);
	if (newobj == NULL)
		return false;

	if (!psyco_knowntobe(fdefaults, (long) NULL)) {
		if (!psyco_generic_call(po, PyFunction_SetDefaults,
					CfNoReturnValue|CfPyErrIfNonNull,
					"vv", newobj, fdefaults))
			return false;
	}
	
	/* move the resulting non-virtual Python object back into 'v' */
	vinfo_move(po, v, newobj);
	return true;
}


DEFINEFN
vinfo_t* PsycoFunction_New(PsycoObject* po, vinfo_t* fcode,
			   vinfo_t* fglobals, vinfo_t* fdefaults)
{
	vinfo_t* r = vinfo_new(VirtualTime_New(&psyco_computed_function));
	r->array = array_new(MAX3(FUNC_CODE, FUNC_GLOBALS, FUNC_DEFAULTS)+1);
	r->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyFunction_Type)));
	vinfo_incref(fcode);
	r->array->items[FUNC_CODE] = fcode;
	vinfo_incref(fglobals);
	r->array->items[FUNC_GLOBALS] = fglobals;
	if (fdefaults == NULL)
		fdefaults = psyco_vi_Zero();
	else
		vinfo_incref(fdefaults);
	r->array->items[FUNC_DEFAULTS] = fdefaults;
	return r;
}


 /***************************************************************/
  /*** function objects meta-implementation                    ***/

static vinfo_t* pfunction_call(PsycoObject* po, vinfo_t* func,
                              vinfo_t* arg, vinfo_t* kw)
{
	/* calling a Python function: compile the called function if
	   auto_recursion > 0. We promote the Python function to
	   compile-time if it is not known yet. */

	/* XXX this is not how it should be done! We must read the
	   function's func_code, func_globals and func_defaults
	   and pass them further. The code below forces all functions
	   out of virtual-time. */
	if (po->pr.auto_recursion > 0 && psyco_knowntobe(kw, (long) NULL)) {
		PyObject* pyfunc = psyco_pyobj_atcompiletime(po, func);
		if (pyfunc == NULL)
			return NULL;
		return psyco_call_pyfunc(po, (PyFunctionObject*) pyfunc,
					 arg, po->pr.auto_recursion - 1);
	}
	else
		return psyco_generic_call(po, PyFunction_Type.tp_call,
					  CfReturnRef|CfPyErrIfNull,
					  "vvv", func, arg, kw);
}


static vinfo_t* pfunc_descr_get(PsycoObject* po, PyObject* func,
				vinfo_t* obj, PyObject* type)
{
	/* see comments of pmember_get() in pdescrobject.c. */

	/* XXX obj is never Py_None here in the current implementation,
	   but could be if called by other routines than
	   PsycoObject_GenericGetAttr(). */
	return PsycoMethod_New(func, obj, type);
}


DEFINEFN
void psy_funcobject_init()
{
	Psyco_DefineMeta(PyFunction_Type.tp_call, pfunction_call);
	Psyco_DefineMeta(PyFunction_Type.tp_descr_get, pfunc_descr_get);
	psyco_computed_function.compute_fn = &compute_function;
}
