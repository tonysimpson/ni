#include "processor.h"
#include "vcompiler.h"
#include "codemanager.h"
#include "pycencoding.h"
#include "Python/pycompiler.h"  /* for exception handling stuff */


/* define to copy static machine code in the heap before running it.
   I've seen some Linux distributions in which the static data pages
   are not executable by default. */
#define COPY_CODE_IN_HEAP


/* We make no special use of any register but ESP, and maybe EBP
 * (if EBP_IF_RESERVED).
 * We consider that we can call C functions with arbitrary values in
 * all registers but ESP, and that only EAX, ECX and EDX will be
 * clobbered.
 * We do not use EBP as the frame pointer, unlike normal C compiled
 * functions. This makes instruction encodings one byte longer
 * (ESP-relative instead of EBP-relative).
 */

DEFINEVAR
reg_t RegistersLoop[REG_TOTAL] = {
  /* following EAX: */  REG_386_ECX,
  /* following ECX: */  REG_386_EDX,
  /* following EDX: */  REG_386_EBX,
  /* following EBX: */  EBP_IS_RESERVED ? REG_386_ESI : REG_386_EBP,
  /* following ESP: */  REG_NONE,
  /* following EBP: */  EBP_IS_RESERVED ? REG_NONE : REG_386_ESI,
  /* following ESI: */  REG_386_EDI,
  /* following EDI: */  REG_386_EAX };


/* glue code for psyco_processor_run(). */
static code_t glue_run_code[] = {
  0x8B, 0x44, 0x24, 4,          /*   MOV EAX, [ESP+4]  (code target)   */
  0x8B, 0x4C, 0x24, 8,          /*   MOV ECX, [ESP+8]  (stack end)     */
  0x8B, 0x54, 0x24, 12,         /*   MOV EDX, [ESP+12] (initial stack) */
  PUSH_REG_INSTR(REG_386_EBP),  /*   PUSH EBP        */
  PUSH_REG_INSTR(REG_386_EBX),  /*   PUSH EBX        */
  PUSH_REG_INSTR(REG_386_ESI),  /*   PUSH ESI        */
  PUSH_REG_INSTR(REG_386_EDI),  /*   PUSH EDI        */
  0x8B, 0x5C, 0x24, 32,         /*   MOV EBX, [ESP+32] (finfo frame stack ptr) */
  0x6A, -1,                     /*   PUSH -1         */
  0x89, 0x23,                   /*   MOV [EBX], ESP  */
  0xEB, +5,                     /*   JMP Label2      */
                                /* Label1:           */
  0x83, 0xE9, 4,                /*   SUB ECX, 4      */
  0xFF, 0x31,                   /*   PUSH [ECX]      */
                                /* Label2:           */
  0x39, 0xCA,                   /*   CMP EDX, ECX    */
  0x75, -9,                     /*   JNE Label1      */
  0xFF, 0xD0,                   /*   CALL *EAX     (callee removes args)  */
  POP_REG_INSTR(REG_386_EDI),   /*   POP EDI         */
  POP_REG_INSTR(REG_386_ESI),   /*   POP ESI         */
  POP_REG_INSTR(REG_386_EBX),   /*   POP EBX         */
  POP_REG_INSTR(REG_386_EBP),   /*   POP EBP         */
  0xC3,                         /*   RET             */
};

typedef PyObject* (*glue_run_code_fn) (code_t* code_target,
				       long* stack_end,
				       long* initial_stack,
				       struct stack_frame_info_s*** finfo);

#ifdef COPY_CODE_IN_HEAP
static glue_run_code_fn glue_run_code_1;
#else
# define glue_run_code_1 ((glue_run_code_fn) glue_run_code)
#endif

DEFINEFN
PyObject* psyco_processor_run(CodeBufferObject* codebuf,
                              long initial_stack[], int argc,
                              struct stack_frame_info_s*** finfo)
{
  return glue_run_code_1(codebuf->codeptr, initial_stack + argc,
                         initial_stack, finfo);
}

/* call a C function with a variable number of arguments */
DEFINEVAR long (*psyco_call_var) (void* c_func, int argcount, long arguments[]);

static code_t glue_call_var[] = {
	0x53,			/*   PUSH EBX                      */
	0x8B, 0x5C, 0x24, 12,	/*   MOV EBX, [ESP+12]  (argcount) */
	0x8B, 0x44, 0x24, 8,	/*   MOV EAX, [ESP+8]   (c_func)   */
	0x09, 0xDB,		/*   OR EBX, EBX                   */
	0x74, +16,		/*   JZ Label1                     */
	0x8B, 0x54, 0x24, 16,	/*   MOV EDX, [ESP+16] (arguments) */
	0x8D, 0x0C, 0x9A,	/*   LEA ECX, [EDX+4*EBX]          */
				/* Label2:                         */
	0x83, 0xE9, 4,		/*   SUB ECX, 4                    */
	0xFF, 0x31,		/*   PUSH [ECX]                    */
	0x39, 0xCA,		/*   CMP EDX, ECX                  */
	0x75, -9,		/*   JNE Label2                    */
				/* Label1:                         */
	0xFF, 0xD0,		/*   CALL *EAX                     */
	0x8D, 0x24, 0x9C,	/*   LEA ESP, [ESP+4*EBX]          */
	0x5B,			/*   POP EBX                       */
	0xC3,			/*   RET                           */
};

/* check for signed integer multiplication overflow */
static code_t glue_int_mul[] = {
  0x8B, 0x44, 0x24, 8,          /*   MOV  EAX, [ESP+8]  (a)   */
  0x0F, 0xAF, 0x44, 0x24, 4,    /*   IMUL EAX, [ESP+4]  (b)   */
  0x0F, 0x90, 0xC0,             /*   SETO AL                  */
  0xC3,                         /*   RET                      */
};

typedef char (*glue_int_mul_fn) (long a, long b);

#ifdef COPY_CODE_IN_HEAP
static glue_int_mul_fn glue_int_mul_1;
#else
# define glue_int_mul_1 ((glue_int_mul_fn) glue_int_mul)
#endif


DEFINEFN
void psyco_emit_header(PsycoObject* po, int nframelocal)
{
  int j = nframelocal;
  vinfo_array_t* array;
  extra_assert(LOC_CONTINUATION->array->count == 0);

  BEGIN_CODE
  INITIALIZE_FRAME_LOCALS(nframelocal);
  po->stack_depth += 4*nframelocal;
  END_CODE

  array = LOC_CONTINUATION->array = array_new(nframelocal);
  while (j--)
    array->items[j] = vinfo_new(RunTime_NewStack
                             (po->stack_depth - 4*j, REG_NONE, false));
}

DEFINEFN
code_t* psyco_finish_return(PsycoObject* po, NonVirtualSource retval)
{
  code_t* code = po->code;
  int retpos;
  int nframelocal = LOC_CONTINUATION->array->count;

  /* 'retpos' is the position in the stack of the return address. */
  retpos = getstack(LOC_CONTINUATION->source);
  extra_assert(retpos != RunTime_StackNone);

  /* load the return value into EAX for regular functions, EBX for functions
     with a prologue */
  if (retval != SOURCE_DUMMY) {
    reg_t rg = nframelocal>0 ? REG_ANY_CALLEE_SAVED : REG_FUNCTIONS_RETURN;
    LOAD_REG_FROM(retval, rg);
  }

  if (nframelocal > 0)
    {
      /* psyco_emit_header() was used; first clear the stack only up to and not
         including the frame-local data */
      int framelocpos = retpos + 4*nframelocal;
      extra_assert(framelocpos ==
                   getstack(LOC_CONTINUATION->array->items[0]->source));
      STACK_CORRECTION(framelocpos - po->stack_depth);
      po->stack_depth = framelocpos;
      
      /* perform Python-specific cleanup */
      FINALIZE_FRAME_LOCALS(nframelocal);
      LOAD_REG_FROM_REG(REG_FUNCTIONS_RETURN, REG_ANY_CALLEE_SAVED);
    }

  /* now clean up the stack up to retpos */
  STACK_CORRECTION(retpos - po->stack_depth);

  /* emit a 'RET xxx' instruction that pops and jumps to the address
     which is now at the top of the stack, and finishes to clean the
     stack by removing everything left past the return address
     (typically the arguments, although it could be anything). */
  retpos -= INITIAL_STACK_DEPTH;
  extra_assert(0<retpos);
  if (retpos >= 0x8000)
    {
      /* uncommon case: too many stuff left in the stack for the 16-bit
         immediate we can encoding in RET */
      POP_REG(REG_386_EDX);
      STACK_CORRECTION(-retpos);
      PUSH_REG(REG_386_EDX);
      retpos = 0;
    }
  code[0] = 0xC2;   // RET imm16
  *(short*)(code+1) = retpos;
  PsycoObject_Delete(po);
  return code+3;
}

#if 0   /* disabled */
DEFINEFN
code_t* psyco_emergency_jump(PsycoObject* po, code_t* code)
{
  STACK_CORRECTION(INITIAL_STACK_DEPTH - po->stack_depth);  /* at most 6 bytes */
  code[0] = 0xE9;   /* JMP rel32 */
  code += 5;
  *(long*)(code-4) = ((code_t*)(&PyErr_NoMemory)) - code;
  /* total: at most 11 bytes. Check the value of EMERGENCY_PROXY_SIZE. */
  return code;
}
#endif

DEFINEFN
void* psyco_jump_proxy(PsycoObject* po, void* fn, int restore, int nb_args)
{
  code_t* code = po->code;
  void* result;
  code_t* fixvalue;
  
  /* last pushed argument (will be the first argument of 'fn') */
  code[0] = 0x68;     /* PUSH IMM32	*/
  fixvalue = code+1;    /* will be filled below */
  code[5] = 0xE8;     /* CALL fn	*/
  code += 10;
  *(long*)(code-4) = ((code_t*)fn) - code;

  if (restore)
    {
      /* cancel the effect of any CALL_SET_ARG_xxx on po->stack_depth,
         to match the 'ADD ESP' instruction below */
      po->stack_depth -= 4*(nb_args-1);
      
      extra_assert(4*nb_args < 128);   /* a safe guess */
      CODE_FOUR_BYTES(code,
                      0x83,       /* ADD		  */
                      0xC4,       /*     ESP,		  */
                      4*nb_args,  /*           4*nb_args  */
                      0);         /* not used             */
      code += 3;
      TEMP_RESTORE_REGS_FN_CALLS_AND_JUMP;
    }
  else
    {
      po->stack_depth += 4;  /* for the PUSH IMM32 above */
      code[0] = 0xFF;      /* JMP *EAX */
      code[1] = 0xE0;
      code += 2;
    }

    /* make 'fs' point just after the end of the code, aligned */
  result = (void*)(((long)code + 3) & ~ 3);
#if CODE_DUMP
  while (code != (code_t*) result)
    *code++ = (code_t) 0xCC;   /* fill with INT 3 (debugger trap) instructions */
#endif
  *(void**)fixvalue = result;    /* set value at code+1 above */
  return result;
}

/*DEFINEFN
void psyco_stack_space_array(PsycoObject* po, vinfo_t** args,
                             int cnt, bool with_reference)
{
  int i = cnt;
  BEGIN_CODE
  ......
  END_CODE
  while (i--)
    args[i] = vinfo_new(RunTime_NewStack(po_stack_depth - 4*i,
                                         REG_NONE, with_reference));
					 }*/

/* DEFINEFN */
/* code_t* insert_push_from_rt(PsycoObject* po, code_t* code1, */
/*                             long source, code_t* insert_at) */
/* { */
/*   code_t codebuffer[16]; */
/*   code_t* code = codebuffer; */
/*   int shiftby; */
/*   PUSH_FROM_RT(source); */
/*   shiftby = code - codebuffer; */
/*   memmove(insert_at + shiftby, insert_at, code1-insert_at); */
/*   return code1 + shiftby; */
/* } */


DEFINEFN
vinfo_t* psyco_get_array_item(PsycoObject* po, vinfo_t* vi, int index)
{
  vinfo_t* result;
  NonVirtualSource source = vinfo_compute(vi, po);
  if (source == SOURCE_ERROR) return NULL;
  if (is_runtime(source))
    {
      reg_t src, rg;
      BEGIN_CODE
      RTVINFO_IN_REG(vi);
      src = RUNTIME_REG(vi);
      DELAY_USE_OF(src);
      NEED_FREE_REG(rg);
      code[0] = 0x8B;        /* MOV rg, [src + 4*index] */
      if (COMPACT_ENCODING && index < 128/4) {
        code[1] = 0x40 | (rg<<3) | src;
        code[2] = index*4;
        code += 3;
      }
      else {
        code[1] = 0x80 | (rg<<3) | src;
        *(long*)(code+2) = index*4;
        code += 6;
      }
      END_CODE
      result = new_rtvinfo(po, rg, false);
    }
  else {
    long value = ((long*)(CompileTime_Get(source)->value))[index];
    result = vinfo_new(CompileTime_New(value));
  }
  vinfo_setitem(po, vi, index, result);
  return result;
}

#define READ_ARRAY_ITEM_1(rg, _v, offset, _byte)     do {       \
  code_t _modrm;                                                \
  long _value = (offset);                                       \
  if (!is_compiletime(_v->source))                              \
    {                                                           \
      reg_t _src;                                               \
      DELAY_USE_OF(rg);                                         \
      RTVINFO_IN_REG(_v);                                       \
      _src = RUNTIME_REG(_v);                                   \
      extra_assert(_value >= 0);                                \
      if (COMPACT_ENCODING && _value < 128)                     \
        _modrm = 0x40 | _src;    /* MOV rg, [src + offset] */   \
      else                                                      \
        _modrm = 0x80 | _src;    /* MOV rg, [src + offset] */   \
    }                                                           \
  else                                                          \
    {                                                           \
      _modrm = 0x05;           /* MOV rg, [immed] */            \
      _value += KNOWN_SOURCE(_v)->value;                        \
    }                                                           \
  if (_byte) {                                                  \
    *code++ = 0x0F;                                             \
    code[0] = 0xB6;   /* MOVZX instead of MOV */                \
  }                                                             \
  else                                                          \
    code[0] = 0x8B;   /* MOV */                                 \
  code[1] = _modrm | ((rg)<<3);                                 \
  if (COMPACT_ENCODING && (_modrm & 0x40) != 0) {               \
    code[2] = (code_t) _value;                                  \
    code += 3;                                                  \
  }                                                             \
  else {                                                        \
    *(long*)(code+2) = _value;                                  \
    code += 6;                                                  \
  }                                                             \
} while (0)

DEFINEFN
vinfo_t* psyco_read_array_item(PsycoObject* po, vinfo_t* vi, int index)
{
  reg_t rg;
  if (vinfo_compute(vi, po) == SOURCE_ERROR) return NULL;
  BEGIN_CODE
  NEED_FREE_REG(rg);
  READ_ARRAY_ITEM_1(rg, vi, index*4, 0);
  END_CODE
  return new_rtvinfo(po, rg, false);
}

DEFINEFN
vinfo_t* psyco_read_array_item_var(PsycoObject* po, vinfo_t* v0,
                                   vinfo_t* v1, int ofsbase, int shift)
{
  reg_t rg1;
  NonVirtualSource v0_source;
  NonVirtualSource v1_source;
  v0_source = vinfo_compute(v0, po);
  if (v0_source == SOURCE_ERROR) return NULL;
  v1_source = vinfo_compute(v1, po);
  if (v0_source == SOURCE_ERROR) return NULL;
  BEGIN_CODE
  NEED_FREE_REG(rg1);
  if (is_compiletime(v1_source))
    READ_ARRAY_ITEM_1(rg1, v0,
                      (CompileTime_Get(v1_source)->value << shift) + ofsbase,
                      !shift);
  else
    {
      reg_t src0, src1;
      DELAY_USE_OF(rg1);
      RTVINFO_IN_REG(v1);
      src1 = RUNTIME_REG(v1);
      
      if (!is_compiletime(v0_source))
        {
          DELAY_USE_OF_2(rg1, src1);
          RTVINFO_IN_REG(v0);
          src0 = RUNTIME_REG(v0);
        }
      else
        src0 = 0x05;

      if (shift)
        code[0] = 0x8B;     /* MOV rg1, [ofsbase+(src0-or-immed)+src1<<shift] */
      else
        {
          *code++ = 0x0F;
          code[0] = 0xB6;   /* MOVZX instead of MOV */
        }
      code[1] = 0x04 | (rg1<<3);
      code[2] = (shift<<6) | (src1<<3) | src0;
      
      if (is_compiletime(v0_source))
        {
          /* MOV(ZX)  rg1, [base+src1<<shift] */
          *(long*)(code+3) = CompileTime_Get(v0_source)->value + ofsbase;
          code += 7;
        }
      else
        {
          code += 3;
          if (ofsbase != 0 || (!EBP_IS_RESERVED && src0 == REG_386_EBP))
            {
              extra_assert(0 <= ofsbase);
              if (COMPACT_ENCODING && ofsbase < 128)
                {
                  code[-2] |= 0x40;
                  *code++ = ofsbase;
                }
              else
                {
                  code[-2] |= 0x80;
                  *(long*)code = ofsbase;
                  code += 4;
                }
            }
        }
    }
  END_CODE
  return new_rtvinfo(po, rg1, false);
}

DEFINEFN
bool psyco_write_array_item(PsycoObject* po, vinfo_t* src, vinfo_t* v, int index)
{
  reg_t rg;
  code_t modrm;
  long value = index*4;
  if (is_virtualtime(v->source))
    {
      vinfo_incref(src);   /* done, bypass compute() */
      set_array_item(po, v, index, src);
    }
  else
    {
      if (vinfo_compute(src, po) == SOURCE_ERROR) return false;
      BEGIN_CODE
      if (!is_compiletime(v->source))
        {
          reg_t srcrg;
          RTVINFO_IN_REG(v);
          srcrg = RUNTIME_REG(v);
          DELAY_USE_OF(srcrg);
          if (COMPACT_ENCODING && value < 128)
            modrm = 0x40 | srcrg;    /* MOV [src + 4*item], ... */
          else
            modrm = 0x80 | srcrg;    /* MOV [src + 4*item], ... */
        }
      else
        {
          modrm = 0x05;           /* MOV [immed], ... */
          value += KNOWN_SOURCE(v)->value;
        }
      if (is_compiletime(src->source))
        code[0] = 0xC7;   /* MOV [...], imm32 */
      else
        {
          RTVINFO_IN_REG(src);
          rg = RUNTIME_REG(src);
          code[0] = 0x89;   /* MOV [...], rg */
          modrm |= (rg<<3);
        }
      code[1] = modrm;
      if (COMPACT_ENCODING && (modrm & 0x40) != 0) {
        code[2] = (code_t) value;
        code += 3;
      }
      else {
        *(long*)(code+2) = value;
        code += 6;
      }
      if (is_compiletime(src->source)) {
        *(long*)code = KNOWN_SOURCE(src)->value;
        code += 4;
      }
      END_CODE
    }
  return true;
}

DEFINEFN
bool psyco_write_array_item_var(PsycoObject* po, vinfo_t* src,
                                vinfo_t* v0, vinfo_t* v1, int ofsbase)
{
  NonVirtualSource v1_source = vinfo_compute(v1, po);
  if (v1_source == SOURCE_ERROR) return false;
  if (is_compiletime(v1_source))
    psyco_write_array_item(po, src, v0,
                           CompileTime_Get(v1_source)->value + QUARTER(ofsbase));
  else
    {
      reg_t src0, src1, rg1;
      NonVirtualSource src_source, v0_source;
      src_source = vinfo_compute(src, po);
      if (src_source == SOURCE_ERROR) return false;
      v0_source  = vinfo_compute(v0, po);
      if (v0_source == SOURCE_ERROR) return false;

      BEGIN_CODE
      if (!is_compiletime(src_source))
        {
          RTVINFO_IN_REG(src);
          rg1 = RUNTIME_REG(src);
          DELAY_USE_OF(rg1);
        }
      else
        rg1 = REG_NONE;
      
      RTVINFO_IN_REG(v1);
      src1 = RUNTIME_REG(v1);
      
      if (!is_compiletime(v0_source))
        {
          DELAY_USE_OF_2(rg1, src1);
          RTVINFO_IN_REG(v0);
          src0 = RUNTIME_REG(v0);
        }
      else
        src0 = 0x05;

      if (rg1 != REG_NONE)
        {
          code[0] = 0x89;    /* MOV [ofsbase+(src0-or-immed)+src1<<shift], rg1 */
          code[1] = 0x04 | (rg1<<3);
        }
      else
        {
          code[0] = 0xC7;    /* MOV [ the same as above ], immed */
          code[1] = 0x04 | (0<<3);
        }
      code[2] = (2<<6) | (src1<<3) | src0;
      if (is_compiletime(v0_source))
        {
          /* MOV  [base+src1<<shift], ... */
          *(long*)(code+3) = CompileTime_Get(v0_source)->value + ofsbase;
          code += 7;
        }
      else
        {
          code += 3;
          if (ofsbase != 0 || (!EBP_IS_RESERVED && src0 == REG_386_EBP))
            {
              extra_assert(0 <= ofsbase);
              if (COMPACT_ENCODING && ofsbase < 128)
                {
                  code[-2] |= 0x40;
                  *code++ = ofsbase;
                }
              else
                {
                  code[-2] |= 0x80;
                  *(long*)code = ofsbase;
                  code += 4;
                }
            }
        }
      
      if (is_compiletime(src_source))
        {
          /* MOV  [...], immed */
          *(long*)code = CompileTime_Get(src_source)->value;
          code += 4;
        }
      END_CODE
    }
  return true;
}


/***************************************************************/
 /*** Condition Codes (a.k.a. the processor 'flags' register) ***/

typedef struct {
  source_virtual_t header;
  code_t cc;
} computed_cc_t;

/* internal, see NEED_CC() */
DEFINEFN
code_t* psyco_compute_cc(PsycoObject* po, code_t* code, reg_t reserved)
{
	vinfo_t* v = po->ccreg;
	computed_cc_t* cc = (computed_cc_t*) VirtualTime_Get(v->source);
	reg_t rg;

	NEED_FREE_BYTE_REG(rg, reserved);
	LOAD_REG_FROM_CONDITION(rg, cc->cc);

	v->source = RunTime_New(rg, false);
	REG_NUMBER(po, rg) = v;
	po->ccreg = NULL;
        return code;
}

static bool generic_computed_cc(PsycoObject* po, vinfo_t* v)
{
	extra_assert(po->ccreg == v);
        BEGIN_CODE
	code = psyco_compute_cc(po, code, REG_NONE);
        END_CODE
	return true;
}

static computed_cc_t cc_functions_table[CC_TOTAL];


DEFINEFN
vinfo_t* psyco_vinfo_condition(PsycoObject* po, condition_code_t cc)
{
  vinfo_t* result;
  if (cc < CC_TOTAL)
    {
      if (po->ccreg != NULL)
        {
          /* there is already a value in the processor flags register */
          extra_assert(psyco_vsource_cc(po->ccreg->source) != CC_ALWAYS_FALSE);
          
          if (psyco_vsource_cc(po->ccreg->source) == cc)
            {
              /* it is the same condition, so reuse it */
              result = po->ccreg;
              vinfo_incref(result);
              return result;
            }
          /* it is not the same condition, save it */
          BEGIN_CODE
          NEED_CC();
          END_CODE
        }
      extra_assert(po->ccreg == NULL);
      po->ccreg = vinfo_new(VirtualTime_New
                            (&cc_functions_table[(int)cc].header));
      result = po->ccreg;
    }
  else
    result = vinfo_new(CompileTime_New(cc == CC_ALWAYS_TRUE));
  return result;
}

DEFINEFN
condition_code_t psyco_vsource_cc(Source source)
{
  if (is_virtualtime(source))
    {
      computed_cc_t* s = (computed_cc_t*) VirtualTime_Get(source);
      long result = s - cc_functions_table;
      if (0 <= result && result < CC_TOTAL)
        return (condition_code_t) result;
    }
  return CC_ALWAYS_FALSE;
}


#ifdef COPY_CODE_IN_HEAP
#  define COPY_CODE(target, source, type)   do {	\
	char* c = PyMem_MALLOC(sizeof(source));		\
	if (c == NULL) {				\
		PyErr_NoMemory();			\
		return;					\
	}						\
	memcpy(c, source, sizeof(source));		\
	target = (type) c;				\
} while (0)
#else
#  define COPY_CODE(target, source, type)   (target = (type) source)
#endif


static bool computed_promotion(PsycoObject* po, vinfo_t* v);  /* forward */

INITIALIZATIONFN
void psyco_processor_init(void)
{
  int i;
#ifdef COPY_CODE_IN_HEAP
  COPY_CODE(glue_run_code_1, glue_run_code, glue_run_code_fn);
  COPY_CODE(glue_int_mul_1,  glue_int_mul,  glue_int_mul_fn);
#endif
  COPY_CODE(psyco_call_var, glue_call_var, long(*)(void*, int, long[]));
  for (i=0; i<CC_TOTAL; i++)
    {
      cc_functions_table[i].header.compute_fn = &generic_computed_cc;
      cc_functions_table[i].cc = (condition_code_t) i;
    }
  psyco_nonfixed_promotion.header.compute_fn = &computed_promotion;
#if USE_RUNTIME_SWITCHES
  psyco_nonfixed_promotion.fs = NULL;
#endif
  psyco_nonfixed_promotion.kflags = SkFlagFixed;
  psyco_nonfixed_pyobj_promotion.header.compute_fn = &computed_promotion;
#if USE_RUNTIME_SWITCHES
  psyco_nonfixed_pyobj_promotion.fs = NULL;
#endif
  psyco_nonfixed_pyobj_promotion.kflags = SkFlagFixed | SkFlagPyObj;
}


/*****************************************************************/
 /***   run-time switches                                       ***/

static bool computed_promotion(PsycoObject* po, vinfo_t* v)
{
  /* uncomputable, but still use the address of computed_promotion() as a
     tag to figure out if a virtual source is a c_promotion_s structure. */
  return psyco_vsource_not_important.compute_fn(po, v);
}

DEFINEVAR struct c_promotion_s psyco_nonfixed_promotion;
DEFINEVAR struct c_promotion_s psyco_nonfixed_pyobj_promotion;

DEFINEFN
bool psyco_vsource_is_promotion(VirtualTimeSource source)
{
  return VirtualTime_Get(source)->compute_fn == &computed_promotion;
}


#if USE_RUNTIME_SWITCHES

/* The tactic is to precompute, given a list of the values we will
   have to switch on, a binary tree search algorithm in machine
   code. Example for the list of values [10, 20, 30, 40]:

	CMP EAX, 30
	JG L1		; if EAX > 30, jump to L1
	JE Case30	; if EAX == 30, jump to Case30
	CMP EAX, 20
	JG L2		; if 20 < EAX < 30, jump to L2
	JE Case20	; if EAX == 20, jump to Case20
	CMP EAX, 10
	JE Case10	; if EAX == 10, jump to Case10
	JMP L2		; otherwise, jump to L2
   L1:  CMP EAX, 40
	JE Case40	; if EAX == 40, jump to Case40
   L2:  JMP Default     ; <--- 'supposed_end' below
                        ; <--- 'supposed_end+5'

   All targets (Default and Case10 ... Case40) are initially set to
   point to the end of this code block. At compile-time we will put
   at this place a proxy that calls the dispatcher. The dispatcher then
   fixes the target in the code itself as a shortcut for the next time
   that the same value is found.

   The second byte of the CMP instructions must be dynamically corrected
   to mention the actual register, which is not necessarily EAX. This is
   done by coding a linked list in these bytes, each one holding the
   offset to the next CMP's second byte.
*/

struct fxcase_s {
  long value;             /* value to switch on */
  long index;             /* original (unsorted) index of this value */
};

static int fx_compare(const void* a, const void* b)
{
  long va = ((const struct fxcase_s*)a)->value;
  long vb = ((const struct fxcase_s*)b)->value;
  if (va < vb)
    return -1;
  if (va > vb)
    return 1;
  return 0;
}

#define FX_BASE_SIZE        5
#define FX_MAX_ITEM_SIZE    18
#define FX_LAST_JUMP_SIZE   2

static code_t* fx_writecases(code_t* code, code_t** lastcmp,
                             struct fxcase_s* fxc, long* fixtargets,
                             int first, int last, code_t* supposed_end)
{
  /* write the part of the switch corresponding to the cases between
     'first' (inclusive) and 'last' (exclusive).
     '*lastcmp' points to the last CMP instruction's second byte. */
  if (first == last)
    {
      /* jump to 'default:' */
      long offset;
      code += 2;
      offset = supposed_end - code;
      if (offset < 128)
        {
          code[-2] = 0xEB;   /* JMP rel8 */
          code[-1] = (code_t) offset;
        }
      else
        {
          code += 3;
          code[-5] = 0xE9;   /* JMP rel32 */
          *(long*)(code-4) = offset;
        }
    }
  else
    {
      code_t* code2 = code+1;
      long offset;
      int middle = (first+last-1)/2;
      COMMON_INSTR_IMMED(7, 0, fxc[middle].value); /* CMP reg, imm */
      if (*lastcmp != NULL)
        {
          extra_assert(code2 - *lastcmp < 128);   /* CMP instructions are close
                                                    to each other */
          **lastcmp = code2 - *lastcmp;
        }
      *lastcmp = code2;
      if (middle > first)
        {
          code += 2;
          code2 = fx_writecases(code+6, lastcmp, fxc, fixtargets,
                                first, middle, supposed_end);
          offset = code2 - code;
          if (offset < 128)
            {
              code[-2] = 0x7F;    /* JG rel8 */
              code[-1] = (code_t) offset;
            }
          else
            {
              code += 4;
              code2 = fx_writecases(code+6, lastcmp, fxc, fixtargets,
                                    first, middle, supposed_end);
              code[-6] = 0x0F;
              code[-5] = 0x8F;    /* JG rel32 */
              *(long*)(code-4) = code2 - code;
            }
        }
      else
        code2 = code+6;
      code[0] = 0x0F;
      code[1] = 0x84;    /* JE rel32 */
      code += 6;
      fixtargets[fxc[middle].index] = *(long*)(code-4) = (supposed_end+5) - code;
      code = fx_writecases(code2, lastcmp, fxc, fixtargets,
                           middle+1, last, supposed_end);
    }
  return code;
}

/* preparation for psyco_write_run_time_switch() */
DEFINEFN
int psyco_build_run_time_switch(fixed_switch_t* rts, long kflags,
				long values[], int count)
{
  code_t* code;
  code_t* codeend;
  code_t* codeorigin = NULL;
  struct fxcase_s* fxc = NULL;
  long* fixtargets = NULL;
  int i, size;
  
    /* a large enough buffer, will be shrunk later */
  codeorigin = (code_t*) PyMem_MALLOC(FX_BASE_SIZE + count * FX_MAX_ITEM_SIZE);
  if (codeorigin == NULL)
    goto out_of_memory;
  fxc = (struct fxcase_s*) PyMem_MALLOC(count *
                                         (sizeof(struct fxcase_s)+sizeof(long)));
  if (fxc == NULL)
    goto out_of_memory;
  /* 'fixtargets' lists the offset of the targets to fix in 'switchcode'.
     It is not stored in the array 'fxc' because it is indexed by the
     original index value, whereas 'fxc' is sorted by value. But we can
     put 'fixtargets' after 'fxc' in the same memory block. */
  fixtargets = (long*) (fxc+count);
  
  for (i=0; i<count; i++)
    {
      fxc[i].value = values[i];
      fxc[i].index = i;
    }
  qsort(fxc, count, sizeof(struct fxcase_s), &fx_compare);

  /* we try to emit the code by supposing that its end is at 'codeend'.
     Depending on the supposition this creates either short or long jumps,
     so it changes the size of the code. We begin by supposing that the
     end is at the beginning, and emit code over and over again until the
     real end stabilizes at the presupposed end position. (It can be proved
     to converge.) */
  codeend = codeorigin;
  while (1)
    {
      code_t* lastcmp = NULL;
      code = fx_writecases(codeorigin, &lastcmp, fxc, fixtargets,
                           0, count, codeend);
      if (lastcmp != NULL)
        *lastcmp = 0;  /* end of list */
      if (code == (codeend + FX_LAST_JUMP_SIZE))
        break;   /* ok, it converged */
      codeend = code - FX_LAST_JUMP_SIZE;   /* otherwise try again */
    }
  /* the LAST_JUMP is a 'JMP rel8', which we do not need (it corresponds
     to the default case jumping to the supposed_end). We overwrite it
     with a 'JMP rel32' which jumps at supposed_end+5, that it,
     at the end of the block. */
  codeend[0] = 0xE9;    /* JMP rel32 */
  codeend += 5;
  *(long*)(codeend-4) = 0;

  size = codeend - codeorigin;
  codeorigin = (code_t*) PyMem_REALLOC(codeorigin, size);
  if (codeorigin == NULL)
    goto out_of_memory;

  rts->switchcodesize = size;
  rts->switchcode = codeorigin;
  rts->count = count;
  rts->fxc = fxc;
  rts->fixtargets = fixtargets;
  rts->zero = 0;
  rts->fixed_promotion.header.compute_fn = &computed_promotion;
  rts->fixed_promotion.fs = rts;
  rts->fixed_promotion.kflags = kflags;
  return 0;

 out_of_memory:
  PyMem_FREE(fxc);
  PyMem_FREE(codeorigin);
  PyErr_NoMemory();
  return -1;
}

DEFINEFN
int psyco_switch_lookup(fixed_switch_t* rts, long value)
{
  /* look-up in the fxc array */
  struct fxcase_s* fxc = rts->fxc;
  int first = 0, last = rts->count;
  while (first < last)
    {
      int middle = (first+last)/2;
      if (fxc[middle].value == value)
        return fxc[middle].index;   /* found, return index */
      if (fxc[middle].value < value)
        first = middle+1;
      else
        last = middle;
    }
  return -1;    /* not found */
}

DEFINEFN
code_t* psyco_write_run_time_switch(fixed_switch_t* rts, code_t* code, reg_t reg)
{
  /* Write the code that does a 'switch' on the prepared 'values'. */
  memcpy(code, rts->switchcode, rts->switchcodesize);

  if (rts->count > 0)
    {
      /* Fix the 1st operand (register number) used by all CMP instruction */
      code_t new_value = 0xC0 | (7<<3) | reg;
      code_t* fix = code+1;   /* 2nd byte of first CMP instruction */
      while (1)
        {
          int offset = *fix;
          *fix = new_value;
          if (offset == 0)
            break;
          fix += offset;
        }
    }
  return code + rts->switchcodesize;
}

DEFINEFN
void psyco_fix_switch_case(fixed_switch_t* rts, code_t* code,
                           int item, code_t* newtarget)
{
/* Fix the target corresponding to the given case. */
  long fixtarget = item<0 ? 0 : rts->fixtargets[item];
  code_t* fixme = code - fixtarget;
  *(long*)(fixme-4) = newtarget - fixme;
}

#endif   /* USE_RUNTIME_SWITCHES */


/*****************************************************************/
 /***   Calling C functions                                     ***/

#define MAX_ARGUMENTS_COUNT    16

DEFINEFN
vinfo_t* psyco_generic_call(PsycoObject* po, void* c_function,
                            int flags, const char* arguments, ...)
{
	char argtags[MAX_ARGUMENTS_COUNT];
	long raw_args[MAX_ARGUMENTS_COUNT], args[MAX_ARGUMENTS_COUNT];
	int count, i, j, stackbase, totalstackspace = 0;
	vinfo_t* vresult;
	bool has_refs = false;

	va_list vargs;

#ifdef HAVE_STDARG_PROTOTYPES
	va_start(vargs, arguments);
#else
	va_start(vargs);
#endif

	for (count=0; arguments[count]; count++) {
		long arg;
		NonVirtualSource src;
		char tag;
		vinfo_t* vi;
		
		extra_assert(count <= MAX_ARGUMENTS_COUNT);
		raw_args[count] = arg = va_arg(vargs, long);
		tag = arguments[count];

		switch (tag) {
			
		case 'l':
			break;
			
		case 'v':
			/* Compute all values first */
			vi = (vinfo_t*) arg;
			src = vinfo_compute(vi, po);
			if (src == SOURCE_ERROR)
				return NULL;
			if (!is_compiletime(src)) {
				flags &= ~CfPure;
			}
			else {
				/* compile-time: get the value */
				arg = CompileTime_Get(src)->value;
				tag = 'l';
			}
			break;

		case 'r':
			/* Push by-reference values in the stack now */
			vi = (vinfo_t*) arg;
			extra_assert(is_runtime(vi->source));
			if (getstack(vi->source) == RunTime_StackNone) {
				reg_t rg = getreg(vi->source);
				if (rg == REG_NONE) {
					/* for undefined sources, pushing
					   just any register will be fine */
					rg = REG_ANY_CALLER_SAVED;
				}
				BEGIN_CODE
				SAVE_REG_VINFO(vi, rg);
				END_CODE
			}
                        arg = RunTime_NewStack(getstack(vi->source),
                                               REG_NONE, false);
			has_refs = true;
			break;

                case 'a':
                case 'A':
			has_refs = true;
			totalstackspace += 4*((vinfo_array_t*) arg)->count;
			break;

		default:
			Py_FatalError("unknown character argument in"
				      " psyco_generic_call()");
		}
		args[count] = arg;
		argtags[count] = tag;
	}
	va_end(vargs);

        if (flags & CfPure) {
                /* calling a pure function with no run-time argument */
                long result;

                if (has_refs) {
                    for (i = 0; i < count; i++) {
                        if (argtags[i] == 'a' || argtags[i] == 'A')
                            args[i] = (long)malloc( ((vinfo_array_t*)args[i])->count );
                        
#if ALL_CHECKS
                        if (argtags[i] == 'r')
                            Py_FatalError("psyco_generic_call(): arg mode "
                            "incompatible with CfPure");
                        
#endif
                    }
                }
                result = psyco_call_var(c_function, count, args);
                if (PyErr_Occurred()) {
                    if (has_refs)
                        for (i = 0; i < count; i++) 
                            if (argtags[i] == 'a' || argtags[i] == 'A')
                                free((void*)args[i]);
                    psyco_virtualize_exception(po);
                    return NULL;
                }
                if (has_refs) {
                    for (i = 0; i < count; i++)
                        if (argtags[i] == 'a' || argtags[i] == 'A') {
                            vinfo_array_t* array = (vinfo_array_t*)raw_args[i];
                            long sk_flag = (argtags[i] == 'a') ? 0 : SkFlagPyObj;
                            for (j = 0; j < array->count; j++) {
                                array->items[j] = vinfo_new(CompileTime_NewSk(
                                    sk_new( ((long*)args[i])[j], sk_flag)));
                            }
                            free((void*)args[i]);
                        }
                }
		switch (flags & CfReturnMask) {

		case CfReturnNormal:
			vresult = vinfo_new(CompileTime_New(result));
			break;
			
		case CfReturnRef:
			vresult = vinfo_new(CompileTime_NewSk(sk_new(result,
								SkFlagPyObj)));
			break;

		default:
			vresult = (vinfo_t*) 1;   /* anything non-NULL */
		}
		return vresult;
	}

	if (has_refs) {
		/* we will need a trash register to compute the references
		   we push later. The following three lines prevent another
		   argument which would currently be in the same trash
		   register from being pushed from the register after we
		   clobbered it. */
		BEGIN_CODE
		NEED_REGISTER(REG_ANY_CALLER_SAVED);
		END_CODE
	}
	
	for (count=0; arguments[count]; count++) {
		if (argtags[count] == 'v') {
			/* We collect all the sources in 'args' now,
			   before SAVE_REGS_FN_CALLS which might move
			   some run-time values into the stack. In this
			   case the old copy in the registers is still
			   useable to PUSH it for the C function call. */
			RunTimeSource src = ((vinfo_t*)(args[count]))->source;
			args[count] = (long) src;
		}
	}

	BEGIN_CODE
	SAVE_REGS_FN_CALLS;
	stackbase = po->stack_depth;
	po->stack_depth += totalstackspace;
	STACK_CORRECTION(totalstackspace);
	for (i=count; i--; ) {
		switch (argtags[i]) {
			
		case 'v':
			CALL_SET_ARG_FROM_RT (args[i],   i, count);
			break;
			
		case 'r':
			LOAD_ADDRESS_FROM_RT (args[i],   REG_ANY_CALLER_SAVED);
			CALL_SET_ARG_FROM_RT (RunTime_New(REG_ANY_CALLER_SAVED,
							  false),  i, count);
			break;
			
		case 'a':
		case 'A':
		{
			vinfo_array_t* array = (vinfo_array_t*) args[i];
			bool with_reference = (argtags[i] == 'A');
			int j = array->count;
			if (j > 0) {
				do {
					stackbase += 4;
					array->items[--j] = vinfo_new
						(RunTime_NewStack(stackbase,
								REG_NONE,
								with_reference));
				} while (j);
				LOAD_ADDRESS_FROM_RT (array->items[0]->source,
						      REG_ANY_CALLER_SAVED);
			}
			CALL_SET_ARG_FROM_RT (RunTime_New(REG_ANY_CALLER_SAVED,
							  false),  i, count);
			break;
		}
			
		default:
			CALL_SET_ARG_IMMED   (args[i],   i, count);
			break;
		}
	}
	CALL_C_FUNCTION                      (c_function,   count);
	END_CODE

	switch (flags & CfReturnMask) {

	case CfReturnNormal:
		vresult = new_rtvinfo(po, REG_FUNCTIONS_RETURN, false);
		break;

	case CfReturnRef:
		vresult = new_rtvinfo(po, REG_FUNCTIONS_RETURN, true);
		break;

	default:
		if ((flags & CfPyErrMask) == 0)
			return (vinfo_t*) 1;   /* anything non-NULL */
		
		vresult = new_rtvinfo(po, REG_FUNCTIONS_RETURN, false);
		vresult = generic_call_check(po, flags, vresult);
		if (vresult == NULL)
			goto error_detected;
		vinfo_decref(vresult, po);
		return (vinfo_t*) 1;   /* anything non-NULL */
	}
	
        if (flags & CfPyErrMask) {
		vresult = generic_call_check(po, flags, vresult);
		if (vresult == NULL)
			goto error_detected;
	}
	return vresult;

   error_detected:
	/* if the called function returns an error, we then assume that
	   it did not actually fill the arrays */
	if (has_refs) {
		for (i = 0; i < count; i++)
			if (argtags[i] == 'a' || argtags[i] == 'A') {
				vinfo_array_t* array = (vinfo_array_t*)args[i];
				int j = array->count;
				while (j--) {
					vinfo_t* v = array->items[j];
					array->items[j] = NULL;
					v->source = remove_rtref(v->source);
					vinfo_decref(v, po);
				}
                        }
	}
	return NULL;
}


DEFINEFN
vinfo_t* psyco_call_psyco(PsycoObject* po, CodeBufferObject* codebuf,
			  Source argsources[], int argcount,
			  struct stack_frame_info_s* finfo)
{
	/* this is a simplified version of psyco_generic_call() which
	   assumes Psyco's calling convention instead of the C's. */
	int i, initial_depth;
	bool ccflags;
	Source* p;
	BEGIN_CODE
          /* cannot use NEED_CC() */
	ccflags = (po->ccreg != NULL);
	if (ccflags)
		PUSH_CC_FLAGS();
	for (i=0; i<REG_TOTAL; i++)
		NEED_REGISTER(i);
	finfo->stack_depth = po->stack_depth;
	SAVE_IMMED_TO_EBP_BASE((long) finfo, INITIAL_STACK_DEPTH);
	initial_depth = po->stack_depth;
	CALL_SET_ARG_IMMED(-1, argcount, argcount+1);
	p = argsources;
	for (i=argcount; i--; p++)
		CALL_SET_ARG_FROM_RT(*p, i, argcount+1);
	CALL_C_FUNCTION(codebuf->codeptr,   argcount+1);
	po->stack_depth = initial_depth;  /* callee removes arguments */
	SAVE_IMM8_TO_EBP_BASE(-1, INITIAL_STACK_DEPTH);
	if (ccflags)
		POP_CC_FLAGS();
	END_CODE
	return generic_call_check(po, CfReturnRef|CfPyErrIfNull,
				  new_rtvinfo(po, REG_FUNCTIONS_RETURN, true));
}

DEFINEFN struct stack_frame_info_s**
psyco_next_stack_frame(struct stack_frame_info_s** finfo)
{
	/* Hack to pick directly from the machine stack the stored
	   "stack_frame_info_t*" pointers */
	return (struct stack_frame_info_s**)
		(((char*) finfo) - (*finfo)->stack_depth);
}


/*****************************************************************/
 /***   Emit common instructions                                ***/

DEFINEFN
condition_code_t integer_non_null(PsycoObject* po, vinfo_t* vi)
{
	condition_code_t result;
	
	if (is_virtualtime(vi->source)) {
		result = psyco_vsource_cc(vi->source);
		if (result != CC_ALWAYS_FALSE)
			return result;
		if (vinfo_compute(vi, po) == SOURCE_ERROR)
			return CC_ERROR;
	}
	if (is_compiletime(vi->source)) {
		if (KNOWN_SOURCE(vi)->value != 0)
			return CC_ALWAYS_TRUE;
		else
			return CC_ALWAYS_FALSE;
	}
	BEGIN_CODE
	CHECK_NONZERO_FROM_RT(vi->source);
	END_CODE
	return CHECK_NONZERO_CONDITION;
}

DEFINEFN
condition_code_t integer_NON_NULL(PsycoObject* po, vinfo_t* vi)
{
	condition_code_t result;

	if (vi == NULL)
		return CC_ERROR;

        result = integer_non_null(po, vi);

	/* 'vi' cannot be a reference to a Python object if we are
	   asking ourselves if it is NULL or not. So the following
	   vinfo_decref() will not emit a Py_DECREF() that would
	   clobber the condition code. We check all this. */
#if ALL_CHECKS
	assert(!has_rtref(vi->source));
	{ code_t* code1 = po->code;
#endif
	vinfo_decref(vi, po);
#if ALL_CHECKS
	assert(po->code == code1); }
#endif
	return result;
}

#define GENERIC_BINARY_HEADER                   \
  NonVirtualSource v1s, v2s;                    \
  v2s = vinfo_compute(v2, po);                  \
  if (v2s == SOURCE_ERROR) return NULL;         \
  v1s = vinfo_compute(v1, po);                  \
  if (v1s == SOURCE_ERROR) return NULL;

#define GENERIC_BINARY_HEADER_i                 \
  NonVirtualSource v1s;                         \
  v1s = vinfo_compute(v1, po);                  \
  if (v1s == SOURCE_ERROR) return NULL;

#define GENERIC_BINARY_CT_CT(c_code)                    \
  if (is_compiletime(v1s) && is_compiletime(v2s))       \
    {                                                   \
      long a = CompileTime_Get(v1s)->value;             \
      long b = CompileTime_Get(v2s)->value;             \
      long c = (c_code);                                \
      return vinfo_new(CompileTime_New(c));             \
    }

#define GENERIC_BINARY_COMMON_INSTR(group, ovf)   {             \
  reg_t rg;                                                     \
  BEGIN_CODE                                                    \
  NEED_CC_SRC(v2s);                                             \
  DONT_OVERWRITE_SOURCE(v2s);                                   \
  COPY_IN_REG(v1, rg);                   /* MOV rg, (v1) */     \
  COMMON_INSTR_FROM(group, rg, v2s);     /* XXX rg, (v2) */     \
  END_CODE                                                      \
  if ((ovf) && runtime_condition_f(po, CC_O))                   \
    return NULL;  /* if overflow */                             \
  return new_rtvinfo(po, rg, false);                            \
}

#define GENERIC_BINARY_INSTR_2(group, c_code)                           \
{                                                                       \
  NonVirtualSource v1s = vinfo_compute(v1, po);                         \
  if (v1s == SOURCE_ERROR) return NULL;				        \
  if (is_compiletime(v1s))                                              \
    {                                                                   \
      long a = CompileTime_Get(v1s)->value;                             \
      long b = value2;                                                  \
      long c = (c_code);                                                \
      return vinfo_new(CompileTime_New(c));                             \
    }                                                                   \
  else                                                                  \
    {                                                                   \
      reg_t rg;                                                         \
      BEGIN_CODE                                                        \
      NEED_CC();                                                        \
      COPY_IN_REG(v1, rg);                   /* MOV rg, (v1) */         \
      COMMON_INSTR_IMMED(group, rg, value2); /* XXX rg, value2 */       \
      END_CODE                                                          \
      return new_rtvinfo(po, rg, false);                                \
    }                                                                   \
}

static vinfo_t* int_add_i(PsycoObject* po, RunTimeSource v1s, long value2)
{
  reg_t rg, dst;
  BEGIN_CODE
  NEED_FREE_REG(dst);
  rg = getreg(v1s);
  if (rg == REG_NONE)
    {
      rg = dst;
      LOAD_REG_FROM(v1s, rg);
    }
  LOAD_REG_FROM_REG_PLUS_IMMED(dst, rg, value2);
  END_CODE
  return new_rtvinfo(po, dst, false);
}

DEFINEFN
vinfo_t* integer_add(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf)
{
  GENERIC_BINARY_HEADER
  if (is_compiletime(v1s))
    {
      long a = CompileTime_Get(v1s)->value;
      if (a == 0)
        {
          /* adding zero to v2 */
          vinfo_incref(v2);
          return v2;
        }
      if (is_compiletime(v2s))
        {
          long b = CompileTime_Get(v2s)->value;
          long c = a + b;
          if (ovf && (c^a) < 0 && (c^b) < 0)
            return NULL;   /* overflow */
          return vinfo_new(CompileTime_New(c));
        }
      if (!ovf)
        return int_add_i(po, v2s, a);
    }
  else
    if (is_compiletime(v2s))
      {
        long b = CompileTime_Get(v2s)->value;
        if (b == 0)
          {
            /* adding zero to v1 */
            vinfo_incref(v1);
            return v1;
          }
        if (!ovf)
          return int_add_i(po, v1s, b);
      }

  GENERIC_BINARY_COMMON_INSTR(0, ovf)   /* ADD */
}

DEFINEFN
vinfo_t* integer_add_i(PsycoObject* po, vinfo_t* v1, long value2)
{
  if (value2 == 0)
    {
      /* adding zero to v1 */
      vinfo_incref(v1);
      return v1;
    }
  else
    {
      GENERIC_BINARY_HEADER_i
      if (is_compiletime(v1s))
        {
          long c = CompileTime_Get(v1s)->value + value2;
          return vinfo_new(CompileTime_New(c));
        }
      return int_add_i(po, v1s, value2);
    }
}

DEFINEFN
vinfo_t* integer_sub(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf)
{
  GENERIC_BINARY_HEADER
  if (is_compiletime(v1s))
    {
      long a = CompileTime_Get(v1s)->value;
      if (is_compiletime(v2s))
        {
          long b = CompileTime_Get(v2s)->value;
          long c = a - b;
          if (ovf && (c^a) < 0 && (c^~b) < 0)
            return NULL;   /* overflow */
          return vinfo_new(CompileTime_New(c));
        }
    }
  else
    if (is_compiletime(v2s))
      {
        long b = CompileTime_Get(v2s)->value;
        if (b == 0)
          {
            /* subtracting zero from v1 */
            vinfo_incref(v1);
            return v1;
          }
        if (!ovf)
          return int_add_i(po, v1s, -b);
      }

  GENERIC_BINARY_COMMON_INSTR(5, ovf)   /* SUB */
}

DEFINEFN
vinfo_t* integer_or(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  GENERIC_BINARY_HEADER
  GENERIC_BINARY_CT_CT(a | b)
  GENERIC_BINARY_COMMON_INSTR(1, false)   /* OR */
}

DEFINEFN
vinfo_t* integer_and(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  GENERIC_BINARY_HEADER
  GENERIC_BINARY_CT_CT(a & b)
  GENERIC_BINARY_COMMON_INSTR(4, false)   /* AND */
}

DEFINEFN
vinfo_t* integer_and_i(PsycoObject* po, vinfo_t* v1, long value2)
     GENERIC_BINARY_INSTR_2(4, a & b)    /* AND */

#define GENERIC_SHIFT_BY(rtmacro)                       \
  {                                                     \
    reg_t rg;                                           \
    extra_assert(0 < counter && counter < LONG_BIT);    \
    BEGIN_CODE                                          \
    NEED_CC();                                          \
    COPY_IN_REG(v1, rg);                                \
    rtmacro(rg, counter);                               \
    END_CODE                                            \
    return new_rtvinfo(po, rg, false);                  \
  }

static vinfo_t* int_lshift_i(PsycoObject* po, vinfo_t* v1, int counter)
     GENERIC_SHIFT_BY(SHIFT_LEFT_BY)

static vinfo_t* int_rshift_i(PsycoObject* po, vinfo_t* v1, int counter)
     GENERIC_SHIFT_BY(SHIFT_SIGNED_RIGHT_BY)

static vinfo_t* int_urshift_i(PsycoObject* po, vinfo_t* v1, int counter)
     GENERIC_SHIFT_BY(SHIFT_RIGHT_BY)

static vinfo_t* int_mul_i(PsycoObject* po, vinfo_t* v1, long value2,
                          bool ovf)
{
  switch (value2) {
  case 0:
    return vinfo_new(CompileTime_New(0));
  case 1:
    vinfo_incref(v1);
    return v1;
  }
  if (((value2-1) & value2) == 0 && value2 >= 0 && !ovf)
    {
      /* value2 is a power of two */
      return int_lshift_i(po, v1, intlog2(value2));
    }
  else
    {
      reg_t rg;
      RunTimeSource v1s = v1->source;
      BEGIN_CODE
      NEED_CC_SRC(v1s);
      DONT_OVERWRITE_SOURCE(v1s);
      NEED_FREE_REG(rg);
      IMUL_IMMED_FROM_RT(v1s, value2, rg);
      END_CODE
      if (ovf && runtime_condition_f(po, CC_O))
        return NULL;
      return new_rtvinfo(po, rg, false);
    }
}

DEFINEFN
vinfo_t* integer_mul(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf)
{
  reg_t rg;
  GENERIC_BINARY_HEADER
  if (is_compiletime(v1s))
    {
      long a = CompileTime_Get(v1s)->value;
      if (is_compiletime(v2s))
        {
          long b = CompileTime_Get(v2s)->value;
          /* unlike Python, we use a function written in assembly
             to perform the product overflow checking */
          if (ovf && glue_int_mul_1(a, b))
            return NULL;   /* overflow */
          return vinfo_new(CompileTime_New(a * b));
        }
      return int_mul_i(po, v2, a, ovf);
    }
  else
    if (is_compiletime(v2s))
      {
        long b = CompileTime_Get(v2s)->value;
        return int_mul_i(po, v1, b, ovf);
      }
  
  BEGIN_CODE
  NEED_CC_SRC(v2s);
  DONT_OVERWRITE_SOURCE(v2s);
  COPY_IN_REG(v1, rg);              /* MOV rg, (v1) */
  IMUL_REG_FROM_RT(v2s, rg);        /* IMUL rg, (v2) */
  END_CODE
  if (ovf && runtime_condition_f(po, CC_O))
    return NULL;  /* if overflow */
  return new_rtvinfo(po, rg, false);
}

DEFINEFN
vinfo_t* integer_mul_i(PsycoObject* po, vinfo_t* v1, long value2)
{
  GENERIC_BINARY_HEADER_i
  if (is_compiletime(v1s))
    {
      long c = CompileTime_Get(v1s)->value * value2;
      return vinfo_new(CompileTime_New(c));
    }
  return int_mul_i(po, v1, value2, false);
}

DEFINEFN
vinfo_t* integer_lshift_i(PsycoObject* po, vinfo_t* v1, long counter)
{
  GENERIC_BINARY_HEADER_i
  if (0 < counter && counter < LONG_BIT)
    {
      if (is_compiletime(v1s))
        {
          long c = CompileTime_Get(v1s)->value << counter;
          return vinfo_new(CompileTime_New(c));
        }
      else
        return int_lshift_i(po, v1, counter);
    }
  else if (counter == 0)
    {
      vinfo_incref(v1);
      return v1;
    }
  else if (counter >= LONG_BIT)
    return vinfo_new(CompileTime_New(0));
  else
    {
      PycException_SetString(po, PyExc_ValueError, "negative shift count");
      return NULL;
    }
}

DEFINEFN
vinfo_t* integer_urshift_i(PsycoObject* po, vinfo_t* v1, long counter)
{
  GENERIC_BINARY_HEADER_i
  if (0 < counter && counter < LONG_BIT)
    {
      if (is_compiletime(v1s))
        {
          long c = ((unsigned long)(CompileTime_Get(v1s)->value)) >> counter;
          return vinfo_new(CompileTime_New(c));
        }
      else
        return int_urshift_i(po, v1, counter);
    }
  else if (counter == 0)
    {
      vinfo_incref(v1);
      return v1;
    }
  else if (counter >= LONG_BIT)
    return vinfo_new(CompileTime_New(0));
  else
    {
      PycException_SetString(po, PyExc_ValueError, "negative shift count");
      return NULL;
    }
}

/* DEFINEFN */
/* vinfo_t* integer_lshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) */
/* { */
/*   NonVirtualSource v1s, v2s; */
/*   v2s = vinfo_compute(v2, po); */
/*   if (v2s == SOURCE_ERROR) return NULL; */
/*   if (is_compiletime(v2s)) */
/*     return integer_lshift_i(po, v1, CompileTime_Get(v2s)->value); */
  
/*   v1s = vinfo_compute(v1, po); */
/*   if (v1s == SOURCE_ERROR) return NULL; */
/*   XXX implement me */
/* } */


#define GENERIC_UNARY_INSTR(rtmacro, c_code, ovf, c_ovf, cond_ovf)      \
{                                                                       \
  NonVirtualSource v1s = vinfo_compute(v1, po);                         \
  if (v1s == SOURCE_ERROR) return NULL;				        \
  if (is_compiletime(v1s))                                              \
    {                                                                   \
      long a = CompileTime_Get(v1s)->value;                             \
      long c = (c_code);                                                \
      if (!((ovf) && (c_ovf)))                                          \
        return vinfo_new(CompileTime_New(c));                           \
    }                                                                   \
  else                                                                  \
    {                                                                   \
      reg_t rg;                                                         \
      BEGIN_CODE                                                        \
      NEED_CC();                                                        \
      COPY_IN_REG(v1, rg);                  /* MOV rg, (v1) */          \
      rtmacro;                              /* XXX rg       */          \
      END_CODE                                                          \
      if (!((ovf) && runtime_condition_f(po, cond_ovf)))                \
        return new_rtvinfo(po, rg, false);                              \
    }                                                                   \
  return NULL;                                                          \
}

DEFINEFN
vinfo_t* integer_not(PsycoObject* po, vinfo_t* v1)
  GENERIC_UNARY_INSTR(UNARY_INSTR_ON_REG(2, rg), ~a,
                           false, false, CC_ALWAYS_FALSE)

DEFINEFN
vinfo_t* integer_neg(PsycoObject* po, vinfo_t* v1, bool ovf)
  GENERIC_UNARY_INSTR(UNARY_INSTR_ON_REG(3, rg), -a,
                           ovf, c == (-LONG_MAX-1), CC_O)

DEFINEFN
vinfo_t* integer_abs(PsycoObject* po, vinfo_t* v1, bool ovf)
  GENERIC_UNARY_INSTR(INT_ABS(rg, v1->source), a<0 ? -a : a,
                           ovf, c == (-LONG_MAX-1), CHECK_ABS_OVERFLOW)


static const condition_code_t direct_results[16] = {
	  /*****   signed comparison      **/
          /* Py_LT: */  CC_L,
          /* Py_LE: */  CC_LE,
          /* Py_EQ: */  CC_E,
          /* Py_NE: */  CC_NE,
          /* Py_GT: */  CC_G,
          /* Py_GE: */  CC_GE,
	  /* (6)    */  CC_ERROR,
	  /* (7)    */  CC_ERROR,
	  /*****  unsigned comparison     **/
          /* Py_LT: */  CC_uL,
          /* Py_LE: */  CC_uLE,
          /* Py_EQ: */  CC_E,
          /* Py_NE: */  CC_NE,
          /* Py_GT: */  CC_uG,
          /* Py_GE: */  CC_uGE,
	  /* (14)   */  CC_ERROR,
	  /* (15)   */  CC_ERROR };

static const condition_code_t inverted_results[16] = {
	  /*****   signed comparison      **/
          /* inverted Py_LT: */  CC_G,
          /* inverted Py_LE: */  CC_GE,
          /* inverted Py_EQ: */  CC_E,
          /* inverted Py_NE: */  CC_NE,
          /* inverted Py_GT: */  CC_L,
          /* inverted Py_GE: */  CC_LE,
	  /* (6)             */  CC_ERROR,
	  /* (7)             */  CC_ERROR,
	  /*****  unsigned comparison     **/
          /* inverted Py_LT: */  CC_uG,
          /* inverted Py_LE: */  CC_uGE,
          /* inverted Py_EQ: */  CC_E,
          /* inverted Py_NE: */  CC_NE,
          /* inverted Py_GT: */  CC_uL,
          /* inverted Py_GE: */  CC_uLE,
	  /* (14)            */  CC_ERROR,
	  /* (15)            */  CC_ERROR };

inline condition_code_t immediate_compare(long a, long b, int py_op)
{
  switch (py_op) {
    case Py_LT:  return a < b  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_LE:  return a <= b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_EQ|COMPARE_UNSIGNED:
    case Py_EQ:  return a == b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_NE|COMPARE_UNSIGNED:
    case Py_NE:  return a != b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_GT:  return a > b  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_GE:  return a >= b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;

  case Py_LT|COMPARE_UNSIGNED:  return ((unsigned long) a) <  ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  case Py_LE|COMPARE_UNSIGNED:  return ((unsigned long) a) <= ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  case Py_GT|COMPARE_UNSIGNED:  return ((unsigned long) a) >  ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  case Py_GE|COMPARE_UNSIGNED:  return ((unsigned long) a) >= ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  default:
    Py_FatalError("immediate_compare(): bad py_op");
    return CC_ERROR;
  }
}

DEFINEFN
condition_code_t integer_cmp(PsycoObject* po, vinfo_t* v1,
                             vinfo_t* v2, int py_op)
{
  condition_code_t result;
  NonVirtualSource v1s;
  NonVirtualSource v2s;
  
  if (v1->source == v2->source)
    goto same_source;

  v1s = vinfo_compute(v1, po);
  if (v1s == SOURCE_ERROR) return CC_ERROR;
  v2s = vinfo_compute(v2, po);
  if (v2s == SOURCE_ERROR) return CC_ERROR;

  if (v1s == v2s)
    {
    same_source:
      /* comparing equal sources */
      switch (py_op & ~ COMPARE_UNSIGNED) {
      case Py_LE:
      case Py_EQ:
      case Py_GE:
        return CC_ALWAYS_TRUE;
      default:
        return CC_ALWAYS_FALSE;
      }
    }
  if (is_compiletime(v1s))
    if (is_compiletime(v2s))
      {
        long a = CompileTime_Get(v1s)->value;
        long b = CompileTime_Get(v2s)->value;
        return immediate_compare(a, b, py_op);
      }
    else
      {
        NonVirtualSource tmp;
        /* invert the two operands because the processor has only CMP xxx,immed
           and not CMP immed,xxx */
        result = inverted_results[py_op];
        tmp = v1s;
        v1s = v2s;
        v2s = tmp;
      }
  else
    {
      result = direct_results[py_op];
    }
  BEGIN_CODE
  NEED_CC();
  if (is_compiletime(v2s))
    COMPARE_IMMED_FROM_RT(v1s, CompileTime_Get(v2s)->value); /* CMP v1, immed2 */
  else
    {
      RTVINFO_IN_REG(v1);         /* CMP v1, v2 */
      COMMON_INSTR_FROM_RT(7, getreg(v1->source), v2->source);
    }
  END_CODE
  return result;
}

DEFINEFN
condition_code_t integer_cmp_i(PsycoObject* po, vinfo_t* v1,
                               long value2, int py_op)
{
  NonVirtualSource v1s = vinfo_compute(v1, po);
  if (v1s == SOURCE_ERROR) return CC_ERROR;
  
  if (is_compiletime(v1s))
    {
      long a = CompileTime_Get(v1s)->value;
      return immediate_compare(a, value2, py_op);
    }
  else
    {
      BEGIN_CODE
      NEED_CC_SRC(v1s);
      COMPARE_IMMED_FROM_RT(v1s, value2);  /* CMP v1, immed2 */
      END_CODE
      return direct_results[py_op];
    }
}

#if 0
DEFINEFN      (not used)
vinfo_t* integer_seqindex(PsycoObject* po, vinfo_t* vi, vinfo_t* vn, bool ovf)
{
  NonVirtualSource vns, vis;
  vns = vinfo_compute(vn, po);
  if (vns == SOURCE_ERROR) return NULL;
  vis = vinfo_compute(vi, po);
  if (vis == SOURCE_ERROR) return NULL;
  
  if (!is_compiletime(vis))
    {
      reg_t rg, tmprg;
      BEGIN_CODE
      NEED_CC_SRC(vis);
      NEED_FREE_REG(rg);
      LOAD_REG_FROM_RT(vis, rg);
      DELAY_USE_OF(rg);
      NEED_FREE_REG(tmprg);

      /* Increase 'rg' by 'vns' unless it is already in the range(0, vns). */
         /* CMP i, n */
      vns = vn->source;  /* reload, could have been moved by NEED_FREE_REG */
      COMMON_INSTR_FROM(7, rg, vns);
         /* SBB t, t */
      COMMON_INSTR_FROM_RT(3, tmprg, RunTime_New(tmprg, false));
         /* AND t, n */
      COMMON_INSTR_FROM(4, tmprg, vns);
         /* SUB i, t */
      COMMON_INSTR_FROM_RT(5, rg, RunTime_New(tmprg, false));
         /* ADD i, n */
      COMMON_INSTR_FROM(0, rg, vns);
      END_CODE

      if (ovf && runtime_condition_f(po, CC_NB))  /* if out of range */
        return NULL;
      return new_rtvinfo(po, rg, false);
    }
  else
    {
      long index = CompileTime_Get(vis)->value;
      long reqlength;
      if (index >= 0)
        reqlength = index;  /* index is known, length must be greater than it */
      else
        reqlength = ~index;  /* idem for negative index */
      if (ovf)
        {
          /* test for out of range index -- more precisely, test that the
             length is not large enough for the known index */
          condition_code_t cc = integer_cmp_i(po, vn, reqlength, Py_LE);
          if (cc == CC_ERROR || runtime_condition_f(po, cc))
            return NULL;
        }
      if (index >= 0)
        {
          vinfo_incref(vi);
          return vi;
        }
      else
        return integer_add_i(po, vn, index);
    }
}
#endif  /* 0 */

DEFINEFN
vinfo_t* make_runtime_copy(PsycoObject* po, vinfo_t* v)
{
	reg_t rg;
	NonVirtualSource src = vinfo_compute(v, po);
	if (src == SOURCE_ERROR)
		return NULL;
	BEGIN_CODE
	NEED_FREE_REG(rg);
	LOAD_REG_FROM(src, rg);
	END_CODE
	return new_rtvinfo(po, rg, false);
}
