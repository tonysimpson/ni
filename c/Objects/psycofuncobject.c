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


DEFINEFN
void psy_psycofuncobject_init()
{
	Psyco_DefineMeta(PsycoFunction_Type.tp_call, meta_psycofunction_call);
}
