
#include "pycencoding.h"


DEFINEFN
void decref_create_new_ref(PsycoObject* po, vinfo_t* w)
{
	/* we must Py_INCREF() the object */
	BEGIN_CODE
	if (is_compiletime(w->source))
		INC_KNOWN_OB_REFCNT((PyObject*)
				    CompileTime_Get(w->source)->value);
	else {
		/* 'w' is in a register because of write_array_item() */
		extra_assert(!RUNTIME_REG_IS_NONE(w));
		INC_OB_REFCNT(RUNTIME_REG(w));
	}
	END_CODE
}

DEFINEFN
void decref_create_new_lastref(PsycoObject* po, vinfo_t* w)
{
	if (!eat_reference(w)) {
		/* in this case we must Py_INCREF() the object */
		BEGIN_CODE
		if (is_compiletime(w->source))
			INC_KNOWN_OB_REFCNT((PyObject*)
					    CompileTime_Get(w->source)->value);
		else {
			/* 'w' is in a register because of write_array_item() */
			extra_assert(!RUNTIME_REG_IS_NONE(w));
			INC_OB_REFCNT(RUNTIME_REG(w));
		}
                END_CODE
	}
}
