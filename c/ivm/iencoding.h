 /***************************************************************/
/***       Processor-specific code-producing macros            ***/
 /***************************************************************/

#ifndef _IENCODING_H
#define _IENCODING_H


#include "../psyco.h"
#include "ivm-insns.h"


#define REG_TOTAL   0   /* the virtual machine has only a stack */

typedef enum {
	CC_TOS           = 0,   /* flag is on the top of the stack */
	CC_NEG_TOS       = 1,   /* negation of CC_TOS */
#define CC_TOTAL       2
        CC_ALWAYS_FALSE  = 2,   /* pseudo condition codes for known outcomes */
        CC_ALWAYS_TRUE   = 3,
        CC_ERROR         = -1 } condition_code_t;


/* processor-depend part of PsycoObject */
#define PROCESSOR_PSYCOOBJECT_FIELDS                                            \
	int stack_depth;   /* the size of data currently pushed in the stack */ \
	vinfo_t* ccreg;            /* processor condition codes (aka flags)  */


/* release a run-time vinfo_t */
#define RTVINFO_RELEASE(rtsource)       do {				\
	/* pop an item off the stack only if it is close to the top */	\
	switch (current_stack_position(rtsource)) {			\
	case 0:								\
		insn_pop();						\
		break;							\
	case 1:								\
		insn_pop2nd();						\
		break;							\
	default:							\
		break;  /* not removed */				\
	}								\
} while (0)

/* move a run-time vinfo_t */
#define RTVINFO_MOVE(rtsource, vtarget)   do { /*nothing*/ } while (0)

/* for PsycoObject_Duplicate() */
#define DUPLICATE_PROCESSOR(result, po)   do {	\
	if (po->ccreg != NULL)			\
		result->ccreg = po->ccreg->tmp;	\
	result->stack_depth = po->stack_depth;	\
} while (0)

#define RTVINFO_CHECK(po, vsource, found) do { /*nothing*/ } while (0)
#define RTVINFO_CHECKED(po, found)        do { /*nothing*/ } while (0)


/***************************************************************/
 /***   some macro for code emission                          ***/

#define ALIGN_PAD_CODE_PTR()     do { /*nothing*/ } while (0)
#define ALIGN_WITH_BYTE(byte)    do { /*nothing*/ } while (0)
#define ALIGN_WITH_NOP()         do { /*nothing*/ } while (0)
#define ALIGN_NO_FILL()          do { /*nothing*/ } while (0)


#endif /* _IENCODING_H */
