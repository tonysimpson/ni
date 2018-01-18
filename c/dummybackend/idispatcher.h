#ifndef _IDISPATCHER_H
#define _IDISPATCHER_H

#include "../vcompiler.h"

#define NEED_STACK_FRAME_HACK    1
typedef void (*fz_find_fn) (vinfo_t* a, RunTimeSource bsource, void* extra);
static void* conditional_jump_to(PsycoObject* po, code_t* target, condition_code_t condition)
{
    return po->code;
}

static void change_cond_jump_target(void* tag, code_t* newtarget)
{
}

static code_t* resume_after_cond_jump(void* tag)
{
    return (code_t*)tag;
}

#define PROMOTION_FAST_COMMON_CASE   0   /* not implemented for ivm */
#define INTERNAL_PROMOTION_FIELDS    /* nothing */

PSY_INLINE code_t* fix_fast_common_case(void* fs, long value, code_t* codeptr)
{
        return NULL;
}

PSY_INLINE void* ipromotion_finish(PsycoObject* po, vinfo_t* fix, void* do_promotion)
{
        return NULL;
}
/* reserve a small buffer of code behind po->code in which conditional
   code can be stored.  See make_code_conditional(). */
PSY_INLINE void* setup_conditional_code_bounds(PsycoObject* po, PsycoObject* po2,
                                           condition_code_t condition)
{
  return NULL;
}

/* mark a small buffer reserved by setup_conditional_code_bounds() to be
   only executed if 'condition' holds. */
PSY_INLINE void make_code_conditional(PsycoObject* po, code_t* codeend,
                                  condition_code_t condition, void* extra)
{
}
#endif
