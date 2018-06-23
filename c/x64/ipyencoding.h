 /***************************************************************/
/***     Processor- and language-dependent code producers      ***/
 /***************************************************************/

#ifndef _IPYENCODING_H
#define _IPYENCODING_H

#include "iencoding.h"
#include "../processor.h"
#include "../dispatcher.h"

#include "../Objects/pobject.h"
#include "../Objects/pdictobject.h"

#define TRACE_START_COMPILING(c)    do { /* nothing */ } while (0)

#define DICT_ITEM_CHECK_CC     CC_NE
#define DICT_ITEM_KEYVALUE(index, key, value, mprg)  do {\
  MOV_R_UI(REG_TRANSIENT_1, index);\
  CMP_R_O8(REG_TRANSIENT_1, mprg, offsetof(PyDictObject, ma_mask));\
  MOV_R_O8(mprg, mprg, offsetof(PyDictObject, ma_table));\
  BEGIN_SHORT_COND_JUMP(0, CC_L);\
  MOV_R_I(REG_TRANSIENT_1, key);\
  CMP_R_UO32(REG_TRANSIENT_1, mprg, (index) * sizeof(PyDictEntry) + offsetof(PyDictEntry, me_key));\
  BEGIN_SHORT_COND_JUMP(1, DICT_ITEM_CHECK_CC);\
  MOV_R_I(REG_TRANSIENT_1, value);\
  CMP_R_UO32(REG_TRANSIENT_1, mprg, (index) * sizeof(PyDictEntry) + offsetof(PyDictEntry, me_value));\
  END_SHORT_JUMP(0);\
  END_SHORT_JUMP(1);\
} while (0)

PSY_INLINE void* dictitem_check_change(PsycoObject* po,
                                   PyDictObject* dict, PyDictEntry* ep)
{
  int index        = ep - dict->ma_table;
  PyObject* key    = ep->me_key;
  PyObject* result = ep->me_value;
  reg_t mprg;
  code_t* codebase;
  
  Py_INCREF(key);    /* XXX these become immortal */
  Py_INCREF(result); /* XXX                       */
  
  BEGIN_CODE
  NEED_CC();
  NEED_FREE_REG(mprg);
  /* write code that quickly checks that the same
     object is still in place in the dictionary */
  LOAD_REG_FROM_IMMED(mprg, (qword_t)dict);
  codebase = code;
  DICT_ITEM_KEYVALUE(index, key, result, mprg);
  END_CODE
  return codebase;
}

PSY_INLINE void dictitem_update_nochange(void* originalmacrocode,
                                     PyDictObject* dict, PyDictEntry* new_ep)
{
  PsycoObject *po = NULL;
  int index = new_ep - dict->ma_table;
  code_t* code = (code_t*) originalmacrocode;
#undef ONLY_UPDATING
#define ONLY_UPDATING 1
  DICT_ITEM_KEYVALUE(index, 0, 0, 0);
#undef ONLY_UPDATING
#define ONLY_UPDATING 0
}


/* emit the equivalent of the Py_INCREF() macro */
/* the PyObject* is stored in the register 'rg' */
/* XXX if Py_REF_DEBUG is set (Python debug mode), the
       following will not properly update _Py_RefTotal.
       Don't trust _Py_RefTotal with Psyco.     */
#define INC_OB_REFCNT(rg)			do {    \
  NEED_CC_REG(rg);                                      \
  INC_OB_REFCNT_internal(rg);                           \
} while (0)
/* same as above, preserving the cc */
#define INC_OB_REFCNT_CC(rg)			do {    \
  bool _save_ccreg = HAS_CCREG(po);                     \
  if (_save_ccreg) PUSH_CC();                     \
  INC_OB_REFCNT_internal(rg);                           \
  if (_save_ccreg) POP_CC();                      \
} while (0)
#define INC_OB_REFCNT_internal(rg) do {\
    if (offsetof(PyObject, ob_refcnt) == 0) {\
        ADD_A_I8(rg, 1);\
    } else {\
        ADD_O8_I8(rg, offsetof(PyObject, ob_refcnt), 1);\
    }\
} while (0)

/* Py_INCREF() for a compile-time-known 'pyobj' */
#define INC_KNOWN_OB_REFCNT(pyobj)    do {              \
  NEED_CC();                                            \
  MOV_R_I(REG_TRANSIENT_1,  &(pyobj)->ob_refcnt);\
  ADD_A_I8(REG_TRANSIENT_1, 1);\
 } while (0)

/* Py_DECREF() for a compile-time 'pyobj' assuming counter cannot reach zero */
#define DEC_KNOWN_OB_REFCNT_NZ(pyobj)    do {           \
  NEED_CC();                                            \
  MOV_R_I(REG_TRANSIENT_1,  &(pyobj)->ob_refcnt);\
  SUB_A_I8(REG_TRANSIENT_1, 1);\
 } while (0)

/* like DEC_OB_REFCNT() but assume the reference counter cannot reach zero */
#define DEC_OB_REFCNT_NZ(rg)    do {                    \
  NEED_CC_REG(rg);                                      \
  SUB_R_I8(rg, 1);\
} while (0)

/* internal utilities for the macros below */
EXTERNFN code_t* decref_dealloc_calling(code_t* code, PsycoObject* po, reg_t rg,
                                        destructor fn);

/* the equivalent of Py_DECREF().
   XXX Same remark as INC_OB_REFCNT().
   We correctly handle the Py_TRACE_REFS case,
   however, by calling the _Py_Dealloc() function.
   Slow but correct (and you have the debugging Python
   version anyway, so you are not looking for top speed
   but just testing things). */
#ifdef Py_TRACE_REFS
/* debugging only */
# define DEC_OB_REFCNT(rg)  (code=decref_dealloc_calling(code, po, rg,  \
                                                         _Py_Dealloc))
#else
# define DEC_OB_REFCNT(rg)  (code=decref_dealloc_calling(code, po, rg, NULL))
#endif

/* the equivalent of Py_DECREF() when we know the type of the object
   (assuming that tp_dealloc never changes for a given type) */
#ifdef Py_TRACE_REFS
/* debugging only */
# define DEC_OB_REFCNT_T(rg, type)  (code=decref_dealloc_calling(code, po, rg, \
                                                                 _Py_Dealloc))
#else
# define DEC_OB_REFCNT_T(rg, type)  (code=decref_dealloc_calling(code, po, rg, \
                                                          (type)->tp_dealloc))
#endif


/***************************************************************/
 /***   generic reference counting functions                  ***/

/* emit Py_INCREF(v) for run-time v */
PSY_INLINE void psyco_incref_rt(PsycoObject* po, vinfo_t* v)
{
  reg_t rg;
  BEGIN_CODE
  RTVINFO_IN_REG(v);
  rg = RUNTIME_REG(v);
  INC_OB_REFCNT(rg);
  END_CODE
}

/* emit Py_INCREF(v) for non-virtual v */
PSY_INLINE void psyco_incref_nv(PsycoObject* po, vinfo_t* v)
{
  if (!is_compiletime(v->source))
    psyco_incref_rt(po, v);
  else
    {
      BEGIN_CODE
      INC_KNOWN_OB_REFCNT((PyObject*) CompileTime_Get(v->source)->value);
      END_CODE
    }
}

/* emit Py_DECREF(v) for run-time v. Used by vcompiler.c when releasing a
   run-time vinfo_t holding a reference to a Python object. */
PSY_INLINE void psyco_decref_rt(PsycoObject* po, vinfo_t* v)
{
  PyTypeObject* tp = Psyco_KnownType(v);
  reg_t rg;
  BEGIN_CODE
  RTVINFO_IN_REG(v);
  rg = RUNTIME_REG(v);
  if (tp != NULL)
    DEC_OB_REFCNT_T(rg, tp);
  else
    DEC_OB_REFCNT(rg);
  END_CODE
}

/* emit Py_DECREF(o) for a compile-time o */
PSY_INLINE void psyco_decref_c(PsycoObject* po, PyObject* o)
{
  BEGIN_CODE
  DEC_KNOWN_OB_REFCNT_NZ(o);
  END_CODE
}


/* to store a new reference to a Python object into a memory structure,
   use psyco_put_field() or psyco_put_field_array() to store the value
   proper and then one of the following two functions to adjust the
   reference counter: */

/* normal case */
EXTERNFN void decref_create_new_ref(PsycoObject* po, vinfo_t* w);

/* if 'w' is supposed to be freed soon, this function tries (if possible)
   to move an eventual Python reference owned by 'w' to the memory
   structure.  This avoids a Py_INCREF()/Py_DECREF() pair.
   Returns 'true' if the reference was successfully transfered;
   'false' does not mean failure. */
EXTERNFN bool decref_create_new_lastref(PsycoObject* po, vinfo_t* w);


/* called by psyco_emit_header() */
#define INITIALIZE_FRAME_LOCALS(nframelocal)   do {\
  if(RBP_IS_RESERVED) {\
      PUSH_R(REG_X64_RBP);\
      psyco_inc_stackdepth(po);\
      MOV_R_R(REG_X64_RBP, REG_X64_RSP);\
  }\
  STACK_CORRECTION(sizeof(long)*((nframelocal)-1));\
  PUSH_IMMED(0);    /* f_exc_type, initially NULL */\
} while (0)

/* called by psyco_finish_return() */
#define FINALIZE_FRAME_LOCALS(nframelocal) do {\
  CMP_I8_A(0, REG_X64_RSP);\
  BEGIN_SHORT_COND_JUMP(0, CC_E);\
  BEGIN_CALL(3);\
  CALL_SET_ARG_FROM_RT(LOC_CONTINUATION->array->items[0]->source, 0);\
  CALL_SET_ARG_FROM_RT(LOC_CONTINUATION->array->items[1]->source, 1);\
  CALL_SET_ARG_FROM_RT(LOC_CONTINUATION->array->items[2]->source, 2);\
  END_CALL_I(&cimpl_finalize_frame_locals);\
  END_SHORT_JUMP(0);\
} while (0)


#define WRITE_FRAME_EPILOGUE(retval, nframelocal)   do {                        \
  /* load the return value into EAX for regular functions, EBX for functions    \
     with a prologue */                                                         \
  if (retval != SOURCE_DUMMY) {                                                 \
    reg_t rg = nframelocal>0 ? REG_ANY_CALLEE_SAVED : REG_FUNCTIONS_RETURN;     \
    LOAD_REG_FROM(retval, rg);                                                  \
  }                                                                             \
                                                                                \
  if (nframelocal > 0)                                                          \
    {                                                                           \
      /* psyco_emit_header() was used; first clear the stack only up to and not \
         including the frame-local data */                                      \
      int framelocpos = getstack(LOC_CONTINUATION->array->items[0]->source);    \
      STACK_CORRECTION(framelocpos - po->stack_depth);                          \
      po->stack_depth = framelocpos;                                            \
                                                                                \
      /* perform Python-specific cleanup */                                     \
      FINALIZE_FRAME_LOCALS(nframelocal);                                       \
      MOV_R_R(REG_FUNCTIONS_RETURN, REG_ANY_CALLEE_SAVED);            \
    }                                                                           \
} while (0)

/* retpos is position in stack of the return address pushed by call */
/* the stack must be cleared up this */
#define FUNCTION_RET(retpos) do {\
    int post_return_stack_depth = 0;\
    int pre_return_stack_correction = retpos - po->stack_depth;\
    int return_depth = sizeof(long) + INITIAL_STACK_DEPTH;\
    if(RBP_IS_RESERVED) {\
        pre_return_stack_correction += sizeof(long);\
    }\
    STACK_CORRECTION(pre_return_stack_correction);\
    po->stack_depth += pre_return_stack_correction;\
    if(RBP_IS_RESERVED) {\
        POP_R(REG_X64_RBP);\
        psyco_dec_stackdepth(po);\
    }\
    post_return_stack_depth = po->stack_depth - return_depth;\
    if (post_return_stack_depth >= 0x8000) {\
        POP_R(REG_TRANSIENT_1);\
        STACK_CORRECTION(-post_return_stack_depth);\
        PUSH_R(REG_TRANSIENT_1);\
        po->stack_depth = return_depth;\
        STACK_DEPTH_CHECK();\
        RET();\
    } else {\
        STACK_DEPTH_CHECK();\
        RET_N(post_return_stack_depth);\
    }\
    po->stack_depth = 0;\
} while (0)

/* implemented in pycompiler.c */
EXTERNFN void cimpl_finalize_frame_locals(PyObject*, PyObject*, PyObject*);

static void function_return(PsycoObject *po, Source retval, int nframelocal, long retpos) {
    int post_return_stack_depth = 0;
    int pre_return_stack_correction = 0;
    int return_depth = sizeof(long) + INITIAL_STACK_DEPTH;
    BEGIN_CODE
    /* load the return value into EAX for regular functions, EBX for functions with a prologue */
    if (retval != SOURCE_DUMMY) {
        reg_t rg = nframelocal>0 ? REG_ANY_CALLEE_SAVED : REG_FUNCTIONS_RETURN;
        LOAD_REG_FROM(retval, rg);
    }
  
    if (nframelocal > 0)
    {
        /* psyco_emit_header() was used; first clear the stack only up to and not including the frame-local data */
        int framelocpos = getstack(LOC_CONTINUATION->array->items[0]->source);
        STACK_CORRECTION(framelocpos - po->stack_depth);
        po->stack_depth = framelocpos;
        /* perform Python-specific cleanup */

        CMP_I8_A(0, REG_X64_RSP);
        BEGIN_SHORT_COND_JUMP(0, CC_E);
        BEGIN_CALL(3);
        CALL_SET_ARG_FROM_RT(LOC_CONTINUATION->array->items[0]->source, 0);
        CALL_SET_ARG_FROM_RT(LOC_CONTINUATION->array->items[1]->source, 1);
        CALL_SET_ARG_FROM_RT(LOC_CONTINUATION->array->items[2]->source, 2);
        END_CALL_I(&cimpl_finalize_frame_locals);
        END_SHORT_JUMP(0);
        
        MOV_R_R(REG_FUNCTIONS_RETURN, REG_ANY_CALLEE_SAVED);
    }

    pre_return_stack_correction = retpos - po->stack_depth;
    if(RBP_IS_RESERVED) {
        pre_return_stack_correction += sizeof(long);
    }
    STACK_CORRECTION(pre_return_stack_correction);
    po->stack_depth += pre_return_stack_correction;
    if(RBP_IS_RESERVED) {
        POP_R(REG_X64_RBP);
        psyco_dec_stackdepth(po);
    }
    post_return_stack_depth = po->stack_depth - return_depth;
    if (post_return_stack_depth >= 0x8000) {
        POP_R(REG_TRANSIENT_1);
        STACK_CORRECTION(-post_return_stack_depth);
        PUSH_R(REG_TRANSIENT_1);
        po->stack_depth = return_depth;
        STACK_DEPTH_CHECK();
        RET();
    } else {
        STACK_DEPTH_CHECK();
        RET_N(post_return_stack_depth);
    }
    po->stack_depth = 0;

    END_CODE
}

#endif /* _IPYENCODING_H */
