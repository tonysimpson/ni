#include <Python.h>
#include "Python/pyver.h"
#include "codemanager.h"
#include "Python/frames.h"
#include "mergepoints.h"

#if PY3
#error "Not Yet"
#else
typedef PyObject* (*PyEval_EvalFrameExFunction)(PyFrameObject *, int);

EXTERNVAR PyEval_EvalFrameExFunction original_eval_frame;
#endif /* !PY3 */

static bool ni_register_start_frame_runtime(PyFrameObject* f, stack_frame_info_t*** finfo) {
    PyObject* tdict;
    PyFrameRuntime* fruntime;
    tdict = psyco_thread_dict();
    if (tdict==NULL) {
        return false;
    }
    fruntime = PyCStruct_NEW(PyFrameRuntime, PyFrameRuntime_dealloc);
    Py_INCREF(f);
    fruntime->cs_key = (PyObject*) f;
    fruntime->psy_frames_start = finfo;
    fruntime->psy_code = f->f_code;
    fruntime->psy_globals = f->f_globals;
    extra_assert(PyDict_GetItem(tdict, (PyObject*) f) == NULL);
    if(PyDict_SetItem(tdict, (PyObject*) f, (PyObject*) fruntime) == -1) {
        Py_DECREF(fruntime);
        return false;
    }
    Py_DECREF(fruntime);
    return true;
}

static bool ni_unregister_start_frame_runtime(PyFrameObject* f) {
    PyObject* tdict;
    tdict = psyco_thread_dict();
    if (tdict==NULL) {
        return false;
    }
    if (PyDict_DelItem(tdict, (PyObject*)f) == -1) {
        return false;
    }
    return true;
}

static PyObject* NiCode_Run(PyObject* codebuf, PyFrameObject* f) {
    stack_frame_info_t** finfo;
    PyObject* result;

    if(!ni_register_start_frame_runtime(f, &finfo)) {
        return NULL;
    }
    /* tdict arg is NULL here as it is not used - not sure why */
    result = psyco_processor_run((CodeBufferObject*)codebuf, (long*)f->f_localsplus, &finfo, NULL);
    psyco_trash_object(NULL);  /* free any trashed object now */
    if (!ni_unregister_start_frame_runtime(f)) {
        Py_XDECREF(result);
        return NULL;
    }
    return result;
}

#define DEFAULT_RECURSION 10

static PyObject* NiEval_EvalFrame(PyFrameObject *frame, int throwflag) {
    /* Don't compile modules - buggy */
    if (frame->f_globals != frame->f_locals) {
        PyCodeObject * co;
        PyObject *mergepoints;
        co = frame->f_code;
        mergepoints = psyco_get_merge_points(co, 0);
        if (mergepoints != Py_None) {
            PyObject *entry_point;
            entry_point = PyDict_GetItem(co_to_entry_point, (PyObject*)co);
            if (entry_point == NULL) {
                /* Not compiled yet */
                PsycoObject *po;
                CodeBufferObject *codebuf;
                po = PsycoObject_FromCode(co, frame->f_globals, mergepoints, DEFAULT_RECURSION);
                codebuf = psyco_compile_code(po, PsycoObject_Ready(po));
                entry_point = PyTuple_New(2);
                PyTuple_SET_ITEM(entry_point, 0, (PyObject*)codebuf);
                PyTuple_SET_ITEM(entry_point, 1, frame->f_globals);
                PyDict_SetItem(co_to_entry_point, (PyObject*)co, entry_point);
            }
            if (entry_point != Py_None && PyTuple_GET_ITEM(entry_point, 1) == frame->f_globals) {
                return NiCode_Run(PyTuple_GET_ITEM(entry_point, 0), frame);
            }
        }
    }
    return original_eval_frame(frame, throwflag);
}

#if PY3
#error "Not yet"
#else
/* 
   This code is bad and that's OK

   The only purpose of python 2.7 support is to aid transition to Python 3.6+.

   There probably a simple hooking library out there that can do this.
*/
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

INITIALIZATIONFN
int ni_eval_hook_init(void) {
    code_t *limit, *code;
    long page_size;
    CodeBufferObject* codebuf;
    void *eval_frame_code;
    eval_frame_code = (void*)PyEval_EvalFrameEx;
    page_size = sysconf (_SC_PAGESIZE);
    if(memcmp(eval_frame_code, "\x41\x57\x41\x56\x41\x55\x41\x54\x55\x53\x48\x81\xec\xd8\x00\x00\x00", 17) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "ni_eval_hook_init: PyEval_EvalFrameEx finger print does not match - wont patch");
        return -1;
    }
    codebuf = psyco_new_code_buffer(NULL, NULL, &limit);

    code = codebuf->codestart;
    *code++ = 0x41; /* PUSH R15 */
    *code++ = 0x57;
    *code++ = 0x41; /* PUSH R14 */
    *code++ = 0x56; 
    *code++ = 0x41; /* PUSH R13 */
    *code++ = 0x55;
    *code++ = 0x41; /* PUSH R12 */
    *code++ = 0x54;
    *code++ = 0x55; /* PUSH RBP */
    *code++ = 0x53; /* PUSH RBX */
    *code++ = 0x48; /* SUB RSP immed32 */
    *code++ = 0x81;
    *code++ = 0xEC;
    *(int*)code = 0xD8;
    code += 4;
    *code++ = 0x48; /* MOV RAX immed64 */
    *code++ = 0xB8;
    *(unsigned long*)code = (unsigned long)eval_frame_code + 17;
    code += sizeof(unsigned long);
    *code++ = 0xFF; /* JMP RAX */
    *code++ = 0xE0;
    original_eval_frame = (PyEval_EvalFrameExFunction)codebuf->codestart;
    SHRINK_CODE_BUFFER(codebuf, code, "eval_jump");
    /* Write jump into orginal to our function */
    /* make original Python function writable so we can write jump to our code */
    code = (code_t*)eval_frame_code;
    mprotect(code - ((long)code % page_size), page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC );
    *code++ = 0x48; /* MOV RAX immed64 */
    *code++ = 0xB8;
    *(unsigned long*)code = (unsigned long)NiEval_EvalFrame;
    code += sizeof(unsigned long);
    *code++ = 0xFF; /* JMP RAX */
    *code++ = 0xE0;
    return 0;
}
#endif /* !PY3 */
