 /***************************************************************/
/***       Processor-specific code-producing macros            ***/
 /***************************************************************/

#ifndef _IENCODING_H
#define _IENCODING_H


#include "../psyco.h"
#define MACHINE_CODE_FORMAT    "86x64"
#define HAVE_FP_FN_CALLS       1


 /* set to 0 to emit code that runs on 386 and 486 */
#ifndef PENTIUM_INSNS
# define PENTIUM_INSNS    1
#endif

 /* Define to 1 to always write the most compact encoding of instructions.
    (a quite minor overhead). Set to 0 to disable. No effect on real
    optimizations. */
#ifndef COMPACT_ENCODING
#ifdef __APPLE__
/* COMPACT_ENCODING not yet supported on MacOS X */
# define COMPACT_ENCODING   0
#else
# define COMPACT_ENCODING   1
#endif
#endif

#define CHECK_STACK_DEPTH 1

/* Define to 0 to use RBP as any other register, or to 1 to reserve it 
 * useful for debugging to set this to 1 */
#ifndef RBP_IS_RESERVED
# define RBP_IS_RESERVED 0
#endif

/* Set to 0 to limit registers to RAX>RDI */

#define REG_TOTAL    16
typedef enum {
        REG_X64_RAX = 0, /* reserved for return values */
        REG_X64_RCX = 1, /* 4th ARG */
        REG_X64_RDX = 2, /* 3rd ARG */
        REG_X64_RBX = 3, /* CALLEE Saved */
        REG_X64_RSP = 4, /* stack pointer */
        REG_X64_RBP = 5, /* CALLEE Saved (special see RBP_IS_RESERVED) */
        REG_X64_RSI = 6, /* 2nd ARG */
        REG_X64_RDI = 7, /* 1st ARG */
        REG_X64_R8  = 8, /* 5th ARG */
        REG_X64_R9  = 9, /* 6th ARG */
        REG_X64_R10 = 10,/* CALLER Saved */   
        REG_X64_R11 = 11,/* CALLER Saved */
        REG_X64_R12 = 12,/* CALLEE Saved */
        REG_X64_R13 = 13,/* CALLEE Saved */
        REG_X64_R14 = 14,/* CALLEE Saved */
        REG_X64_R15 = 15,/* CALLEE Saved */
        REG_NONE    = -1} reg_t;

#define REG_LOOP_START REG_X64_R13

#define REG_ANY_CALLEE_SAVED       REG_X64_RBX  /* saved by C functions */
#define REG_FUNCTIONS_RETURN       REG_X64_RAX  
#define REG_TRANSIENT_1            REG_X64_R10
#define REG_TRANSIENT_2            REG_X64_R11

/* the registers we want Psyco to use in compiled code,
 *    as a circular linked list */
EXTERNVAR reg_t RegistersLoop[REG_TOTAL];

/* returns the next register that should be used */
#define next_free_reg(po)	\
	((po)->last_used_reg = RegistersLoop[(int)((po)->last_used_reg)])

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

#define INVERT_CC(cc)    ((condition_code_t)((int)(cc) ^ 1))
#define HAVE_CCREG       2
#define INDEX_CC(cc)     ((int)(cc) & 1)
#define HAS_CCREG(po)    ((po)->ccregs[0] != NULL || (po)->ccregs[1] != NULL)




/* processor-depend part of PsycoObject */
#define PROCESSOR_PSYCOOBJECT_FIELDS                                            \
  int stack_depth;         /* the size of data currently pushed in the stack */ \
  vinfo_t* reg_array[REG_TOTAL];   /* the 'vinfo_t' currently stored in regs */ \
  vinfo_t* ccregs[2];              /* processor condition codes (aka flags)  */ \
                                   /* ccregs[0] is for "positive" conditions */ \
                                   /* and ccregs[1] for "negative" ones      */ \
  reg_t last_used_reg;             /* the most recently used register        */
#define INIT_PROCESSOR_PSYCOOBJECT(po)                                          \
  (po->last_used_reg = REG_LOOP_START)

#define PROCESSOR_FROZENOBJECT_FIELDS                   \
  short fz_last_used_reg;
#define SAVE_PROCESSOR_FROZENOBJECT(fpo, po)            \
  (fpo->fz_last_used_reg = (int) po->last_used_reg)
#define RESTORE_PROCESSOR_FROZENOBJECT(fpo, po)         \
  (po->last_used_reg = (reg_t) fpo->fz_last_used_reg)

#define INIT_CODE_EMISSION(code)         do { /* nothing */ } while (0)
#define POST_CODEBUFFER_SIZE             0
#define insn_code_label(code)            ((code_t*)(code))

#define CHECK_STACK_SPACE()              do { /* nothing */ } while (0)


/*****************************************************************/
 /***   Production of code (common instruction encodings)       ***/

/* Most of the following macros implicitely use and update the
 * local variable 'code'. Some also use 'po'. No macro outside the
 * present header file must implicitely use or modify 'code'.
 */


/* Written as a large set of macro. */

#define RSOURCE_REG(src)         getreg(src)
#define RSOURCE_REG_IS_NONE(src) is_reg_none(src)
#define RSOURCE_STACK(src)       getstack(src)

#define RUNTIME_STACK_MAX        RunTime_StackMax
#define RUNTIME_STACK_NONE       RunTime_StackNone
#define RUNTIME_REG(vi)	         RSOURCE_REG         ((vi)->source)
#define RUNTIME_REG_IS_NONE(vi)	 RSOURCE_REG_IS_NONE ((vi)->source)
#define RUNTIME_STACK(vi)        RSOURCE_STACK       ((vi)->source)

#define SET_RUNTIME_REG_TO(vi, rg)      ((vi)->source =                         \
                                         set_rtreg_to((vi)->source, rg))
#define SET_RUNTIME_REG_TO_NONE(vi)     ((vi)->source =                         \
                                         set_rtreg_to_none((vi)->source))
#define SET_RUNTIME_STACK_TO(vi, s)     ((vi)->source =                         \
                                         set_rtstack_to((vi)->source, (s)))
#define SET_RUNTIME_STACK_TO_NONE(vi)   ((vi)->source =                         \
                                         set_rtstack_to_none((vi)->source))

#define KSOURCE_SOURCE(src)     CompileTime_Get(src)
#define KNOWN_SOURCE(vi)        KSOURCE_SOURCE((vi)->source)

#define NEXT_FREE_REG()         next_free_reg(po)
#define REG_NUMBER(po, rg)      ((po)->reg_array[(int)(rg)])


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

#define RTVINFO_CHECK(po, vsource, found) do {                          \
  RunTimeSource _src = (vsource)->source;                               \
  if (!RSOURCE_REG_IS_NONE(_src))                                       \
    {                                                                   \
      extra_assert(REG_NUMBER(po, RSOURCE_REG(_src)) == (vsource));     \
      found[(int) RSOURCE_REG(_src)] = 1;                               \
    }                                                                   \
} while (0)
#define RTVINFO_CHECKED(po, found)        do {  \
  int i;                                        \
  for (i=0; i<REG_TOTAL; i++)                   \
    if (!found[i])                              \
      extra_assert(REG_NUMBER(po, i) == NULL);  \
} while (0)

/*****************************************************************
 * Low level code producing macros
 *
 * All codegen should be done via these
 *
 * REX will include the REX byte before opcode
 * MODRM will include the MODRM byte after opcode
 * U   - Prefix for updatable (use #define UPDATE_CODE ... #undef UPDATE_CODE
 * REG - MODRM REG or just reg encoded in opcode (REX.R)
 * RM  - MODRM RM or just reg encoded in opcode (REX.B)
 * A   - MODRM RM with indirect addressing (reg is pointer to address)
 * O8  - MODRM RM with indirect addressing and 8bit offset
 * O32 - MODRM RM with indirect addressing and 32bit offset
 * I8  - Immediate 8bit
 * I32 - Immediate 32bit
 * I64 - Immediate 64bit
 */
#define FITS_IN_8BITS(v) (((v) < 128 && (v) >= -128))
#define FITS_IN_32BITS(v) (((v) < 2147483648 && (v) >= -2147483648))
#define ONLY_UPDATING 0 /* redefine to 1 when updating code */
#define REX_64 0x48
#define REX_64_REG(reg) (REX_64 | (reg > 7 ? 4 : 0))
#define REX_64_RM(rm) (REX_64 | (rm > 7 ? 1 : 0))
#define REX_64_REG_RM(reg, rm) (0x48 | (reg > 7 ? 4 : 0) | (rm > 7 ? 1 : 0))
#define REX_64_REG_X_B(reg, index, base) (REX_64_REG_RM(reg, base) | (index > 7 ? 2 : 0))
#define REG_IN_OPCODE(opcode, reg) (opcode | (reg & 0x7))
#define MODRM_GENERAL(type_code, reg, rm) (type_code | ((reg & 0x7) << 3) | (rm & 0x7))
#define MODRM_REG_A(reg, rm) MODRM_GENERAL(0x0, reg, rm) 
#define MODRM_REG_O8(reg, rm) MODRM_GENERAL(0x40, reg, rm)
#define MODRM_REG_O32(reg, rm) MODRM_GENERAL(0x80, reg, rm)
#define MODRM_REG_RM(reg, rm) MODRM_GENERAL(0xC0, reg, rm)
#define WRITE_8BIT(immed) do {\
if(!ONLY_UPDATING) {\
    *code = (code_t)immed;\
}\
    code += 1;\
} while(0)
#define WRITE_32BIT(immed) do {\
if(!ONLY_UPDATING) {\
    *(dword_t*)code = (dword_t)immed;\
}\
    code += sizeof(dword_t);\
} while(0)
#define WRITE_64BIT(immed) do {\
if(!ONLY_UPDATING) {\
  *(qword_t*)code = (qword_t)immed;\
}\
  code += sizeof(qword_t);\
} while(0)
#define UPDATABLE_WRITE_8BIT(immed) do {\
    *code = (code_t)immed;\
    code += 1;\
} while(0)
#define UPDATABLE_WRITE_32BIT(immed) do {\
    *(dword_t*)code = (dword_t)immed;\
    code += sizeof(dword_t);\
} while(0)
#define UPDATABLE_WRITE_64BIT(immed) do {\
  *(qword_t*)code = (qword_t)immed;\
  code += sizeof(qword_t);\
} while(0)
#define WRITE_1(b1) do {\
if(!ONLY_UPDATING) {\
    code[0] = b1;\
}\
    code += 1;\
} while(0)
#define WRITE_2(b1, b2) do {\
if(!ONLY_UPDATING) {\
    code[0] = b1;\
    code[1] = b2;\
}\
    code += 2;\
} while(0)
#define WRITE_3(b1, b2, b3) do {\
if(!ONLY_UPDATING) {\
    code[0] = b1;\
    code[1] = b2;\
    code[2] = b3;\
}\
    code += 3;\
} while(0)
#define WRITE_4(b1, b2, b3, b4) do {\
if(!ONLY_UPDATING) {\
    code[0] = b1;\
    code[1] = b2;\
    code[2] = b3;\
    code[3] = b4;\
}\
    code += 4;\
} while(0)
#define OP_REX_64_RM(opcode, reg) WRITE_2(REX_64_RM(reg), REG_IN_OPCODE(opcode, reg))
#define OP_REX_64_RM_I64(opcode, reg, immed) do {\
    WRITE_2(REX_64_RM(reg), REG_IN_OPCODE(opcode, reg));\
    WRITE_64BIT(immed);\
} while(0)
#define OP_REX_64_RM_UI64(opcode, reg, immed) do {\
    WRITE_2(REX_64_RM(reg), REG_IN_OPCODE(opcode, reg));\
    UPDATABLE_WRITE_64BIT(immed);\
} while(0)
#define OP_REX_64_MODRM_REG_RM(opcode, reg, rm) WRITE_3(\
        REX_64_REG_RM(reg, rm), \
        opcode, \
        MODRM_REG_RM(reg, rm))
#define OP_REX_64_MODRM_REG_A(opcode, reg, rm) WRITE_3(\
        REX_64_REG_RM(reg, rm), \
        opcode, \
        MODRM_REG_A(reg, rm))
#define OP_REX_64_MODRM_REG_O8(opcode, reg, rm, offset) WRITE_4(\
        REX_64_REG_RM(reg, rm), \
        opcode, \
        MODRM_REG_O8(reg, rm),\
        offset)
#define OP_REX_64_MODRM_REG_O32(opcode, reg, rm, offset) do {\
    WRITE_3(REX_64_REG_RM(reg, rm), opcode, MODRM_REG_O32(reg, rm));\
    WRITE_32BIT(offset);\
} while(0)
#define OP_REX_64_MODRM_REG_UO32(opcode, reg, rm, offset) do {\
    WRITE_3(REX_64_REG_RM(reg, rm), opcode, MODRM_REG_O32(reg, rm));\
    UPDATABLE_WRITE_32BIT(offset);\
} while(0)
/* Common Instructions:
 * U - Prefix for updatable fields
 * R - Register direct
 * A - Address in register
 * O8 - Address is register + 8 bit offset
 * O32 - Address is register + 32 bit offset
 * I - Immediat
 * X - base + (index * scale) + offset
 */
#define REX_ENCODING(rex_w, r, i, rm) do {\
    if(rex_w || r > 7 || rm > 7) {\
        *code++ = 0x40 | (rex_w ? 8 : 0) | (r > 7 ? 4 : 0) | (i > 7 ? 2 : 0) |(rm > 7 ? 1 : 0);\
    }\
} while (0)
#define MODRM_ENCODING(mod, r, rm) do {\
    *code++ = mod | ((r & 7) << 3) | (rm & 7);\
} while (0)
#define ENCODE_OFFSET(updatable, size, value) do {\
    if (updatable) {\
        if (size == 8) {\
            UPDATABLE_WRITE_8BIT(value);\
        } else {\
            UPDATABLE_WRITE_32BIT(value);\
        }\
    } else {\
        if (size == 8) {\
            WRITE_8BIT(value);\
        } else {\
            WRITE_32BIT(value);\
        }\
    }\
} while (0)
#define SIB_ENCODING(updatable, rex_w, opcode, mod, size, r, base, offset, index, scale) do {\
    REX_ENCODING(rex_w, r, index, base);\
    *code++ = (opcode);\
    MODRM_ENCODING(mod | (offset == 0 ? 0x04 : (size == 8 ? 0x44 : 0x84)), r, 0);\
    *code++ = (scale == 8 ? 0xC0 : scale == 4 ? 0x80 : scale == 2 ? 0x40 : 0x00) | (base & 7) | ((index & 7) << 3);\
    if (offset != 0 || (base == REG_X64_RSP && mod != 0x38)) {\
        ENCODE_OFFSET(updatable, size, offset);\
    }\
} while (0)
#define OFFSET_ENCODING(updatable, rex_w, opcode, mod, size, r, rm, offset) do {\
    if((rm & 7) == 4) {\
        SIB_ENCODING(updatable, rex_w, opcode, mod, size, r, rm, offset, rm, 0);\
    } else {\
        REX_ENCODING(rex_w, r, 0, rm);\
        *code++ = (opcode);\
        MODRM_ENCODING(mod | (size == 8 ? 0x40 : 0x80), r, rm);\
        ENCODE_OFFSET(updatable, size, offset);\
    }\
} while(0)
#define INDIRECT_ENCODING(rex_w, opcode, mod, r, rm) do {\
    if((rm & 7) == 4 || (rm & 7) == 5) {\
        OFFSET_ENCODING(false, rex_w, opcode, mod, 8, r, rm, 0);\
    } else {\
        REX_ENCODING(rex_w, r, 0, rm);\
        *code++ = (opcode);\
        MODRM_ENCODING(mod, r, rm);\
    }\
} while (0)
#define DIRECT_ENCODING(rex_w, opcode, mod, r, rm) do {\
    REX_ENCODING(rex_w, r, 0, rm);\
    *code++ = (opcode);\
    MODRM_ENCODING(mod | 0xC0, r, rm);\
} while (0)
#define BRKP() WRITE_1(0xCC)
#define MOV_R_R(r, rm) DIRECT_ENCODING(true, 0x8B, 0, r, rm)
#define MOV_R_A(r, rm) INDIRECT_ENCODING(true, 0x8B, 0, r, rm)
#define MOV_A_R(rm, r) INDIRECT_ENCODING(true, 0x89, 0, r, rm)
#define MOV_R_I(rm, i) REX_ENCODING(true, 0, 0, rm); *code++ = (0xB8 | (rm & 7)); WRITE_64BIT(i)
#define MOV_R_UI(rm, i) REX_ENCODING(true, 0, 0, rm); *code++ = (0xB8 | (rm & 7)); UPDATABLE_WRITE_64BIT(i)
#define MOV_R_O8(r, rm, o) OFFSET_ENCODING(false, true, 0x8B, 0, 8, r, rm, o)
#define MOV_R_O32(r, rm, o) OFFSET_ENCODING(false, true, 0x8B, 0, 32, r, src, o)
#define MOV_R_O(r, rm, o) OFFSET_ENCODING(false, true, 0x8B, 0, (FITS_IN_8BITS(o) ? 8 : 32), r, rm, o)
#define MOV_O_R(rm, o, r) OFFSET_ENCODING(false, true, 0x89, 0, (FITS_IN_8BITS(o) ? 8 : 32), r, rm, o)
#define LEA_R_O8(r, rm, o) OFFSET_ENCODING(false, true, 0x8D, 0, 8, r, rm, o)
#define LEA_R_O(r, rm, o) OFFSET_ENCODING(false, true, 0x8D, 0, (FITS_IN_8BITS(o) ? 8 : 32), r, rm, o)
#define MOV_R_X8(r, base, offset, index, scale) SIB_ENCODING(false, true, 0x8B, 0, 8, r, base, offset, index, scale)
#define MOV_R_X32(r, base, offset, index, scale) SIB_ENCODING(false, true, 0x8B, 0, 32, r, base, offset, index, scale) 
#define XCHG_R_R(rg1, rg2) do {\
  if(rg1 == REG_X64_RAX) {\
      OP_REX_64_RM(0x90, rg2);\
  } else if(rg2 == REG_X64_RAX) {\
      OP_REX_64_RM(0x90, rg1);\
  } else {\
      DIRECT_ENCODING(true, 0x87, 0, rg1, rg2);\
  }\
} while (0)
#define XCHG_R_O(r, rm, o) OFFSET_ENCODING(false, true, 0x87, 0, (FITS_IN_8BITS(o) ? 8 : 32), r, rm, o)
#define TEST_R_R(r, rm) DIRECT_ENCODING(true, 0x85, 0, r, rm)
#define CMP_R_R(r, rm) DIRECT_ENCODING(true, 0x39, 0, r, rm)
#define CMP_R_A(r, rm) INDIRECT_ENCODING(true, 0x39, 0, r, rm)
#define CMP_R_O8(r, rm, o) OFFSET_ENCODING(false, true, 0x39, 0, 8, r, rm, o)
#define CMP_I8_O(i, rm, o) OFFSET_ENCODING(false, false, 0x83, 0x78, (FITS_IN_8BITS(o) ? 8 : 32), 0, rm, o); WRITE_8BIT(i)
#define CMP_I8_A(i, rm) INDIRECT_ENCODING(false, 0x83, 0x38, 0, rm); WRITE_8BIT(i)
#define CMP_R_UO32(r, rm, o) OFFSET_ENCODING(true, true, 0x39, 0, 32, r, rm, o)
#define CMP_I_R(i1, r2) do {\
    if(i1 >= -128 && i1 < 128) {\
        WRITE_4(REX_64_RM(r2), 0x83, REG_IN_OPCODE(0xF8, r2), (code_t)(i1));\
    }else {\
        MOV_R_I(REG_TRANSIENT_1, (i1));\
        CMP_R_R(REG_TRANSIENT_1, (r2));\
    }\
} while (0)
#define OP_VREX_VMOD_R(mod, reg) do {\
    if(reg <= 7) {\
        WRITE_2(0xFF, mod | reg);\
    } else {\
        WRITE_3(0x41, 0XFF, mod | (reg & 0x7));\
    }\
} while(0)
#define OP_VREX_R(opcode, reg) do {\
    if(reg <= 7) {\
        WRITE_1(opcode | reg);\
    } else {\
        WRITE_2(0x41, opcode | (reg & 0x7));\
    }\
} while(0)
#define JMP_R(r) OP_VREX_VMOD_R(0xE0, r)
#define JMP_CC_UI32(cc, address) WRITE_2(0x0F, 0x80 | (cc)); UPDATABLE_WRITE_32BIT((address) - ((long)code + 4))
#define CALL_R(r) OP_VREX_VMOD_R(0xD0, r)
#define CALL_I(i) do {\
    long jump_amount = (long)(i) - ((long)code + 5);\
    if(FITS_IN_32BITS(jump_amount)) {\
        WRITE_1(0xE8);\
        WRITE_32BIT(jump_amount);\
    } else {\
        MOV_R_I(REG_TRANSIENT_1, i);\
        CALL_R(REG_TRANSIENT_1);\
    }\
} while (0)
#define ADD_A_I8(rm, i) INDIRECT_ENCODING(false, 0x83, 0, 0, rm); WRITE_8BIT(i)
#define ADD_O8_I8(rm, o, i) OFFSET_ENCODING(false, false, 0x83, 0, 8, 0, rm, o); WRITE_8BIT(i)
#define SUB_A_I8(rm, i) INDIRECT_ENCODING(false, 0x83, 0x28, 0, rm); WRITE_8BIT(i)
#define ADD_R_R(r, rm) DIRECT_ENCODING(true, 0x01, 0, r, rm);
#define SUB_R_I8(rm, i) DIRECT_ENCODING(true, 0x83, 0x28, 0, rm); WRITE_8BIT(i)
#define SUB_R_I32(rm, i) DIRECT_ENCODING(true, 0x81, 0x28, 0, rm); WRITE_32BIT(i)
#define ADD_R_I8(rm, i) DIRECT_ENCODING(true, 0x83, 0x0, 0, rm); WRITE_8BIT(i)
#define ADD_R_I32(rm, i) DIRECT_ENCODING(true, 0x81, 0x0, 0, rm); WRITE_32BIT(i)
#define IMUL_R_R(r1, r2) WRITE_4(REX_64_REG_RM(r1, r2), 0x0F, 0xAF, MODRM_REG_RM(r1, r2))
#define SET_R_CC(r, cc) do {\
    if(r <= 7) {\
        WRITE_3(0x0F, 90 | cc, MODRM_REG_RM(0, r));\
    } else {\
        WRITE_4(REX_64_RM(r), 0x0F, 90 | cc, MODRM_REG_RM(0, r));\
    }\
} while(0)

#define PUSH_A(rm) INDIRECT_ENCODING(false, 0xFF, 0x30, 0, rm)
#define PUSH_R(r) OP_VREX_R(0x50, r)
#define POP_R(r) OP_VREX_R(0x58, r)
#define PUSH_I(immed) do {\
    MOV_R_I(REG_TRANSIENT_1, immed);\
    PUSH_R(REG_TRANSIENT_1);\
} while(0)
#define PUSH_O(rm, o) OFFSET_ENCODING(false, false, 0xFF, 0x70, (FITS_IN_8BITS(o) ? 8 : 32), 0, rm, o)
#define RET() WRITE_1(0xC3)
#define PUSH_CC() do {\
    *code++ = 0x9C;\
    psyco_inc_stackdepth(po);\
} while (0)
#define POP_CC() do {\
    *code++ = 0x9D;\
    psyco_dec_stackdepth(po);\
} while (0)
/***********************************************************************/
/* vinfo based instructions */
#define V_MOV_N_A(vnew, vaddr) do {\
    reg_t addr_reg;\
    reg_t target_reg = NEXT_FREE_REG();\
    NEED_REGISTER(target_reg);\
    if(is_compiletime(vaddr->source)) {\
        addr_reg = REG_TRANSIENT_1;\
        MOV_R_I(addr_reg, CompileTime_Get(vaddr->source)->value);\
    } else {\
        addr_reg = getreg(vaddr->source);\
    }\
    MOV_R_A(target_reg, addr_reg);\
    vnew = vinfo_new(RunTime_New(target_reg, false, false));\
    REG_NUMBER(po, target_reg) = vnew;\
} while(0)
#define V_MOV_N_O8(vnew, vaddr, offset) do {\
    reg_t addr_reg;\
    reg_t target_reg = NEXT_FREE_REG();\
    NEED_REGISTER(target_reg);\
    if(is_compiletime(vaddr->source)) {\
        addr_reg = REG_TRANSIENT_1;\
        MOV_R_I(addr_reg, CompileTime_Get(vaddr->source)->value);\
    } else {\
        addr_reg = getreg(vaddr->source);\
    }\
    MOV_R_O8(target_reg, addr_reg, offset);\
    vnew = vinfo_new(RunTime_New(target_reg, false, false));\
    REG_NUMBER(po, target_reg) = vnew;\
} while(0)

/***********************************************************************/
#if CHECK_STACK_DEPTH
#define STACK_DEPTH_CHECK() do {\
    /* see glue_run_code for companion code */\
    /* if stack_depth <= 0 we're in glue code */\
    if(po->stack_depth > 0) {\
        PUSH_CC();\
        MOV_R_I(REG_TRANSIENT_1, po->stack_depth + sizeof(long));\
        ADD_R_R(REG_X64_RSP, REG_TRANSIENT_1);\
        CMP_R_A(REG_TRANSIENT_1, REG_TRANSIENT_1);\
        BEGIN_SHORT_COND_JUMP(0, CC_E);\
        BRKP();\
        END_SHORT_JUMP(0);\
        POP_CC();\
    }\
} while(0)
#else
#define STACK_DEPTH_CHECK() do { } while (0)
#endif
/**********************************************************************/
#define STACK_POS_OFFSET(stack_pos) (po->stack_depth - (stack_pos))

/* access stack by [ESP+n] where 'n' varies depending on the current ESP */
#define MODRM_EBP_BASE(middle, stack_pos)       do {                    \
  int _s_p = po->stack_depth - (stack_pos);                             \
  extra_assert(0 < (stack_pos) && (stack_pos) <= RUNTIME_STACK_MAX);    \
  extra_assert(0 <= _s_p);                                              \
  code[2] = 0x24;                                                       \
  if (COMPACT_ENCODING && _s_p == 0)                                    \
    {                                                                   \
      code[1] = 0x04 | (middle);                                        \
      code += 3;                                                        \
    }                                                                   \
  else if (COMPACT_ENCODING && _s_p < 128)                              \
    {                                                                   \
      code[1] = 0x44 | (middle);                                        \
      code[3] = _s_p;                                                   \
      code += 4;                                                        \
    }                                                                   \
  else                                                                  \
    {                                                                   \
      code[1] = 0x84 | (middle);                                        \
      *(long*)(code+3) = _s_p;                                          \
      code += 7;                                                        \
    }                                                                   \
} while (0)

/* Emit instruction 'opcode' having a mod/rm as its second byte.
   Insert 'middle' in the mod/rm. Let the mod/rm point to the given stack_pos. */
#define INSTR_EBP_BASE(opcode, middle, stack_pos)   do {        \
  code[0] = (opcode);                                           \
  MODRM_EBP_BASE(middle, stack_pos);                            \
} while (0)


/* note: the following macro starts writing at code+1 */
#define MODRM_FROM_RT(source, middle)   do {            \
  if (RSOURCE_REG_IS_NONE(source))                      \
    MODRM_EBP_BASE(middle, RSOURCE_STACK(source));      \
  else {  /* register source */                         \
    code[1] = 0xC0 | (middle) | RSOURCE_REG(source);    \
    code += 2;                                          \
  }                                                     \
} while (0)

/* Same as INSTR_EBP_BASE but reading from the 'source' of a run-time vinfo_t */
#define INSTR_MODRM_FROM_RT(source, opcode, middle)	do {    \
  code[0] = (opcode);                                           \
  MODRM_FROM_RT(source, middle);                                \
} while (0)


/* The "common instructions" groups: there are 8 arithmetic instructions
   whose encodings are identical. Here they are, with their 'group' value:
     ADD    (group 0)
     OR     (group 1)
     ADC    (group 2)
     SBB    (group 3)
     AND    (group 4)
     SUB    (group 5)
     XOR    (group 6)
     CMP    (group 7)  */

/* The following macro encodes  "INSTR register, immediate"  */
#define COMMON_INSTR_IMMED(group, rg, value) do {       \
  long _v;                                              \
  code[1] = 0xC0 | (group<<3) | (rg);                   \
  _v = value;                                           \
  if (COMPACT_ENCODING && -128 <= _v && _v < 128) {     \
    code[2] = (code_t) _v;                              \
    code[0] = 0x83;                                     \
    code += 3;                                          \
  }                                                     \
  else {                                                \
    *(long*)(code+2) = _v;                              \
    code[0] = 0x81;                                     \
    code += 6;                                          \
  }                                                     \
} while (0)

/* Encodes  "INSTR register, source"  for a run-time or compile-time vinfo_t */
#define COMMON_INSTR_FROM(group, rg, source)   do {                     \
  if (((source) & TimeMask) == RunTime)                                 \
    COMMON_INSTR_FROM_RT(group, rg, source);                            \
  else                                                                  \
    COMMON_INSTR_IMMED(group, rg, KSOURCE_SOURCE(source)->value);       \
} while(0)

/* Encodes  "INSTR register, source"  for a run-time vinfo_t */
#define COMMON_INSTR_FROM_RT(group, rg, source)    do { \
  code[0] = group*8 + 3;                                \
  MODRM_FROM_RT(source, (rg)<<3);                       \
} while(0)

/* Encodes "INSTR reg" for the following instructions:
     NOT    (group 2)
     NEG    (group 3)
     IDIV   (group 7) */
#define UNARY_INSTR_ON_REG(group, rg)              do { \
  code[0] = 0xF7;                                       \
  code[1] = 0xC0 | (group<<3) | (rg);                   \
  code += 2;                                            \
} while (0)

/* Encodes "INSTR source" for the same instructions as above */
#define UNARY_INSTR_FROM_RT(group, source)         do { \
  INSTR_MODRM_FROM_RT(0xF7, source, (group)<<3);        \
} while (0)

/* Encodes "INC rg" and "DEC rg" */
#define INCREASE_REG(rg)   ADD_R_I8(rg, 1) 
#define DECREASE_REG(rg)   SUB_R_I8(rg, 1) 

/* Encodes taking absolute value of the register 'rg' knowing that
   'sourcecopy' is a (run-time) copy of 'rg' */
#define INT_ABS(rg, sourcecopy)       do {                                      \
  /* as you can check the following takes the absolute value of (say) EAX:      \
       ADD EAX, EAX                                                             \
       SBB EAX, sourcecopy                                                      \
       SBB EDX, EDX                                                             \
       XOR EAX, EDX                                                             \
    (note: although the idea is not original, the above code might be           \
     original as it has been found by an exhaustive search on *all*             \
     short codes :-)                                                            \
  */                                                                            \
  reg_t _rg2;                                                                   \
  code[0] = 0x01;                                                               \
  code[1] = 0xC0 | ((rg)<<3) | (rg);   /* ADD rg, rg */                         \
  code += 2;                                                                    \
  COMMON_INSTR_FROM_RT(3, rg, sourcecopy);  /* SBB rg, sourcecopy */            \
  DELAY_USE_OF(rg);                                                             \
  NEED_FREE_REG(_rg2);                                                          \
  code[0] = 0x19;                                                               \
  code[1] = 0xC0 | (_rg2<<3) | _rg2;  /* SBB _rg2, _rg2 */                      \
  code[2] = 0x31;                                                               \
  code[3] = 0xC0 | (_rg2<<3) | (rg);  /* XOR rg, _rg2 */                        \
  code += 4;                                                                    \
} while (0)
#define CHECK_ABS_OVERFLOW   CC_S

/* Encodes a check (zero/non-zero) on the given 'source' */
#define CHECK_NONZERO_FROM_RT(source, rcc)        do {                          \
  NEED_CC_SRC(source);\
  if (RSOURCE_REG_IS_NONE(source)) {\
      CMP_I8_O(0, REG_X64_RSP, STACK_POS_OFFSET(RSOURCE_STACK(source)));\
  } else {\
    CHECK_NONZERO_REG(RSOURCE_REG(source));\
  }\
  rcc = CC_NE;  /* a.k.a. NZ flag */\
} while (0)
#define CHECK_NONZERO_REG(rg) TEST_R_R(rg, rg)

#define COMPARE_IMMED_FROM_RT(source, immed) CMP_I_R(immed, getreg(source))

/* Signed integer multiplications */
#define IMUL_REG_FROM_RT(source, rg)   do {             \
  *code++ = 0x0F;            /* IMUL rg, source */      \
  INSTR_MODRM_FROM_RT(source, 0xAF, (rg)<<3);           \
} while (0)

#define IMUL_IMMED_FROM_RT(source, immed, dstrg)   do {                 \
  long _value = (immed);                                                \
  code_t opcode = (COMPACT_ENCODING && -128 <= _value && _value < 128)  \
    ? 0x6B : 0x69;    /* IMUL dstrg, source, immed */                   \
  INSTR_MODRM_FROM_RT(source, opcode, (dstrg)<<3);                      \
  if (opcode == 0x69) {                                                 \
    *(long*)code = _value;                                              \
    code += 4;                                                          \
  }                                                                     \
  else                                                                  \
    *code++ = (code_t) _value;                                          \
} while (0)

/* Shitfs. The counters must never be >=32. */
#define SHIFT_GENERIC1(rg, cnt, middle)   do {  \
  code[1] = 0xC0 | ((middle)<<3) | (rg);        \
  if (COMPACT_ENCODING && cnt==1) {             \
    code[0] = 0xD1;                             \
    code += 2;                                  \
  }                                             \
  else {                                        \
    code[0] = 0xC1;                             \
    code[2] = (cnt);                            \
    code += 3;                                  \
  }                                             \
} while (0)
#define SHIFT_COUNTER    REG_X64_RCX  /* must be in range(0,32) */
#define SHIFT_GENERICCL(rg, middle)       do {  \
  code[0] = 0xD3;                               \
  code[1] = 0xC0 | ((middle)<<3) | (rg);        \
  code += 2;                                    \
} while (0)

#define SHIFT_LEFT_BY(rg, cnt)           SHIFT_GENERIC1(rg, cnt, 4)
#define SHIFT_LEFT_CL(rg)                SHIFT_GENERICCL(rg, 4)
#define SHIFT_RIGHT_BY(rg, cnt)          SHIFT_GENERIC1(rg, cnt, 5)
#define SHIFT_RIGHT_CL(rg)               SHIFT_GENERICCL(rg, 5)
#define SHIFT_SIGNED_RIGHT_BY(rg, cnt)   SHIFT_GENERIC1(rg, cnt, 7)
#define SHIFT_SIGNED_RIGHT_CL(rg)        SHIFT_GENERICCL(rg, 7)


EXTERNFN vinfo_t* bininstrgrp(PsycoObject* po, int group, bool ovf,
                                   bool nonneg, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* bininstrmul(PsycoObject* po, bool ovf,
                                   bool nonneg, vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* bininstrshift(PsycoObject* po, int group,
                                   bool nonneg, vinfo_t* v1, vinfo_t* v2);
EXTERNFN condition_code_t bininstrcmp(PsycoObject* po, int base_py_op,
                                      vinfo_t* v1, vinfo_t* v2);
EXTERNFN vinfo_t* bininstrcond(PsycoObject* po, condition_code_t cc,
                                   long immed_true, long immed_false);
EXTERNFN vinfo_t* unaryinstrgrp(PsycoObject* po, int group, bool ovf,
                                   bool nonneg, vinfo_t* v1);
EXTERNFN vinfo_t* unaryinstrabs(PsycoObject* po, bool ovf,
                                   bool nonneg, vinfo_t* v1);

#define BINARY_INSTR_ADD(ovf, nonneg)  bininstrgrp(po, 0, ovf, nonneg, v1, v2)
#define BINARY_INSTR_OR( ovf, nonneg)  bininstrgrp(po, 1, ovf, nonneg, v1, v2)
#define BINARY_INSTR_AND(ovf, nonneg)  bininstrgrp(po, 4, ovf, nonneg, v1, v2)
#define BINARY_INSTR_SUB(ovf, nonneg)  bininstrgrp(po, 5, ovf, nonneg, v1, v2)
#define BINARY_INSTR_XOR(ovf, nonneg)  bininstrgrp(po, 6, ovf, nonneg, v1, v2)
#define BINARY_INSTR_MUL(ovf, nonneg)  bininstrmul(po,    ovf, nonneg, v1, v2)
#define BINARY_INSTR_LSHIFT(  nonneg)  bininstrshift(po, 4,    nonneg, v1, v2)
#define BINARY_INSTR_RSHIFT(  nonneg)  bininstrshift(po, 7,    nonneg, v1, v2)
#define BINARY_INSTR_CMP(base_py_op)   bininstrcmp(po, base_py_op,     v1, v2)
#define BINARY_INSTR_COND(cc, i1, i2)  bininstrcond(po, cc,            i1, i2)
#define UNARY_INSTR_INV(ovf,  nonneg)  unaryinstrgrp(po, 2, ovf, nonneg, v1)
#define UNARY_INSTR_NEG(ovf,  nonneg)  unaryinstrgrp(po, 3, ovf, nonneg, v1)
#define UNARY_INSTR_ABS(ovf,  nonneg)  unaryinstrabs(po,    ovf, nonneg, v1)

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

/*****************************************************************/

/* PUSH the value described in the 'source' of a run-time vinfo_t */
#define PUSH_FROM_RT(source)   do {                     \
  if (RSOURCE_REG_IS_NONE(source))                      \
    PUSH_EBP_BASE(RSOURCE_STACK(source));               \
  else                                                  \
    PUSH_REG(RSOURCE_REG(source));                      \
} while (0)

/* insert a PUSH_FROM_RT at point 'insert_at' in the given 'code1' */
/* EXTERNFN code_t* insert_push_from_rt(PsycoObject* po, code_t* code1, */
/*                                      long source, code_t* insert_at); */

/* PUSH a run-time or compile-time vinfo_t's value */
#define PUSH_FROM(source)   do {                \
  if (((source) & TimeMask) == RunTime)       \
    PUSH_FROM_RT(source);                       \
  else                                          \
    PUSH_IMMED(KSOURCE_SOURCE(source)->value);  \
} while (0)


/*****************************************************************/
 /***   Some basic management instructions...                   ***/

#define PUSH_REG(reg) PUSH_R(reg)       
#define POP_REG(reg) POP_R(reg)



#define LOAD_REG_FROM_IMMED(dst, immed) MOV_R_I(dst, immed)

#define LOAD_REG_FROM_EBP_BASE(dst, stack_pos)		\
  MOV_R_O(dst, REG_X64_RSP, STACK_POS_OFFSET(stack_pos))

#define SAVE_REG_TO_EBP_BASE(src, stack_pos)		\
  MOV_O_R(REG_X64_RSP, STACK_POS_OFFSET(stack_pos), src)

#define XCHG_REG_AND_EBP_BASE(src, stack_pos)		\
  XCHG_R_O(src, REG_X64_RSP, STACK_POS_OFFSET(stack_pos))

#define SAVE_IMMED_TO_EBP_BASE(immed, stack_pos)   do { \
  INSTR_EBP_BASE(0xC7, 0<<3, stack_pos);        /* MOV [EBP-stack_pos], immed */\
  *(long*)code = (immed);                               \
  code += 4;                                            \
} while (0)

#define SAVE_IMM8_TO_EBP_BASE(imm8, stack_pos)     do { \
  INSTR_EBP_BASE(0xC6, 0<<3, stack_pos);    /* MOV byte [EBP-stack_pos], imm8 */\
  *code++ = (imm8);                                     \
} while (0)

#define ABOUT_TO_CALL_SUBFUNCTION(finfo)                \
  SAVE_IMMED_TO_EBP_BASE((long)(finfo), INITIAL_STACK_DEPTH)
#define RETURNED_FROM_SUBFUNCTION()                     \
  SAVE_IMM8_TO_EBP_BASE(-1, INITIAL_STACK_DEPTH)

#define LOAD_REG_FROM_RT(source, dst)			\
  INSTR_MODRM_FROM_RT(source, 0x8B, (dst)<<3)   /* MOV dst, (...) */

#define SAVE_REG_TO_RT(source, src)			\
  INSTR_MODRM_FROM_RT(source, 0x89, (src)<<3)   /* MOV (...), src */

#define PUSH_EBP_BASE(ofs)				\
  INSTR_EBP_BASE(0xFF, 0x30, ofs)		/* PUSH [EBP-ofs] */

#define POP_EBP_BASE(ofs)				\
  INSTR_EBP_BASE(0x8F, 0x00, ofs)		/* POP [EBP-ofs] */

#define PUSH_IMMED(immed) PUSH_I(immed)
/*******************************************************************
 * Call a C function.
 * Use BEGIN_CALL() then CALL_SET_ARG_* in reverse arg order
 * then END_CALL_R(r) or END_CALL_I(immediate)
 *******************************************************************/
static const int argument_reg_table_len = 6;
static const reg_t argument_reg_table[] = {
    REG_X64_RDI, /* First ARG in RDI etc. */
    REG_X64_RSI, 
    REG_X64_RDX, 
    REG_X64_RCX,
    REG_X64_R8,
    REG_X64_R9
};

static const bool callee_saved_reg_table[REG_TOTAL] = {
        false, /* RAX */
        false, /* RCX */
        false, /* RDX */
        true,  /* RBX */
        false, /* RSP*/
        true,  /* RBP */
        false, /* RSI */
        false, /* RDI */
        false, /* R8 */
        false, /* R9 */
        false, /* R10 */   
        false, /* R11 */
        true,  /* R12 */
        true,  /* R13 */
        true,  /* R14 */
        true,  /* R15 */
};

/* Note we only save Registers that may have a runtime vinfo so 
 * RAX, R11 and R12 which are transient are not saved by caller 
 *
 * Also we don't care about restoring thing immediatly after a call. 
 * Important things are put on the stack and if we need them pysco 
 * will generate code to get them.
 * */
#define BEGIN_CALL() do {\
    int _last_arg_index = -1;\
    int _initial_stack_depth = po->stack_depth;\
    do {} while(0)
#define _CHECK_IS_NEXT_ARG(index) do {\
    /* call in reverse order 0 indexed */\
    assert(index == _last_arg_index + 1);\
    _last_arg_index++;\
} while(0)
#define END_CALL_R(r) do {\
        for(int arg_idx = _last_arg_index + 1; arg_idx < argument_reg_table_len; arg_idx++) {\
            NEED_REGISTER(argument_reg_table[arg_idx]);\
        }\
        CALL_R(r);\
        po->stack_depth = _initial_stack_depth;\
    }while(0);\
}while(0)
#define END_CALL_I(immed) do {\
        for(int arg_idx = _last_arg_index + 1; arg_idx < argument_reg_table_len; arg_idx++) {\
            NEED_REGISTER(argument_reg_table[arg_idx]);\
        }\
        CALL_I(immed);\
        po->stack_depth = _initial_stack_depth;\
    }while(0);\
}while(0)
#define CHECK_AND_UPDATE_TARGET_REG(reg) do {\
    vinfo_t *vi = REG_NUMBER(po, reg);\
    if (vi != NULL) {\
        assert(RUNTIME_STACK(vi) != RUNTIME_STACK_NONE);\
        SET_RUNTIME_REG_TO_NONE(vi);\
        REG_NUMBER(po, (reg)) = NULL;\
    }\
} while (0)
#define CALL_SET_ARG_IMMED(immed, arg_index) do {\
  _CHECK_IS_NEXT_ARG(arg_index);\
  if(arg_index < argument_reg_table_len) {\
      reg_t dst_reg = argument_reg_table[arg_index];\
      CHECK_AND_UPDATE_TARGET_REG(dst_reg);\
      MOV_R_I(dst_reg, immed);\
  }\
  else {\
      PUSH_I(immed);\
      psyco_inc_stackdepth(po);\
  }\
} while (0)
#define CHECK_AND_UPDATE_SOURCE_REG(reg) do {\
    vinfo_t *vi = REG_NUMBER(po, reg);\
    if (vi != NULL) {\
        if (!callee_saved_reg_table[RUNTIME_REG(vi)]) {\
            assert(RUNTIME_STACK(vi) != RUNTIME_STACK_NONE);\
            SET_RUNTIME_REG_TO_NONE(vi);\
            REG_NUMBER(po, (reg)) = NULL;\
        }\
    }\
} while (0)
#define CALL_SET_ARG_FROM_REG(src_reg, arg_index) do {  \
    _CHECK_IS_NEXT_ARG(arg_index);\
    if(arg_index < argument_reg_table_len) {\
        reg_t dst_reg = argument_reg_table[arg_index];\
        if(dst_reg != src_reg) {\
            CHECK_AND_UPDATE_TARGET_REG(dst_reg);\
            MOV_R_R(dst_reg, src_reg);\
        }\
        CHECK_AND_UPDATE_SOURCE_REG(src_reg);\
    }\
    else {\
        PUSH_R(src_reg);\
        psyco_inc_stackdepth(po);\
    }\
} while (0)
#define CALL_SET_ARG_FROM_RT(source, arg_index) do {\
    if(RSOURCE_REG_IS_NONE(source)) {\
        _CHECK_IS_NEXT_ARG(arg_index);\
        assert(RUNTIME_STACK(vi) != RUNTIME_STACK_NONE);\
        if(arg_index < argument_reg_table_len) {\
            reg_t dst_reg = argument_reg_table[arg_index];\
            CHECK_AND_UPDATE_TARGET_REG(dst_reg);\
            MOV_R_O(dst_reg, REG_X64_RSP, STACK_POS_OFFSET(RSOURCE_STACK(source)));\
        } else {\
            PUSH_O(REG_X64_RSP, STACK_POS_OFFSET(RSOURCE_STACK(source)));\
            psyco_inc_stackdepth(po);\
        }\
    } else {\
        CALL_SET_ARG_FROM_REG(getreg(source), arg_index);\
    }\
} while (0)
#define CALL_SET_ARG_FROM_STACK_REF(source, arg_index) do {\
    _CHECK_IS_NEXT_ARG(arg_index);\
    assert(RSOURCE_REG_IS_NONE(source));\
    assert(RUNTIME_STACK(vi) != RUNTIME_STACK_NONE);\
    if(arg_index < argument_reg_table_len) {\
        reg_t dst_reg = argument_reg_table[arg_index];\
        CHECK_AND_UPDATE_TARGET_REG(dst_reg);\
        LEA_R_O(dst_reg, REG_X64_RSP, STACK_POS_OFFSET(RSOURCE_STACK(source)));\
    } else {\
        LEA_R_O(REG_TRANSIENT_1, REG_X64_RSP, STACK_POS_OFFSET(RSOURCE_STACK(source)));\
        PUSH_R(REG_TRANSIENT_1);\
        psyco_inc_stackdepth(po);\
    }\
} while (0)

/***************************************************************/
/***************************************************************/
#define LOAD_REG_FROM_REG_PLUS_IMMED(dst, rg1, immed) LEA_R_O(dst, rg1, immed)

#define FUNCTION_RET(popbytes)      do {                                \
  /* emit a 'RET xxx' instruction that pops and jumps to the address    \
     which is now at the top of the stack, and finishes to clean the    \
     stack by removing everything left past the return address          \
     (typically the arguments, although it could be anything). */       \
  int _b = popbytes;                                                    \
  extra_assert(0 < _b);                                                 \
  if (_b >= 0x8000)                                                     \
    {                                                                   \
      /* uncommon case: too many stuff left in the stack for the 16-bit \
         immediate we can encoding in RET */                            \
      POP_REG(REG_X64_RDX);                                             \
      STACK_CORRECTION(-_b);                                            \
      PUSH_REG(REG_X64_RDX);                                            \
      _b = 0;                                                           \
    }                                                                   \
  code[0] = 0xC2;   /* RET imm16 */                                     \
  *(short*)(code+1) = _b;                                               \
  code += 3;                                                            \
} while (0)


/*****************************************************************/
 /***   vinfo_t saving                                          ***/

/* save 'vi', which is currently in register 'rg'. */
#define SAVE_REG_VINFO(vi, rg)	do {            \
  PUSH_REG(rg);                                 \
  psyco_inc_stackdepth(po);                     \
  SET_RUNTIME_STACK_TO(vi, po->stack_depth);    \
} while (0)

/* * save 'vi' if needed. * */
/* #define SAVE_VINFO(vi)		do {                        */
/*   if (((vi)->source & (TIME_MASK | RUNTIME_STACK_MASK)) ==       */
/*                       (RUN_TIME  | RUNTIME_STACK_NONE))          */
/*     SAVE_REG_VINFO(vi, RUNTIME_REG(vi), 0);                      */
/* } while (0) */

/* ensure that the register 'rg' is free */
#define NEED_REGISTER(rg)    do {                       \
  vinfo_t* _content = REG_NUMBER(po, (rg));             \
  if (_content != NULL) {                               \
    if (RUNTIME_STACK(_content) == RUNTIME_STACK_NONE)  \
      SAVE_REG_VINFO(_content, rg);                     \
    SET_RUNTIME_REG_TO_NONE(_content);                  \
    REG_NUMBER(po, (rg)) = NULL;                        \
  }                                                     \
} while (0)

/* ensure that the condition code flags of the processor no
   longer contain any useful information */

#define NEED_CC()       NEED_CC_REG(REG_NONE)
/* same as NEED_CC() but don't overwrite rg */
#define NEED_CC_REG(rg)   do {                  \
  if (HAS_CCREG(po))                            \
    code = psyco_compute_cc(po, code, (rg));    \
} while (0)
/* same as NEED_CC() but don't overwrite the given source */
#define NEED_CC_SRC(src)                                        \
    NEED_CC_REG(is_runtime(src) ? RSOURCE_REG(src) : REG_NONE)
/* internal */
EXTERNFN code_t* psyco_compute_cc(PsycoObject* po, code_t* code, reg_t reserved);

#define LOAD_REG_FROM_CONDITION(rg, cc)   do {  /* 'rg' is an 8-bit reg */      \
  code[0] = 0x0F;               /* SETcond rg8 */                               \
  code[1] = 0x90 | (cc);                                                        \
  code[2] = 0xC0 | (rg);   /* actually an 8-bit register, but the first four    \
                              32-bit registers have the same number as their    \
                              respective lower-8-bit parts */                   \
  code[3] = 0x0F;                                                               \
  code[4] = 0xB6;               /* MOVZX rg32, rg8 */                           \
  code[5] = 0xC0 | ((rg)*9);                                                    \
  code += 6;                                                                    \
} while (0)

  
#define NEED_FREE_REG_COND(targ, cond)   do {           \
  targ = po->last_used_reg;                             \
  if (!(cond) || REG_NUMBER(po, targ) != NULL) {        \
    do {                                                \
      targ = NEXT_FREE_REG();                           \
    } while (!(cond));                                  \
    NEED_REGISTER(targ);                                \
  }                                                     \
} while (0)

/* like NEED_REGISTER but 'targ' is an output argument which will
   receive the number of a now-free register */
#define NEED_FREE_REG(targ)      NEED_FREE_REG_COND(targ, 1)
#define IS_BYTE_REG(rg)          (1) /* all registers are byte registers */
#define NEED_FREE_BYTE_REG(targ, resrv1, resrv2)                        \
           NEED_FREE_REG_COND(targ, IS_BYTE_REG(targ) &&                \
                                    targ!=(resrv1) && targ!=(resrv2))

/* make sure that the register 'reg' will not
   be returned by the next call to NEED_FREE_REG() */
#define DELAY_USE_OF(reg)       do {                    \
  if (RegistersLoop[(int) po->last_used_reg] == (reg))  \
    po->last_used_reg = (reg);                          \
  if (po->last_used_reg == (reg))                       \
    NEXT_FREE_REG();                                    \
} while (0)

/* the same for two registers */
#define DELAY_USE_OF_2(rg1, rg2)   do {         \
  DELAY_USE_OF(rg1);                            \
  DELAY_USE_OF(rg2);                            \
  DELAY_USE_OF(rg1);                            \
} while (0)

/* the same if the given source is run-time and in a register */
#define DONT_OVERWRITE_SOURCE(src)   do {       \
  if (is_runtime_with_reg(src))                 \
    DELAY_USE_OF(RSOURCE_REG(src));             \
} while (0)


/*****************************************************************/
 /***   vinfo_t restoring                                       ***/

/* ensure that a run-time vinfo is in a register */
#define RTVINFO_IN_REG(vi)	  do {          \
  if (RUNTIME_REG_IS_NONE(vi)) {                \
    /* reload the vinfo from the stack */       \
    reg_t _rg;                                  \
    long _stack;                                \
    NEED_FREE_REG(_rg);                         \
    REG_NUMBER(po, _rg) = (vi);                 \
    _stack = RUNTIME_STACK(vi);                 \
    SET_RUNTIME_REG_TO(vi, _rg);                \
    LOAD_REG_FROM_EBP_BASE(_rg, _stack);        \
  }                                             \
} while (0)

#define RTVINFO_IN_BYTE_REG(vi, resrv1, resrv2)   do {  \
  reg_t _currg = RUNTIME_REG(vi);                       \
  if (!IS_BYTE_REG(_currg)) {                           \
    reg_t _rg;                                          \
    NEED_FREE_BYTE_REG(_rg, resrv1, resrv2);            \
    if (_currg != REG_NONE)                             \
      REG_NUMBER(po, _currg) = NULL;                    \
    REG_NUMBER(po, _rg) = (vi);                         \
    LOAD_REG_FROM_RT((vi)->source, _rg);                \
    SET_RUNTIME_REG_TO(vi, _rg);                        \
  }                                                     \
} while (0)

/* load register 'dst' from the given non-virtual source vinfo */
#define LOAD_REG_FROM(source, dst)    do {                      \
  if (((source) & CompileTime) != 0) {                           \
    LOAD_REG_FROM_IMMED(dst, KSOURCE_SOURCE(source)->value);    \
  }\
  else if (RSOURCE_REG(source) != dst) {\
      LOAD_REG_FROM_RT(source, dst);                            \
  }                                                             \
} while (0)


/*****************************************************************/
 /***   conditional jumps                                       ***/

#define JUMP_TO(addr)   do {                    \
  *code++ = 0xE9;   /* JMP rel32 */             \
  *(dword_t*)(code) = (addr) - (code+4);\
  code += 4;\
} while (0)

#define IS_A_JUMP(code, targetaddr)                             \
  (code[1]==(code_t)0xE9 && (targetaddr=code+6+*(dword_t*)(code+2), 1))

#define IS_A_SINGLE_JUMP(code, codeend, targetaddr)             \
  ((codeend)-(code) == SIZE_OF_FAR_JUMP && IS_A_JUMP(code, targetaddr))

#define FAR_COND_JUMP_TO(addr, condition)   do {        \
  *code++ = 0x0F;\
  *code++ = 0x80 | (code_t)(condition);\
  *(dword_t*)(code) = (addr) - (code+4);\
  code += 4;\
} while (0)

#define CHANGE_JUMP_TO(addr)                do {        \
  *(dword_t*)(code-4) = (addr) - code;                  \
} while (0)

#define SIZE_OF_FAR_JUMP                   5
#define MAXIMUM_SIZE_OF_FAR_JUMP    SIZE_OF_FAR_JUMP
#define SIZE_OF_SHORT_CONDITIONAL_JUMP     2    /* Jcond rel8 */
#define RANGE_OF_SHORT_CONDITIONAL_JUMP  127    /* max. positive offset */

#define BEGIN_SHORT_JUMP(id) \
    code[0] = 0xEB;\
    code += 2;\
    code_t *_short_jump_op_end_ ## id = code;\
    do {} while (0)
#define BEGIN_SHORT_COND_JUMP(id, condition) \
    code[0] = 0x70 | (condition);\
    code += SIZE_OF_SHORT_CONDITIONAL_JUMP;\
    code_t *_short_jump_op_end_ ## id = code;\
    do {} while(0)
#define END_SHORT_JUMP(id) do {\
    long _jump_amount = (long)code - (long)_short_jump_op_end_ ## id;\
    extra_assert(-128 <= _jump_amount && _jump_amount < 128);\
    *(_short_jump_op_end_ ## id - 1) = (code_t)_jump_amount;\
} while(0)

#define BEGIN_REVERSE_SHORT_JUMP(id) \
    code_t *_reverse_short_jump_target_ ## id = code;
#define END_REVERSE_SHORT_COND_JUMP(id, cond) do {\
    long _jump_amount = (long)_reverse_short_jump_target_ ## id - (long)code -2;\
    extra_assert(-128 <= _jump_amount && _jump_amount < 128);\
    code[0] = 0x70 | (cond);\
    code[1] = _jump_amount;\
    code += 2;\
} while (0)
    

#define SHORT_COND_JUMP_TO(addr, condition)  do {       \
  long _ofs = (addr) - (code+2);                        \
  extra_assert(-128 <= _ofs && _ofs < 128);             \
  code[0] = 0x70 | (code_t)(condition);                 \
  code[1] = (code_t) _ofs;                              \
  code += 2;                                            \
} while (0)


/* correct the stack pointer */
#define STACK_CORRECTION(stack_correction) do {\
    if ((stack_correction) != 0) {\
        if (FITS_IN_8BITS(stack_correction)) {\
            SUB_R_I8(REG_X64_RSP, (stack_correction));\
        } else {\
            SUB_R_I32(REG_X64_RSP, (stack_correction));\
        }\
    }\
} while (0)


/* convenience macros */
#define COPY_IN_REG(vi, rg)   do {                      \
   NEED_FREE_REG(rg);                                   \
   if (((vi)->source & (TimeMask|RunTime_StackMask)) == \
       (RunTime|RunTime_StackNone)) {                   \
     reg_t _rg2 = rg;                                   \
     rg = RUNTIME_REG(vi);                              \
     extra_assert(rg!=_rg2);                            \
     MOV_R_R(_rg2, rg);                       \
     SET_RUNTIME_REG_TO(vi, _rg2);                      \
     REG_NUMBER(po, _rg2) = vi;                         \
     REG_NUMBER(po, rg) = NULL;                         \
   }                                                    \
   else {                                               \
     LOAD_REG_FROM(vi->source, rg);                     \
   }                                                    \
} while (0)


#define ALIGN_PAD_CODE_PTR()     do {                                   \
  if ((((long)code) & 15) > 8)   /* directive .p2align 4,,7 of 'as' */  \
    code = (code_t*)((((long)code) & ~15) + 16);                        \
} while (0)

#define ALIGN_WITH_BYTE(byte)   do {                                       \
  if ((((long)code) & 15) > 8)   /* directive .p2align 4,,7 of 'as' */     \
    do {                                                                   \
      *code++ = byte;                                                      \
    } while ((((long)code) & 15) != 0);                                    \
} while (0)

/* XXX in the GNU 'as' padding is more subtle: it inserts only one or two
     instructions that do nothing but take more space than a single NOP */
#define ALIGN_WITH_NOP()    ALIGN_WITH_BYTE(0x90)

#if ALL_CHECKS
#define ALIGN_NO_FILL() ALIGN_WITH_BYTE(0xCC)   /* INT 03  (debugging) */
#else
#define ALIGN_NO_FILL() ALIGN_PAD_CODE_PTR()
#endif


#endif /* _IENCODING_H */
