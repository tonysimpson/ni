 /***************************************************************/
/***       Processor-specific routines for dispatcher.c        ***/
 /***************************************************************/

#ifndef _IDISPATCHER_H
#define _IDISPATCHER_H

#include "../vcompiler.h"
#include "../codegen.h"
#include "iencoding.h"
#include "ivm-insns.h"

#define NEED_STACK_FRAME_HACK    0


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

#define PROMOTION_FAST_COMMON_CASE   0   /* not implemented for ivm */
#define INTERNAL_PROMOTION_FIELDS    /* nothing */


inline code_t* fix_fast_common_case(void* fs, long value, code_t* codeptr)
{
	return codeptr;
}

inline void* ipromotion_finish(PsycoObject* po, vinfo_t* fix, void* do_promotion)
{
	return psyco_call_code_builder(po, do_promotion, 0, fix->source);
}


/***************************************************************/
 /***   Misc.                                                 ***/

inline void* conditional_jump_to(PsycoObject* po, code_t* target,
				 condition_code_t condition)
{
	word_t* arg = NULL;
	BEGIN_CODE
	switch (condition) {
	case CC_ALWAYS_FALSE:          /* never jumps */
		break;
	case CC_ALWAYS_TRUE:
		INSN_jumpfar(&arg);            /* always jumps */
		*arg = (word_t) target;
		break;
	default:
		INSN_rtcc_push(condition);
		INSN_jcondfar(&arg);
		*arg = (word_t) target;
	}
	END_CODE
	return arg;
}

inline void change_cond_jump_target(void* tag, code_t* newtarget)
{
	word_t* arg = (word_t*)tag;
	*arg = (word_t) newtarget;
}

/* reserve a small buffer of code behind po->code in which conditional
   code can be stored.  That code should only be executed if 'condition'. */
inline void* setup_conditional_code_bounds(PsycoObject* po, PsycoObject* po2,
					   condition_code_t condition)
{
	code_t* forward_distance_ptr;
	BEGIN_CODE
	INSN_rtcc_push(INVERT_CC(condition));
	INSN_jcondnear(&forward_distance_ptr);
	po2->code = code;
	po2->codelimit = code + 255;
	END_CODE
	return forward_distance_ptr;
}

/* Backpatch the distance over which to skip the conditional code. */
inline void make_code_conditional(PsycoObject* po, code_t* codeend,
                                  condition_code_t condition, void* extra)
{
	code_t* forward_distance_ptr = (code_t*) extra;
        code_t* code = codeend;
        int distance = INSN_CODE_LABEL() - po->code;
        po->code = code;
	extra_assert(0 <= distance && distance <= 255);
	*forward_distance_ptr = (code_t) distance;
}


#endif /* _IDISPATCHER_H */
