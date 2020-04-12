#include "ipyencoding.h"
#include "../pycodegen.h"

DEFINEFN void function_return(PsycoObject *po, Source retval, int nframelocal,
                              long retpos) {
  int post_return_stack_depth = 0;
  int pre_return_stack_correction = 0;
  int return_depth = sizeof(long) + INITIAL_STACK_DEPTH;
  BEGIN_CODE
  /* load the return value into EAX for regular functions, EBX for functions
   * with a prologue */
  if (retval != SOURCE_DUMMY) {
    reg_t rg = nframelocal > 0 ? REG_ANY_CALLEE_SAVED : REG_FUNCTIONS_RETURN;
    LOAD_REG_FROM(retval, rg);
  }

  if (nframelocal > 0) {
    /* psyco_emit_header() was used; first clear the stack only up to and not
     * including the frame-local data */
    int framelocpos = getstack(LOC_CONTINUATION->array->items[0]->source);
    STACK_CORRECTION(framelocpos - po->stack_depth);
    po->stack_depth = framelocpos;
    /* perform Python-specific cleanup */
    FINALIZE_FRAME_LOCALS();
    MOV_R_R(REG_FUNCTIONS_RETURN, REG_ANY_CALLEE_SAVED);
  }

  pre_return_stack_correction = retpos - po->stack_depth;
  if (RBP_IS_RESERVED) {
    pre_return_stack_correction += sizeof(long);
  }
  STACK_CORRECTION(pre_return_stack_correction);
  po->stack_depth += pre_return_stack_correction;
  if (RBP_IS_RESERVED) {
    POP_R(REG_X64_RBP);
    psyco_dec_stackdepth(po);
  }
  post_return_stack_depth = po->stack_depth - return_depth;
  if (post_return_stack_depth >= 0x8000) {
    POP_R(REG_TRANSIENT_1);
    STACK_CORRECTION(-post_return_stack_depth);
    PUSH_R(REG_TRANSIENT_1);
    po->stack_depth = return_depth;
    RET();
  } else {
    RET_N(post_return_stack_depth);
  }
  po->stack_depth = 0;

  END_CODE
}

DEFINEFN
code_t *decref_dealloc_calling(code_t *code, PsycoObject *po, reg_t rg,
                               destructor fn) {
  extra_assert(offsetof(PyObject, ob_type) < 128);
  extra_assert(offsetof(PyTypeObject, tp_dealloc) < 128);
  DEC_OB_REFCNT_NZ(rg);
  BEGIN_SHORT_COND_JUMP(0, CC_NE); /* NE is the not zero flag */
  if (fn == NULL) {
    BEGIN_CALL(1);
    CALL_SET_ARG_FROM_REG(rg, 0);
    MOV_R_O8(REG_TRANSIENT_1, rg, offsetof(PyObject, ob_type));
    MOV_R_O8(REG_TRANSIENT_1, REG_TRANSIENT_1,
             offsetof(PyTypeObject, tp_dealloc));
    END_CALL_R(REG_TRANSIENT_1);
  } else {
    BEGIN_CALL(1);
    CALL_SET_ARG_FROM_REG(rg, 0);
    END_CALL_I(fn);
  }
  END_SHORT_JUMP(0);
  return code;
}

DEFINEFN
void decref_create_new_ref(PsycoObject *po, vinfo_t *w) {
  /* we must Py_INCREF() the object */
  BEGIN_CODE
  if (is_compiletime(w->source))
    INC_KNOWN_OB_REFCNT((PyObject *)CompileTime_Get(w->source)->value);
  else {
    /* 'w' is in a register because of write_array_item() */
    extra_assert(!RUNTIME_REG_IS_NONE(w));
    INC_OB_REFCNT(RUNTIME_REG(w));
  }
  END_CODE
}

DEFINEFN
bool decref_create_new_lastref(PsycoObject *po, vinfo_t *w) {
  bool could_eat = eat_reference(w);
  if (!could_eat) {
    /* in this case we must Py_INCREF() the object */
    BEGIN_CODE
    if (is_compiletime(w->source))
      INC_KNOWN_OB_REFCNT((PyObject *)CompileTime_Get(w->source)->value);
    else {
      /* 'w' is in a register because of write_array_item() */
      extra_assert(!RUNTIME_REG_IS_NONE(w));
      INC_OB_REFCNT(RUNTIME_REG(w));
    }
    END_CODE
  }
  return could_eat;
}
