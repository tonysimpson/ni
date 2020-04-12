#include "mergepoints.h"
#include "Python/pycinternal.h"
#include "vcompiler.h"
#include <Python.h>

typedef struct {
  int flags;
  /*
  NULL terminated.
  on allocation we extend this array to count(mergepoints) + 1.
  */
  mergepoint_t mergepoints[1];
} co_report_t;

static void report_destructor(PyObject *capsule) {
  PyMem_Free(PyCapsule_GetPointer(capsule, NULL));
}

static PyObject *_build_merge_points(PyCodeObject *co, int module) {
  const _Py_CODEUNIT *next_instr;
  int opcode; /* Current opcode */
  int oparg;  /* Current opcode argument, if any */
  const _Py_CODEUNIT *first_instr;
  const _Py_CODEUNIT *instr_end;
  int mergepoint_count = 0;
  co_report_t *report;

#define NEXTOPARG()                                                            \
  do {                                                                         \
    _Py_CODEUNIT word = *next_instr;                                           \
    opcode = _Py_OPCODE(word);                                                 \
    oparg = _Py_OPARG(word);                                                   \
    next_instr++;                                                              \
  } while (0)

  if (module) {
    goto not_ok;
  }
  if (co->co_flags & (CO_COROUTINE | CO_GENERATOR)) {
    goto not_ok;
  }

  first_instr = (_Py_CODEUNIT *)PyBytes_AS_STRING(co->co_code);
  next_instr = first_instr;
  instr_end = first_instr + (PyBytes_Size(co->co_code) / sizeof(_Py_CODEUNIT));

  for (; next_instr < instr_end;) {
    NEXTOPARG();

    switch (opcode) {
    case LOAD_FAST:
    case BINARY_ADD:
    case BINARY_SUBTRACT:
    case RETURN_VALUE:
      continue;
    default:
      goto not_ok;
    }
  }
  mergepoint_count = 1;

  report = (co_report_t *)PyMem_Malloc(
      sizeof(co_report_t) + (sizeof(mergepoint_t) * mergepoint_count));
  report->flags = 0;
  report->mergepoints[0].bytecode_position = 0;
  /* XXX tony: make lazy or make a better entry points mechanism */
  report->mergepoints[0].entries.fatlist = PyList_New(0);
  return PyCapsule_New(report, NULL, report_destructor);
not_ok:
  Py_RETURN_NONE;
}

DEFINEFN PyObject *psyco_get_merge_points(PyCodeObject *co, int module) {
  PyObject *capsule_or_none;
  capsule_or_none = PyDict_GetItem(co_to_mp, (PyObject *)co);
  if (capsule_or_none == NULL) {
    capsule_or_none = _build_merge_points(co, module);
    PyDict_SetItem(co_to_mp, (PyObject *)co, capsule_or_none);
  }
  return capsule_or_none;
}

DEFINEFN mergepoint_t *psyco_next_merge_point(PyObject *mergepoints,
                                              int position) {
  mergepoint_t *mp = psyco_first_merge_point(mergepoints);
  while (mp->bytecode_position < position) {
    mp++;
  }
  return mp;
}

DEFINEFN mergepoint_t *psyco_first_merge_point(PyObject *mergepoints) {
  return &(
      ((co_report_t *)PyCapsule_GetPointer(mergepoints, NULL))->mergepoints[0]);
}

DEFINEFN mergepoint_t *psyco_exact_merge_point(PyObject *mergepoints,
                                               int position) {
  mergepoint_t *mp = psyco_next_merge_point(mergepoints, position);
  if (mp->bytecode_position == position) {
    return mp;
  }
  return NULL;
}

DEFINEFN int psyco_mp_flags(PyObject *mergepoints) {
  co_report_t *report = PyCapsule_GetPointer(mergepoints, NULL);
  return report->flags;
}