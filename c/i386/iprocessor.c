#include "../processor.h"
#include "iencoding.h"
//#include "../vcompiler.h"
#include "../dispatcher.h"
#include "../codemanager.h"
//#include "../Python/pycompiler.h"  /* for exception handling stuff */
#include "../Python/frames.h"
#include "../timing.h"     /* for PENTIUM_TSC */
//#include "ipyencoding.h"


/* define to copy static machine code in the heap before running it.
   I've seen some Linux distributions in which the static data pages
   are not executable by default. */
#define COPY_CODE_IN_HEAP


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
                              long initial_stack[],
                              struct stack_frame_info_s*** finfo,
                              PyObject* tdict)
{
  int argc = RUN_ARGC(codebuf);
  return glue_run_code_1(codebuf->codestart, initial_stack + argc,
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
DEFINEVAR char (*psyco_int_mul_ovf) (long a, long b);

static code_t glue_int_mul[] = {
  0x8B, 0x44, 0x24, 8,          /*   MOV  EAX, [ESP+8]  (a)   */
  0x0F, 0xAF, 0x44, 0x24, 4,    /*   IMUL EAX, [ESP+4]  (b)   */
  0x0F, 0x90, 0xC0,             /*   SETO AL                  */
  0xC3,                         /*   RET                      */
};


#ifdef PENTIUM_TSC  /* if itiming.h is included by timing.h */
static code_t glue_pentium_tsc[] = {
  0x0F, 0x31,                   /*   RDTSC   */
  0xC3,                         /*   RET     */
};
DEFINEVAR psyco_pentium_tsc_fn psyco_pentium_tsc;
#endif /* PENTIUM_TSC */


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


INITIALIZATIONFN
void psyco_processor_init(void)
{
#ifdef COPY_CODE_IN_HEAP
  COPY_CODE(glue_run_code_1,    glue_run_code,     glue_run_code_fn);
#endif
#ifdef PENTIUM_TSC
  COPY_CODE(psyco_pentium_tsc,  glue_pentium_tsc,  psyco_pentium_tsc_fn);
#endif
  COPY_CODE(psyco_int_mul_ovf,  glue_int_mul,      char(*)(long, long));
  COPY_CODE(psyco_call_var,     glue_call_var,     long(*)(void*, int, long[]));
}


DEFINEFN struct stack_frame_info_s**
psyco_next_stack_frame(struct stack_frame_info_s** finfo)
{
	/* Hack to pick directly from the machine stack the stored
	   "stack_frame_info_t*" pointers */
	return (struct stack_frame_info_s**)
		(((char*) finfo) - finfo_last(*finfo)->link_stack_depth);
}
