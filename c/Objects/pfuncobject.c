#include "pfuncobject.h"
#include "pclassobject.h"


static vinfo_t* pfunction_call(PsycoObject* po, vinfo_t* func,
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
}
