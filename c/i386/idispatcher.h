 /***************************************************************/
/***       Processor-specific routines for dispatcher.c        ***/
 /***************************************************************/

#ifndef _IDISPATCHER_H
#define _IDISPATCHER_H

#include "../vcompiler.h"
#include "../processor.h"
#include "iencoding.h"


/***************************************************************/
 /***   Freezing                                              ***/

EXTERNFN void fpo_find_regs_array(vinfo_array_t* source, PsycoObject* po);


/***************************************************************/
 /***   Unification                                           ***/

/* idispatcher.c implements psyco_unify(),
   whose header is given in dispatcher.h.
   Conversely, dispatcher.c implements the following function
   which is declared here because it is really internal: */
typedef void (*fz_find_fn) (vinfo_t* a, RunTimeSource bsource, void* extra);
EXTERNFN void fz_find_runtimes(vinfo_array_t* aa, FrozenPsycoObject* fpo,
                               fz_find_fn callback, void* extra, bool clear);


/***************************************************************/
 /***   Promotion                                             ***/

/* Define to 1 to emit a "compare/jump-if-equal" pair of instructions
   that checks for the most common case (actually the last seen one). */
#define PROMOTION_FAST_COMMON_CASE   1


#if PROMOTION_FAST_COMMON_CASE
                                       /* for FIX_JUMP_IF_EQUAL() */
#  define INTERNAL_PROMOTION_FIELDS    code_t* jump_if_equal_code;
#else
#  define INTERNAL_PROMOTION_FIELDS    /* nothing */
#endif

struct ipromotion_s {
  INTERNAL_PROMOTION_FIELDS
};


inline code_t* fix_fast_common_case(void* fs, long value,
                                    code_t* codeptr)
{
#if PROMOTION_FAST_COMMON_CASE
  FIX_JUMP_IF_EQUAL(((struct ipromotion_s*)fs)->jump_if_equal_code,
                    value, codeptr);
#endif
  return codeptr;
}

inline void* ipromotion_finish(PsycoObject* po, vinfo_t* fix, void* do_promotion)
{
  long xsource;
  struct ipromotion_s* fs;

#if PROMOTION_FAST_COMMON_CASE
  code_t* jeqcode;
  BEGIN_CODE
  NEED_CC();
  RTVINFO_IN_REG(fix);
  xsource = fix->source;
  RESERVE_JUMP_IF_EQUAL(RSOURCE_REG(xsource));
  jeqcode = code;
  END_CODE
#else
  xsource = fix->source;
#endif
  
  if (PROMOTION_FAST_COMMON_CASE || !RSOURCE_REG_IS_NONE(xsource))
    {
      /* remove from 'po->regarray' this value which will soon no longer
         be RUN_TIME */
      REG_NUMBER(po, RSOURCE_REG(xsource)) = NULL;
      SET_RUNTIME_REG_TO_NONE(fix);
    }

  fs = (struct ipromotion_s*) psyco_call_code_builder(po, do_promotion,
                                                      PROMOTION_FAST_COMMON_CASE,
                                                      xsource);
#if PROMOTION_FAST_COMMON_CASE
  fs->jump_if_equal_code = jeqcode;
#endif
  return fs;
}


/***************************************************************/
 /***   Misc.                                                 ***/

inline void conditional_jump_to(PsycoObject* po, code_t* target,
                                condition_code_t condition)
{
  BEGIN_CODE
  switch (condition) {
  case CC_ALWAYS_FALSE:          /* never jumps */
    break;
  case CC_ALWAYS_TRUE:
    JUMP_TO(target);             /* always jumps */
    break;
  default:
    FAR_COND_JUMP_TO(target, condition);
  }
  END_CODE
}

/* reserve a small buffer of code behind po->code in which conditional
   code can be stored.  See make_code_conditional(). */
inline void setup_conditional_code_bounds(PsycoObject* po, PsycoObject* po2)
{
  code_t* code2 = po->code + SIZE_OF_SHORT_CONDITIONAL_JUMP;
  po2->code = code2;
  po2->codelimit = code2 + RANGE_OF_SHORT_CONDITIONAL_JUMP;
}

/* mark a small buffer reserved by setup_conditional_code_bounds() to be
   only executed if 'condition' holds. */
inline void make_code_conditional(PsycoObject* po, code_t* codeend,
                                  condition_code_t condition)
{
  code_t* target;
  code_t* code2 = po->code + SIZE_OF_SHORT_CONDITIONAL_JUMP;
  extra_assert(code2 <= codeend &&
               codeend <= code2 + RANGE_OF_SHORT_CONDITIONAL_JUMP);
  BEGIN_CODE
  if (IS_A_SINGLE_JUMP(code2, codeend, target))
    FAR_COND_JUMP_TO(target, condition);  /* replace a jump with a cond jump */
  else
    { /* other cases: write a short cond jump to skip the block if !condition */
      SHORT_COND_JUMP_TO(codeend, INVERT_CC(condition));
      code = codeend;
    }
  END_CODE
}


#endif /* _IDISPATCHER_H */
