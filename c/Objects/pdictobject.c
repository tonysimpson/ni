#include "pdictobject.h"


DEFINEFN
vinfo_t* PsycoDict_New(PsycoObject* po)
{
	/* no virtual dicts (yet?) */
	vinfo_t* v = psyco_generic_call(po, PyDict_New,
					CfReturnRef|CfPyErrIfNull, "");
	if (v == NULL)
		return NULL;

	/* the result is a dict */
	set_array_item(po, v, OB_TYPE,
		       vinfo_new(CompileTime_New((long)(&PyDict_Type))));
	return v;
}


DEFINEFN
void psy_dictobject_init()
{
	PyMappingMethods *m = PyDict_Type.tp_as_mapping;
	Psyco_DefineMeta(m->mp_length, psyco_generic_mut_ob_size);
}
