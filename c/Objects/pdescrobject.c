#include "pdescrobject.h"


static vinfo_t* pmember_get(PsycoObject* po, PyMemberDescrObject* descr,
			    vinfo_t* obj, PyTypeObject *type)
{
	/* a meta-implementation for member_get() of descrobject.c.
	   Note that not all parameters are 'vinfo_t*'; only 'obj'
	   is. This is because PsycoObject_GenericGetAttr() gives
	   immediate values for the other two arguments. */

	/* We assume that 'obj' is a valid instance of 'type'. */
	return PsycoMember_GetOne(po, obj, descr->d_member);
}


DEFINEFN
void psy_descrobject_init()
{
	PyObject* dummy;
	PyTypeObject* PyMemberDescr_Type;
	PyMemberDef dummydef;

	/* Member descriptors */
	/* any better way to get a pointer to PyMemberDescr_Type? */
	memset(&dummydef, 0, sizeof(dummydef));
	dummydef.name = "dummy";
	dummy = PyDescr_NewMember(&PsycoFunction_Type, &dummydef);
	PyMemberDescr_Type = dummy->ob_type;
	Py_DECREF(dummy);

	Psyco_DefineMeta(PyMemberDescr_Type->tp_descr_get,
			 pmember_get);
}
