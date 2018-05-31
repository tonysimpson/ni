#include "../processor.h"
#include "iencoding.h"
#include "../dispatcher.h"
#include "../codemanager.h"
#include "../Python/frames.h"


typedef PyObject* (*glue_run_code_fn) (code_t* code_target,
				       long* stack_end,
				       long* initial_stack,
				       struct stack_frame_info_s*** finfo);


static glue_run_code_fn glue_run_code;


static void write_glue_run_code_fn(PsycoObject *po) {
    BEGIN_CODE
    PUSH_R(REG_X64_RBP);
    PUSH_R(REG_X64_RBX);
    PUSH_R(REG_X64_R13);
    PUSH_R(REG_X64_R14);
    PUSH_R(REG_X64_R15);

    MOV_R_R(REG_TRANSIENT_1, REG_X64_RSP);
    SUB_R_I8(REG_TRANSIENT_1, 8);
    PUSH_R(REG_TRANSIENT_1);

    PUSH_I(-1);
    MOV_A_R(REG_X64_RCX, REG_X64_RSP);
    BEGIN_SHORT_JUMP(0);
    BEGIN_REVERSE_SHORT_JUMP(1);
    SUB_R_I8(REG_X64_RSI, 8);
    PUSH_A(REG_X64_RSI);
    END_SHORT_JUMP(0);
    CMP_R_R(REG_X64_RDX, REG_X64_RSI);
    END_REVERSE_SHORT_COND_JUMP(1, CC_NE);
    BEGIN_CALL();
    END_CALL_R(REG_X64_RDI);
    SUB_R_I8(REG_X64_RSP, 16);
    POP_R(REG_X64_R15);
    POP_R(REG_X64_R14);
    POP_R(REG_X64_R13);
    POP_R(REG_X64_RBX);
    POP_R(REG_X64_RBP);
    RET();
    END_CODE
}


DEFINEFN
PyObject* psyco_processor_run(CodeBufferObject* codebuf,
                              long initial_stack[],
                              struct stack_frame_info_s*** finfo,
                              PyObject* tdict)
{
  int argc = RUN_ARGC(codebuf);
  return glue_run_code(codebuf->codestart, initial_stack + argc,
                         initial_stack, finfo);
}


typedef char (*psyco_int_mul_ovf_fn) (long a, long b);


psyco_int_mul_ovf_fn psyco_int_mul_ovf;


void write_psyco_int_mul_ovf(PsycoObject *po) {
    BEGIN_CODE
    IMUL_R_R(REG_X64_RDI, REG_X64_RSI);
    SET_R_CC(REG_FUNCTIONS_RETURN, CC_O);
    RET();
    END_CODE
}


DEFINEFN struct stack_frame_info_s**
psyco_next_stack_frame(struct stack_frame_info_s** finfo)
{
	/* Hack to pick directly from the machine stack the stored
	   "stack_frame_info_t*" pointers */
	return (struct stack_frame_info_s**)
		(((char*) finfo) - finfo_last(*finfo)->link_stack_depth);
}


INITIALIZATIONFN
void psyco_processor_init(void)
{
    code_t *limit;
    CodeBufferObject* codebuf = psyco_new_code_buffer(NULL, NULL, &limit);
    PsycoObject *po = PsycoObject_New(0);
    po->code = codebuf->codestart;
    po->codelimit = limit;
    glue_run_code = (glue_run_code_fn)po->code;
    write_glue_run_code_fn(po);
    psyco_int_mul_ovf = (psyco_int_mul_ovf_fn)po->code;
    write_psyco_int_mul_ovf(po);
    SHRINK_CODE_BUFFER(codebuf, po->code, "glue");
}


