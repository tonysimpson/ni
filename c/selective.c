#include "selective.h"

DEFINEVAR PyObject* funcs = NULL;
DEFINEVAR int ticks;


DEFINEFN int do_selective(void *v, PyFrameObject *frame, int what, PyObject *arg)
{
  PyObject *code, *name, *g, *tmp;
  int value;
  int err;

  /* Only handle function calls for now */
  if (what == PyTrace_CALL) {
    code = frame->f_code->co_code;
    name = frame->f_code->co_name;
    if (name == NULL)
      return 0;
    g = frame->f_globals;

    /* Get the current tick counter value */
    tmp = PyDict_GetItem(funcs, code);
    if (tmp == NULL) {
      value = 1;
      tmp = PyInt_FromLong(value);
      if (tmp == NULL)
        return -1;
      err = PyDict_SetItem(funcs, code, tmp);
      Py_DECREF(tmp);
      if (err)
        return -1;
    } else {
      value = PyInt_AS_LONG(tmp);
    }
    
    /* Update ticks if the function is not already bound */
    if (value != FUN_BOUND) {
      if (value++ >= ticks) {
        tmp = PyDict_GetItem(g, name);
        if (tmp != NULL && PyFunction_Check(tmp)) {
          /* Rebind function to a proxy */
#if VERBOSE_LEVEL
          PyObject* modulename = PyDict_GetItemString(g, "__name__");
          debug_printf(("psyco: rebinding function %s.%s\n",
                        (modulename && PyString_Check(modulename))
                            ? PyString_AS_STRING(modulename) : "<anonymous>",
                        PyString_Check(name)
                            ? PyString_AS_STRING(name) : "<anonymous>"));
#endif
          value = FUN_BOUND;
          tmp = (PyObject*)psyco_PsycoFunction_New((PyFunctionObject*)tmp, MAX_RECURSION);
          if (tmp == NULL)
            return -1;
          err = PyDict_SetItem(g, name, tmp);
          Py_DECREF(tmp);
          if (err)
            return -1;
        }
      }
      tmp = PyInt_FromLong(value);
      if (tmp == NULL)
        return -1;
      err = PyDict_SetItem(funcs, code, tmp);
      Py_DECREF(tmp);
      if (err)
        return -1;
    }
  }
  return 0;
}


/* FIXME: not working properly yet */
DEFINEFN int do_selective2(void *v, PyFrameObject *frame, int what, PyObject *arg)
{
  PyObject *code, *name, *g, *tmp;
  int value;
  int err;

  if (what == PyTrace_CALL) {
    code = frame->f_code->co_code;
    tmp = PyTuple_GetItem(frame->f_code->co_consts, 
			  PyTuple_Size(frame->f_code->co_consts)-1);
    assert(tmp != NULL);
    if (PsycoFunction_Check(tmp)) {
      return 0;
    } 

    name = frame->f_code->co_name;
    if (name == NULL)
      return 0;
    g = frame->f_globals;

    tmp = PyDict_GetItem(funcs, code);
    if (tmp == NULL) {
      value = 1;
      tmp = PyInt_FromLong(value);
      if (tmp == NULL)
        return -1;
      err = PyDict_SetItem(funcs, code, tmp);
      Py_DECREF(tmp);
      if (err)
        return -1;
    } else {
      value = PyInt_AS_LONG(tmp);
    }

    if (value++ >= ticks) {
      tmp = PyDict_GetItem(g, name);
      if (tmp != NULL && PyFunction_Check(tmp)) {
	PyDict_DelItem(funcs, code);
	psyco_PsycoFunction_New2((PyFunctionObject*) tmp, 
				 MAX_RECURSION);
	return 0;
      }
    }

    tmp = PyInt_FromLong(value);
    if (tmp == NULL)
      return -1;
    err = PyDict_SetItem(funcs, code, tmp);
    Py_DECREF(tmp);
    if (err)
      return -1;
  }
  return 0;
}
