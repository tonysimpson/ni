#include "psycofuncobject.h"


static vinfo_t* meta_psycofunction_call(PsycoObject* po, vinfo_t* func,
				   vinfo_t* arg, vinfo_t* kw)
{
	PsycoFunctionObject* psyfunc;
	if (!psyco_knowntobe(kw, (long) NULL))  /* XXX support keywords */
		return psyco_generic_call(po, PsycoFunction_Type.tp_call,
					  CfReturnRef|CfPyErrIfNull,
					  "vvv", func, arg, kw);
	
	/* calling a Psyco proxy to a Python function: always compile
	   the function. We promote the PsycoFunctionObject to
	   compile-time if it is not known yet. */
	psyfunc = (PsycoFunctionObject*) psyco_pyobj_atcompiletime(po, func);
	if (psyfunc == NULL)
		return NULL;
	return psyco_call_pyfunc(po, psyfunc->psy_func,
				 arg, psyfunc->psy_recursion);
}


static vinfo_t* meta_psy_descr_get(PsycoObject* po, PyObject* func,
                                   vinfo_t* obj, PyObject* type)
{
	/* see comments of pmember_get() in pdescrobject.c. */
	return PsycoMethod_New(func, obj, type);
}


DEFINEFN
void psy_psycofuncobject_init()
{
	Psyco_DefineMeta(PsycoFunction_Type.tp_call, meta_psycofunction_call);
	Psyco_DefineMeta(PsycoFunction_Type.tp_descr_get, meta_psy_descr_get);
}
