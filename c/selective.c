#include "selective.h"
#include <frameobject.h>

/*#define FUN_BOUND -1*/
/*#define MAX_RECURSION 99*/


static PyObject* funcs = NULL;
DEFINEVAR int ticks;


#ifdef PyTrace_CALL
# define HAVE_PyEval_SetProfile    1
#else
# define HAVE_PyEval_SetProfile    0
# define PyTrace_CALL 0
# define PyTrace_EXCEPTION 1
# define PyTrace_LINE 2
# define PyTrace_RETURN 3
#endif


#if 0
       "Old version, disabled --- will be completely removed soon."
static int do_selective(void *v, PyFrameObject *frame, int what, PyObject *arg)
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
#endif


static int do_selective(PyObject *v, PyFrameObject *frame,
                        int what, PyObject *arg)
{
  PyObject *name, *g, *tmp;
  PyCodeObject *co;
  int value;
  int err;

  /* Only handle function calls for now */
  if (what == PyTrace_CALL) {
    co = frame->f_code;
    if (is_proxycode(co))
      return 0;  /* code already rebound */

    /* Get the current tick counter value */
    tmp = PyDict_GetItem(funcs, (PyObject*) co);
    if (tmp == Py_None)
      return 0;  /* function already rebound, although the old version is
                    still called -- this occurs when we could not find the
                    function object in the globals */
    value = tmp ? PyInt_AS_LONG(tmp) : 0;

    if (++value >= ticks) {
      name = co->co_name;
      g = frame->f_globals;
      tmp = PyDict_GetItem(g, name);
      if (tmp != NULL && PyFunction_Check(tmp) &&
          PyFunction_GET_CODE(tmp) == (PyObject*) co)
        {
          /* don't rebind if we "found" some other unrelated function */
          PyFunctionObject* func = (PyFunctionObject*) tmp;
          tmp = psyco_proxycode(func, DEFAULT_RECURSION);
          if (tmp == NULL)
            PyErr_Clear();  /* ignore errors like 'unsupported code' */
          else
            {
              /* Rebind function to the proxy */
              PyObject* old_code = func->func_code;
              func->func_code = tmp;
              Py_DECREF(old_code);  /* there is a ref left in the proxy */
#if VERBOSE_LEVEL
              {
                PyObject* modulename = PyDict_GetItemString(g, "__name__");
                debug_printf(("psyco: rebinding function %s.%s\n",
                              (modulename && PyString_Check(modulename))
                                ? PyString_AS_STRING(modulename) : "<anonymous>",
                              PyString_Check(name)
                                ? PyString_AS_STRING(name) : "<anonymous>"));
              }
#endif
            }
        }
      Py_INCREF(Py_None);
      tmp = Py_None;
    }
    else {
      tmp = PyInt_FromLong(value);
      if (tmp == NULL)
        return -1;
    }
    err = PyDict_SetItem(funcs, (PyObject*) co, tmp);
    Py_DECREF(tmp);
    if (err)
      return -1;
  }
  return 0;
}


#if !HAVE_PyEval_SetProfile
static PyObject* profile_wrapper(PyObject* self, PyObject* args)
{
  char* s;
  int what;
  extra_assert(PyTuple_Check(args) && PyTuple_Size(args) == 3);
  extra_assert(PyString_Check(PyTuple_GET_ITEM(args, 1)));
  s = PyString_AS_STRING(PyTuple_GET_ITEM(args, 1));

  what = -1;
  switch (s[0]) {
    
  case 'c':
    if (strcmp(s, "call") == 0)
      what = PyTrace_CALL;
    break;

  case 'e':
    if (strcmp(s, "exception") == 0)
      what = PyTrace_EXCEPTION;
    break;

  case 'r':
    if (strcmp(s, "return") == 0)
      what = PyTrace_RETURN;
    break;
  }
  if (what != -1 &&
      do_selective(NULL, (PyFrameObject*) PyTuple_GET_ITEM(args, 0),
                   what, PyTuple_GET_ITEM(args, 2)))
    return NULL;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef def_profile_wrapper = { "wrapper", &profile_wrapper,
                                           METH_VARARGS };
#endif /* !HAVE_PyEval_SetProfile */


DEFINEFN int psyco_start_selective()
{
  /* Allocate a dict to hold counters and statistics in */
  if (funcs == NULL) {
    funcs = PyDict_New();
    if (funcs == NULL)
      return -1;
  }

#if HAVE_PyEval_SetProfile
  /* Set Python profile function to our selective compilation function */
  PyEval_SetProfile(&do_selective, NULL);
#else
  /* Use work-around */
  {
    PyThreadState* tstate = PyThreadState_Get();
    PyObject* fn = PyCFunction_New(&def_profile_wrapper, NULL);
    if (fn == NULL)
      return -1;
    Py_XDECREF(tstate->sys_profilefunc);
    tstate->sys_profilefunc = fn;
  }
#endif
  return 0;
}
