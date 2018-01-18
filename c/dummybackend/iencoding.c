#include "iencoding.h"
#include "../vcompiler.h"
#include "../codegen.h"
#include "../dispatcher.h"
#include "../codemanager.h"
#include "../Python/frames.h"

code_t* psyco_compute_cc(PsycoObject* po, code_t* code, reg_t reserved) {
    return NULL;
}

vinfo_t* bint_add_i(PsycoObject* po, vinfo_t* rt1, long value2, bool unsafe) {
    return NULL;
}

vinfo_t* bint_mul_i(PsycoObject* po, vinfo_t* v1, long value2, bool ovf) {
    return NULL;
}

vinfo_t* bint_lshift_i(PsycoObject* po, vinfo_t* v1, int counter) {
    return NULL;
}

vinfo_t* bint_rshift_i(PsycoObject* po, vinfo_t* v1, int counter) {
    return NULL;
}

vinfo_t* bint_urshift_i(PsycoObject* po, vinfo_t* v1, int counter) {
    return NULL;
}

condition_code_t bint_cmp_i(PsycoObject* po, int base_py_op, vinfo_t* rt1, long immed2)
{
    return CC_ALWAYS_TRUE;
}

vinfo_t* bfunction_result(PsycoObject* po, bool ref) {
    return NULL;
}

void* psyco_call_code_builder(PsycoObject* po, void* fn, int restore, RunTimeSource extraarg) {
    return NULL;
}

vinfo_t* psyco_call_psyco(PsycoObject* po, CodeBufferObject* codebuf, Source argsources[], int argcount, struct stack_frame_info_s* finfo)
{
    return NULL;
}

vinfo_t* psyco_memory_read(PsycoObject* po, vinfo_t* nv_ptr, long offset,vinfo_t* rt_vindex, int size2, bool nonsigned)
{
        return NULL;
}

bool psyco_memory_write(PsycoObject* po, vinfo_t* nv_ptr, long offset, vinfo_t* rt_vindex, int size2, vinfo_t* value)
{
        return true;
}

vinfo_t* make_runtime_copy(PsycoObject* po, vinfo_t* v) {
    return NULL;
}
