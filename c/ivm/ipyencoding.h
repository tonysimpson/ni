 /***************************************************************/
/***     Processor- and language-dependent code producers      ***/
 /***************************************************************/

#ifndef _IPYENCODING_H
#define _IPYENCODING_H

#include "iencoding.h"
#include "ivm-insns.h"

/* See comments in i386/ipyencoding.h about these functions */

inline void dictitem_check_change(PsycoObject* po,
				  code_t* onchange_target,
				  PyDictObject* dict, PyDictEntry* ep)
{
	int index        = ep - dict->ma_table;
	PyObject* key    = ep->me_key;
	PyObject* result = ep->me_value;
	word_t* arg1;
	word_t* arg2;
	word_t* arg3;
	word_t* arg4;
	word_t* arg5;
	
	Py_INCREF(key);    /* XXX these become immortal */
	Py_INCREF(result); /* XXX                       */
	BEGIN_CODE
	/* this special instruction quickly checks that the same
	   object is still in place in the dictionary */
	INSN_checkdict(&arg1, &arg2, &arg3, &arg4, &arg5);
	*arg1 = (word_t) dict;
	*arg2 = (word_t) key;
	*arg3 = (word_t) result;
	*arg4 = (word_t) onchange_target;
	*arg5 = index;
	extra_assert(arg4 == ((word_t*)code) - 2);
	extra_assert(arg5 == ((word_t*)code) - 1);
	END_CODE
}

inline code_t* dictitem_update_nochange(code_t* originalmacrocode,
                                        PyDictObject* dict, PyDictEntry* new_ep)
{
	int index = new_ep - dict->ma_table;
	word_t* arg5 = ((word_t*)originalmacrocode) - 1;
	*arg5 = index;
	return originalmacrocode;
}

inline void dictitem_update_jump(code_t* originalmacrocode, code_t* target)
{
	word_t* arg4 = ((word_t*)originalmacrocode) - 2;
	word_t* arg5 = ((word_t*)originalmacrocode) - 1;
	*arg4 = (word_t) target;
	*arg5 = (word_t) -1;
	/* jump always -- we effectively changed the checkdict
	   instruction into an unconditional jump */
}


inline void psyco_incref_nv(PsycoObject* po, vinfo_t* v)
{
	BEGIN_CODE
	INSN_nv_push(v->source);
	INSN_incref();
	END_CODE
}

inline void psyco_incref_rt(PsycoObject* po, vinfo_t* v)
{
	BEGIN_CODE
	INSN_rt_push(v->source);
	INSN_incref();
	END_CODE
}

inline void psyco_decref_rt(PsycoObject* po, vinfo_t* v)
{
	BEGIN_CODE
	INSN_rt_push(v->source);
	INSN_decref();
	END_CODE
}

inline void psyco_decref_c(PsycoObject* po, PyObject* o)
{
	word_t* arg;
	BEGIN_CODE
	INSN_decrefnz(&arg);
	*arg = (word_t) o;
	END_CODE
}

EXTERNFN void decref_create_new_ref(PsycoObject* po, vinfo_t* w);
EXTERNFN bool decref_create_new_lastref(PsycoObject* po, vinfo_t* w);


/* called by psyco_emit_header() */
#define INITIALIZE_FRAME_LOCALS(nframelocal)   do {     \
  STACK_CORRECTION(sizeof(long)*((nframelocal)-1));     \
  INSN_immed(0);    /* f_exc_type, initially NULL */    \
} while (0)

/* called by psyco_finish_return() */
#define WRITE_FRAME_EPILOGUE(retval, nframelocal)   do {                        \
  /* load the return value into 'flag' -- little abuse here :-)  */             \
  if (retval != SOURCE_DUMMY) {                                                 \
    INSN_nv_push(retval);                                                       \
    INSN_retval();                                                              \
  }                                                                             \
  if (nframelocal > 0)                                                          \
    {                                                                           \
      /* psyco_emit_header() was used; first clear the stack only up to and not \
         including the frame-local data */                                      \
      int framelocpos = getstack(LOC_CONTINUATION->array->items[0]->source);    \
      STACK_CORRECTION(framelocpos - po->stack_depth);                          \
      po->stack_depth = framelocpos;                                            \
                                                                                \
      /* perform Python-specific cleanup */                                     \
      INSN_exitframe();                                                         \
      INSNPOPPED(3);                                                            \
    }                                                                           \
} while (0)

#endif /* _IPYENCODING_H */
