#include "pfuncobject.h"


static vinfo_t* function_call(PsycoObject* po, vinfo_t* func,
                              vinfo_t* arg, vinfo_t* kw)
{
	/* calling a Python function: compile the called function if
	   auto_recursion > 0. We promote the Python function to
	   compile-time if it is not known yet. */
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


DEFINEFN
void psy_funcobject_init()
{
	Psyco_DefineMeta(PyFunction_Type.tp_call, function_call);
}
