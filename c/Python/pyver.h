 /***************************************************************/
/***               Detection of Python features                ***/
 /***************************************************************/

#ifndef _PYVER_H
#define _PYVER_H


#include <Python.h>

#define PSYCO_VERSION_HEX          0x010000b1   /* 1.0.0b1 */
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

#define HAVE_struct_dictobject     (NEW_STYLE_TYPES)
#define HAVE_PyEval_EvalCodeEx     (PYTHON_API_VERSION>=1011)
#define HAVE_PyString_FromFormatV  (PYTHON_API_VERSION>=1011)

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

#ifndef PyCode_GetNumFree
# define PyCode_GetNumFree(op)     (PyTuple_GET_SIZE((op)->co_freevars))
#endif


#if !HAVE_PyString_FromFormatV
EXTERNFN PyObject *    /* re-implemented in pycompiler.c */
PyString_FromFormatV(const char *format, va_list vargs);
#endif


#endif /* _PYVER_H */
