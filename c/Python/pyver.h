 /***************************************************************/
/***               Detection of Python features                ***/
 /***************************************************************/

#ifndef _PYVER_H
#define _PYVER_H


#include <Python.h>

#define PSYCO_VERSION_HEX          0x010500a0   /* 1.5a */
#define HAVE_PYTHON_SUPPORT        (PY_VERSION_HEX>=0x02020200)   /* 2.2.2 */


/*****************************************************************/
 /***   Detects differences between Python versions             ***/

/* Note: not all features can be automatically detected; in some cases
   we just assume that the feature is present or not based on some
   other feature that has been introduced roughly at the same time.
   This may need fixes to compile with some intermediary Python
   versions. */

#ifdef PyString_CheckExact
# define NEW_STYLE_TYPES           1    /* Python >=2.2b1 */
#else
# define NEW_STYLE_TYPES           0
# define PyString_CheckExact       PyString_Check
#endif

#ifndef Py_USING_UNICODE
# define Py_USING_UNICODE          1   /* always true in Python 2.1 */
#endif
#define PSYCO_CAN_CALL_UNICODE     0   /* prevent references to PyUnicode_Xxx
                                          functions causing potential linker
                                          errors because of UCS2/UCS4 name
                                          mangling */

#define HAVE_struct_dictobject     (NEW_STYLE_TYPES)
#define HAVE_PyEval_EvalCodeEx     (PYTHON_API_VERSION>=1011)
#define HAVE_PyString_FromFormatV  (PYTHON_API_VERSION>=1011)
#define HAVE_arrayobject_allocated (PY_VERSION_HEX>=0x02040000)   /* 2.4 */

#ifndef Py_TPFLAGS_HAVE_GC
# define PyObject_GC_New(t,tp)     PyObject_New(t,tp)
# define PyObject_GC_Track(o)      do { } while (0)  /* nothing */
# define PyObject_GC_UnTrack(o)    do { } while (0)  /* nothing */
# define PyObject_GC_Del(o)        PyObject_Del(o)
#endif

#ifdef METH_O
# define HAVE_METH_O               1
#else
# define HAVE_METH_O               0
# define METH_O                    0x0008
#endif

#ifdef METH_NOARGS
# define HAVE_METH_NOARGS          1
#else
# define HAVE_METH_NOARGS          0
# define METH_NOARGS               0x0004
#endif

#ifndef PyCode_GetNumFree
# define PyCode_GetNumFree(op)     (PyTuple_GET_SIZE((op)->co_freevars))
#endif


#if !HAVE_PyString_FromFormatV
EXTERNFN PyObject *    /* re-implemented in pycompiler.c */
PyString_FromFormatV(const char *format, va_list vargs);
#endif


#if HAVE_LONG_LONG && !defined(PY_LONG_LONG)
# define PY_LONG_LONG   LONG_LONG   /* Python < 2.3 */
#endif

#ifdef PyBool_Check
# define BOOLEAN_TYPE              1    /* Python >=2.3 */
#else
# define BOOLEAN_TYPE              0
#endif

#ifndef PyString_CHECK_INTERNED
# define PyString_CHECK_INTERNED(op) (((PyStringObject*)(op))->ob_sinterned)
#endif

#ifndef PyMODINIT_FUNC
# define PyMODINIT_FUNC void
#endif


#endif /* _PYVER_H */
