#ifndef _IENCODING_H
#define _IENCODING_H

#include "../psyco.h"
#define MACHINE_CODE_FORMAT    "x64"

typedef enum {
        REG_386_EAX    = 0,
        REG_386_ECX    = 1,
        REG_386_EDX    = 2,
        REG_386_EBX    = 3,
        REG_386_ESP    = 4,
        REG_386_EBP    = 5,
        REG_386_ESI    = 6,
        REG_386_EDI    = 7,
#define REG_TOTAL        8

        REG_NONE       = -1} reg_t;

/* Condition Code aka Processor Flags
 * The current code is a bit confused 
 * about whether all backends have 
 * a CPU status register
 * I think it should assume all CPUs
 * do for now.
 * This really needs cleaning up!
 */
typedef enum {
        CC_O         = 0,    /* overflow */
        CC_NO        = 1,
        CC_B         = 2,    /* below (unsigned) */
        CC_NB        = 3,
        CC_E         = 4,    /* equal */
        CC_NE        = 5,
        CC_BE        = 6,    /* below or equal (unsigned) */
        CC_NBE       = 7,
        CC_S         = 8,    /* sign (i.e. negative) */
        CC_NS        = 9,
        CC_L         = 12,   /* lower than */
        CC_NL        = 13,
        CC_LE        = 14,   /* lower or equal */
        CC_NLE       = 15,
#define CC_TOTAL       16
/* synonyms */
        CC_NGE       = CC_L,   /* not greater or equal */
        CC_GE        = CC_NL,  /* greater or equal */
        CC_NG        = CC_LE,  /* not greater than */
        CC_G         = CC_NLE, /* greater than */
        CC_uL        = CC_B,     /* unsigned test */
        CC_uNL       = CC_NB,    /* unsigned test */
        CC_uLE       = CC_BE,    /* unsigned test */
        CC_uNLE      = CC_NBE,   /* unsigned test */
        CC_uNGE      = CC_uL,    /* unsigned test */
        CC_uGE       = CC_uNL,   /* unsigned test */
        CC_uNG       = CC_uLE,   /* unsigned test */
        CC_uG        = CC_uNLE,  /* unsigned test */
        CC_ALWAYS_FALSE  = 16,   /* pseudo condition codes for known outcomes */
        CC_ALWAYS_TRUE   = 17,
        CC_ERROR         = -1 } condition_code_t;

#define SET_RUNTIME_REG_TO_NONE(vi)     ((vi)->source =                         \
                                                 set_rtreg_to_none((vi)->source))
#define RSOURCE_REG_IS_NONE(src) is_reg_none(src)
#define RSOURCE_REG(src)         getreg(src)
#define REG_ANY_CALLER_SAVED       REG_386_EAX  /* just any "trash" register */
#define REG_NUMBER(po, rg)      ((po)->reg_array[(int)(rg)])
#define INVERT_CC(cc)    ((condition_code_t)((int)(cc) ^ 1))
#define HAVE_CCREG       2
#define INDEX_CC(cc)     ((int)(cc) & 1)
#define HAS_CCREG(po)    ((po)->ccregs[0] != NULL || (po)->ccregs[1] != NULL)
#define SAVE_REG_VINFO(vi, rg) /* Nothing */
/* END */
#define ALIGN_NO_FILL() /* Nothing */

#define PROCESSOR_PSYCOOBJECT_FIELDS \
  int stack_depth;         /* the size of data currently pushed in the stack */ \
  vinfo_t* reg_array[REG_TOTAL];   /* the 'vinfo_t' currently stored in regs */ \
  vinfo_t* ccregs[2];              /* processor condition codes (aka flags)  */ \
                                   /* ccregs[0] is for "positive" conditions */ \
                                   /* and ccregs[1] for "negative" ones      */ \
  reg_t last_used_reg;             /* the most recently used register        */
#define PROCESSOR_FROZENOBJECT_FIELDS
#define SAVE_PROCESSOR_FROZENOBJECT(fpo, po) 
#define RESTORE_PROCESSOR_FROZENOBJECT(fpo, po)
#define insn_code_label(code) ((code_t*)(code))
#define INIT_CODE_EMISSION(code) /* nothing */


#define ABOUT_TO_CALL_SUBFUNCTION(finfo) /* nothing */
#define BINARY_INSTR_ADD(ovf, nonneg) NULL
#define BINARY_INSTR_AND(ovf, nonneg) NULL 
#define BINARY_INSTR_CMP(base_py_op)  CC_ALWAYS_TRUE
#define BINARY_INSTR_COND(cc, i1, i2) NULL
#define BINARY_INSTR_LSHIFT(  nonneg) NULL 
#define BINARY_INSTR_MUL(ovf, nonneg) NULL 
#define BINARY_INSTR_OR( ovf, nonneg) NULL 
#define BINARY_INSTR_RSHIFT(  nonneg) NULL
#define BINARY_INSTR_SUB(ovf, nonneg) NULL 
#define BINARY_INSTR_XOR(ovf, nonneg) NULL
#define CALL_C_FUNCTION(target, nb_args)   
#define CALL_SET_ARG_FROM_ADDR(source, arg_index, nb_args) 
#define CALL_SET_ARG_FROM_RT(source, arg_index, nb_args)  
#define CALL_SET_ARG_IMMED(immed, arg_index, nb_args)    
#define CALL_STACK_ALIGN(nbargs) 
#define CALL_STACK_ALIGN(nbargs)
#define CHECK_NONZERO_FROM_RT(source, rcc)
#define FUNCTION_RET(popbytes)
#define NEED_CC()
#define RETURNED_FROM_SUBFUNCTION()
#define SAVE_REGS_FN_CALLS(cc) 
#define STACK_CORRECTION(stack_correction)
#define UNARY_INSTR_ABS(ovf,  nonneg) NULL
#define UNARY_INSTR_INV(ovf,  nonneg) NULL
#define UNARY_INSTR_NEG(ovf,  nonneg) NULL
EXTERNFN code_t* psyco_compute_cc(PsycoObject* po, code_t* code, reg_t reserved);
EXTERNFN vinfo_t* bint_add_i(PsycoObject* po, vinfo_t* rt1, long value2,
                                     bool unsafe);
EXTERNFN vinfo_t* bint_mul_i(PsycoObject* po, vinfo_t* v1, long value2,
                                     bool ovf);
EXTERNFN vinfo_t* bint_lshift_i(PsycoObject* po, vinfo_t* v1, int counter);
EXTERNFN vinfo_t* bint_rshift_i(PsycoObject* po, vinfo_t* v1, int counter);
EXTERNFN vinfo_t* bint_urshift_i(PsycoObject* po, vinfo_t* v1, int counter);
EXTERNFN condition_code_t bint_cmp_i(PsycoObject* po, int base_py_op,
                                             vinfo_t* rt1, long immed2);
EXTERNFN vinfo_t* bfunction_result(PsycoObject* po, bool ref);

/* release a run-time vinfo_t */
#define RTVINFO_RELEASE(rtsource)         do {          \
  if (!RSOURCE_REG_IS_NONE(rtsource))                   \
    REG_NUMBER(po, RSOURCE_REG(rtsource)) = NULL;       \
} while (0)

/* move a run-time vinfo_t */
#define RTVINFO_MOVE(rtsource, vtarget)   do {          \
  if (!RSOURCE_REG_IS_NONE(rtsource))                   \
    REG_NUMBER(po, RSOURCE_REG(rtsource)) = (vtarget);  \
} while (0)

/* for PsycoObject_Duplicate() */
#define DUPLICATE_PROCESSOR(result, po)   do {          \
  int i;                                                \
  for (i=0; i<REG_TOTAL; i++)                           \
    if (REG_NUMBER(po, i) != NULL)                      \
      REG_NUMBER(result, i) = REG_NUMBER(po, i)->tmp;   \
  for (i=0; i<2; i++)                                   \
    if (po->ccregs[i] != NULL)                          \
      result->ccregs[i] = po->ccregs[i]->tmp;           \
                                                        \
  result->stack_depth = po->stack_depth;                \
  result->last_used_reg = po->last_used_reg;            \
} while (0)
#define INIT_PROCESSOR_PSYCOOBJECT(po) /* Nothing */
#define SIZE_OF_FAR_JUMP                   5
#define MAXIMUM_SIZE_OF_FAR_JUMP    SIZE_OF_FAR_JUMP

#define JUMP_TO(addr)
#define CHECK_STACK_SPACE() 

#endif
