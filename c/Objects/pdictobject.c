#include "pdictobject.h"


#define           DICT_MA_USED    QUARTER(offsetof(PyDictObject, ma_used))

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

static vinfo_t* psyco_dict_length(PsycoObject* po, vinfo_t* vi)
{
	return read_array_item(po, vi, DICT_MA_USED);
}


INITIALIZATIONFN
void psy_dictobject_init(void)
{
	PyMappingMethods *m = PyDict_Type.tp_as_mapping;
	Psyco_DefineMeta(m->mp_length, psyco_dict_length);

#if !HAVE_struct_dictobject
        extra_assert(sizeof(struct _dictobject) + PyGC_HEAD_SIZE ==
                     PyDict_Type.tp_basicsize);
#endif
}
