#include "selective.h"

DEFINEVAR PyObject* funcs = NULL;
DEFINEVAR int ticks;


DEFINEFN int do_selective(void *v, PyFrameObject *frame, int what, PyObject *arg)
{
  PyObject *code, *name, *g, *tmp;
  int value;

  /* Only handle function calls for now */
  if (what == PyTrace_CALL) {
    code = frame->f_code->co_code;
    name = frame->f_code->co_name;
    g = frame->f_globals;

    /* Get the current tick counter value */
    tmp = PyDict_GetItem(funcs, code);
    if (tmp == NULL) {
      PyDict_SetItem(funcs, code, Py_BuildValue("i", 1));
      value = 1;
    } else {
      value = PyInt_AS_LONG(tmp);
    }
    
    /* Update ticks if the function is not already bound */
    if (value != FUN_BOUND) {
      if (value++ >= ticks) {
        tmp = PyDict_GetItem(g, name);
        if (tmp != NULL) {
          /* Rebind function to a proxy */
          printf("psyco: compiling function %s\n", PyString_AS_STRING(name));
          value = FUN_BOUND;
          PyDict_SetItem(g, name, (PyObject*)psyco_PsycoFunction_New((PyFunctionObject*)tmp, MAX_RECURSION));
        }
      }
      PyDict_SetItem(funcs, code, Py_BuildValue("i", value));
    }
  }
  return 0;
}
