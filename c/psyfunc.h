 /***************************************************************/
/***          Psyco Function objects (a.k.a. proxies)          ***/
 /***************************************************************/

#ifndef _PSYFUNC_H
#define _PSYFUNC_H


#include "psyco.h"
#include <compile.h>        /* for PyCodeObject */


/* Encode a call to the given Python function, compiling it as needed. */
EXTERNFN vinfo_t* psyco_call_pyfunc(PsycoObject* po, PyCodeObject* co,
                                    vinfo_t* vglobals, vinfo_t* vdefaults,
                                    vinfo_t* arg_tuple, int recursion);


/* Psyco proxies for Python functions. Calling a proxy has the same effect
   as calling the function it has been built from, except that the function
   is compiled first. As proxies are real Python objects, calling them is
   the only way to go from Python's base level to Psyco's meta-level.
   Note that (unlike in previous versions of Psyco) proxies should not be
   seen by user Python code. Use _psyco.proxycode() to build a proxy and
   emcompass it in a code object. */
typedef struct {
  PyObject_HEAD
  PyCodeObject* psy_code;  /*                                     */
  PyObject* psy_globals;   /*  same as in Python function object  */
  PyObject* psy_defaults;  /*                                     */
  int psy_recursion;    /* # levels to automatically compile called functions */
  PyObject* psy_fastcall;       /* cache mapping arg count to code bufs */
} PsycoFunctionObject;

EXTERNVAR PyTypeObject PsycoFunction_Type;

#define PsycoFunction_Check(op)	PyObject_TypeCheck(op, &PsycoFunction_Type)


EXTERNFN PyObject* psyco_PsycoFunction_New(PyFunctionObject* func, int rec);
EXTERNFN PsycoFunctionObject* psyco_PsycoFunction_NewEx(PyCodeObject* code,
                                                PyObject* globals,
                                                PyObject* defaults, /* or NULL */
                                                int rec);
EXTERNFN PyObject* psyco_proxycode(PyFunctionObject* func, int rec);

inline bool is_proxycode(PyCodeObject* code) {
  return PyTuple_Size(code->co_consts) > 1 &&
    PsycoFunction_Check(PyTuple_GET_ITEM(code->co_consts, 1));
}


/* Return the nth frame (0=top). If it is a Python frame, psy_frame is
   unmodified. If it is a Psyco frame, *psy_frame is filled and the
   return value is only the next Python frame of the stack. */
EXTERNFN struct _frame* psyco_get_frame(int depth,
                                        struct stack_frame_info_s* psy_frame);
EXTERNFN PyObject* psyco_get_globals(void);
EXTERNFN PyObject* psyco_get_locals(void);


#endif /* _PSYFUNC_H */
