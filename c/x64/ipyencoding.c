#include "ipyencoding.h"
#include "../pycodegen.h"


DEFINEFN
code_t* decref_dealloc_calling(code_t* code, PsycoObject* po, reg_t rg,
                               destructor fn)
{
  extra_assert(offsetof(PyObject, ob_type) < 128);
  extra_assert(offsetof(PyTypeObject, tp_dealloc) < 128);
  DEC_OB_REFCNT_NZ(rg);
  BEGIN_SHORT_COND_JUMP(0, CC_NE); /* NE is the not zero flag */
  BEGIN_CALL();
  CALL_SET_ARG_FROM_REG(rg, 0);
  if (fn == NULL) {
    MOV_R_O8(REG_TRANSIENT_1, rg, offsetof(PyObject, ob_type));
    LEA_R_O8(REG_TRANSIENT_1, REG_TRANSIENT_1, offsetof(PyTypeObject, tp_dealloc));
  }
  else {
    MOV_R_I(REG_TRANSIENT_1, fn);
  }
  END_CALL_R(REG_TRANSIENT_1);
  END_SHORT_JUMP(0);
  return code;
}

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
bool decref_create_new_lastref(PsycoObject* po, vinfo_t* w)
{
	bool could_eat = eat_reference(w);
	if (!could_eat) {
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
	return could_eat;
}
