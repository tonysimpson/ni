#include "mergepoints.h"
#include "Python/pycinternal.h"
#include "vcompiler.h"
#include <Python.h>

typedef struct {
  int flags;
  Py_ssize_t mergepoint_count;
  /*
  NULL terminated.
  on allocation we extend this array to count(mergepoints) + 1.
  */
  mergepoint_t mergepoints[1];
} co_report_t;

static void report_destructor(PyObject *capsule) {
  PyMem_Free(PyCapsule_GetPointer(capsule, NULL));
}

#define NEXTOPARG()                                                            \
  do {                                                                         \
    _Py_CODEUNIT word = *next_instr;                                           \
    opcode = _Py_OPCODE(word);                                                 \
    oparg = _Py_OPARG(word);                                                   \
    next_instr++;                                                              \
  } while (0)

#define _ENTER_INC(loc)                                                        \
  do {                                                                         \
    if (entry_count[loc] < 8) {                                                \
      entry_count[loc]++;                                                      \
    }                                                                          \
  } while (0)

#define ENTER_NEXT()                                                           \
  do {                                                                         \
    _ENTER_INC(entry_count_index + 1);                                         \
  } while (0)

#define ENTER_JUMP_TO(target)                                                  \
  do {                                                                         \
    _ENTER_INC(target / 2);                                                    \
  } while (0)

#define MP_HERE()                                                              \
  do {                                                                         \
    entry_count[entry_count_index] = 8;                                        \
  } while (0)

static co_report_t *_find_merge_points(PyCodeObject *co) {
  const _Py_CODEUNIT *next_instr;
  int opcode; /* Current opcode */
  int oparg;  /* Current opcode argument, if any */
  const _Py_CODEUNIT *first_instr;
  const _Py_CODEUNIT *instr_end;
  Py_ssize_t mergepoint_count = 0;
  Py_ssize_t mergepoint_index = 0;
  co_report_t *report;
  Py_ssize_t entry_count_index = -1;
  Py_ssize_t entry_count_len =
      (PyBytes_Size(co->co_code) / sizeof(_Py_CODEUNIT)) + 1;
  unsigned char entry_count[entry_count_len];
  memset(entry_count, 0, entry_count_len);

  first_instr = (_Py_CODEUNIT *)PyBytes_AS_STRING(co->co_code);
  next_instr = first_instr;
  instr_end = first_instr + (PyBytes_Size(co->co_code) / sizeof(_Py_CODEUNIT));

  entry_count[0] = 2; /* start of code is always a merge point */
  for (; next_instr < instr_end;) {
    NEXTOPARG();
  dispatch_opcode:
    entry_count_index++;
    switch (opcode) {
    case EXTENDED_ARG: {
      int oldoparg = oparg;
      NEXTOPARG();
      oparg |= oldoparg << 8;
      ENTER_NEXT();
      goto dispatch_opcode;
    }
    case RETURN_VALUE:
      break;
    case JUMP_ABSOLUTE:
      ENTER_JUMP_TO(oparg);
      break;
    case POP_JUMP_IF_FALSE:
      ENTER_JUMP_TO(oparg);
      ENTER_NEXT();
      break;
    default:
      MP_HERE(); /* XXX tony: we only need MPs where we might respawn/reenter
                    need to tie this with pycompiler and make effecient.
                    Just making everything a mergepoints works but I think it
                    causes lots of unnecessary reentry points to be written.
                     */
      ENTER_NEXT();
    }
  }

  for (entry_count_index = 0; entry_count_index < entry_count_len;
       entry_count_index++) {
    if (entry_count[entry_count_index] > 1) {
      mergepoint_count++;
    }
  }

  report = (co_report_t *)PyMem_Malloc(
      sizeof(co_report_t) + (sizeof(mergepoint_t) * mergepoint_count));
  report->flags = 0;
  report->mergepoint_count = mergepoint_count;
  if (mergepoint_count == 1) {
    report->flags = MP_FLAGS_INLINABLE;
  }
  for (entry_count_index = 0; entry_count_index < entry_count_len;
       entry_count_index++) {
    if (entry_count[entry_count_index] > 1) {
      report->mergepoints[mergepoint_index].bytecode_position =
          entry_count_index * 2;
      report->mergepoints[mergepoint_index].entries.fatlist = PyList_New(0);
      mergepoint_index++;
    }
  }
  report->mergepoints[mergepoint_count].bytecode_position = -1;
  report->mergepoints[mergepoint_count].entries.fatlist = NULL;
  return report;
}

static PyObject *_build_merge_points(PyCodeObject *co, int module) {
  const _Py_CODEUNIT *next_instr;
  int opcode; /* Current opcode */
  int oparg;  /* Current opcode argument, if any */
  const _Py_CODEUNIT *first_instr;
  const _Py_CODEUNIT *instr_end;
  co_report_t *report;

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
    case STORE_FAST:
    case LOAD_FAST:
    case LOAD_GLOBAL:
    case LOAD_CONST:
    case BINARY_ADD:
    case BINARY_SUBTRACT:
    case RETURN_VALUE:
    case CALL_FUNCTION:
    case POP_TOP:
    case POP_BLOCK:
    case COMPARE_OP:
    case JUMP_ABSOLUTE:
    case POP_JUMP_IF_FALSE:
    case SETUP_LOOP:
      continue;
    default:
      goto not_ok;
    }
  }

  report = _find_merge_points(co);

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