 /***************************************************************/
/***                 Global Psyco definitions                  ***/
 /***************************************************************/

#ifndef _PSYCO_H
#define _PSYCO_H


#include <Python.h>
#include <structmember.h>   /* for offsetof() */
#include <compile.h>        /* for PyCodeObject */

#ifndef DISABLE_DEBUG
 /* define for extra assert()'s */
# define ALL_CHECKS

 /* define for a few debugging outputs */
# define VERBOSE_LEVEL   2   /* 0, 1 or 2 */

 /* define for *heavy* memory checking */
# define HEAVY_MEM_CHECK
 /* define for **really** **heavy** memory checking */
/*#undef HEAVY_HEAVY_MEM_CHECK*/

 /* define to write produced blocks of code into a file
    See 'xam.py' */
# define CODE_DUMP_FILE    "psyco.dump"
//# define CODE_DUMP_AT_END_ONLY
# define SPEC_DICT_SIGNATURE   0x98247b9d   /* arbitrary */

#endif  /* !DISABLE_DEBUG */

 /* define to inline the most common functions in the produced code
    (should be enabled unless you want to trade code size for speed) */
#define INLINE_COMMON_FUNCTIONS

#if defined(CODE_DUMP_FILE) && defined(HAVE_DLFCN_H)
 /* define to locate shared symbols and write them in CODE_DUMP_FILE
    requires the GNU extension dladdr() in <dlfcn.h>
    Not really useful, only finds non-static symbols. */
/*# include <dlfcn.h>
  # define CODE_DUMP_SYMBOLS*/
#endif


/*****************************************************************/

/* Size of buffer to allocate when emitting code.
   Can be relatively large, but not so large that special allocation
   routines (like mmap) are invoked. We rely on the fact that
   PyObject_REALLOC will not move the memory around when shrinking
   a block of BIG_BUFFER_SIZE+sizeof(CodeBufferObject) bytes. */
#ifdef DISABLE_DEBUG
# define BIG_BUFFER_SIZE  0x7F00
#else
# define BIG_BUFFER_SIZE  0x7F0
#endif

/* A safety margin for occasional overflows: we might write a few
   instructions too much before we realize we wrote past 'codelimit'.
   XXX carefully check that it is impossible to overflow by more
   We need more than 128 bytes because of the way conditional jumps
   are emitted; see pycompiler.c */
#define BUFFER_MARGIN    (192 + GUARANTEED_MINIMUM)

/* When emitting code, all called functions can assume that they
   have at least this amount of room to write their code. If they
   might need more, they have to allocate new buffers and write a
   jump to these from the original code (jumps can be done in less
   than GUARANTEED_MINIMUM bytes). */
#define GUARANTEED_MINIMUM    32


#ifdef ALL_CHECKS
# define MALLOC_CHECK_    2  /* GCC malloc() checks */
# undef NDEBUG
# include <assert.h>
# define extra_assert(x)  assert(x)
#else
# define extra_assert(x)  (void)0  /* nothing */
#endif

#ifndef VERBOSE_LEVEL
# define VERBOSE_LEVEL   0
#else
# define VERBOSE
#endif

#ifdef VERBOSE
# define debug_printf(args)   (printf args, fflush(stdout))
#else
# define debug_printf(args)   (void)0  /* nothing */
#endif

#ifdef ALL_STATIC
# define EXTERNVAR   staticforward
# define EXTERNFN    static
# define DEFINEVAR   statichere
# define DEFINEFN    static
#else
# define EXTERNVAR   extern
# define EXTERNFN
# define DEFINEVAR
# define DEFINEFN
#endif

#ifdef INLINE_COMMON_FUNCTIONS
# define inline      __inline__ static   /* is this GCC-specific? */
#else
# define inline      static
#endif

#ifdef HEAVY_MEM_CHECK
# include "linuxmemchk.h"
# ifdef HEAVY_HEAVY_MEM_CHECK
#  define PSYCO_NO_LINKED_LISTS
# endif
#endif


#ifndef bool
typedef int bool;
#endif
#ifndef false
static const bool false = 0;
#endif
#ifndef true
static const bool true = 1;
#endif


typedef char code_t;

typedef struct vinfo_s vinfo_t;             /* defined in compiler.h */
typedef struct vinfo_array_s vinfo_array_t; /* defined in compiler.h */
typedef struct PsycoObject_s PsycoObject;   /* defined in compiler.h */
typedef struct FrozenPsycoObject_s FrozenPsycoObject; /* def in dispatcher.h */
typedef struct CodeBufferObject_s CodeBufferObject;  /* def in codemanager.h */
typedef struct global_entries_s global_entries_t;  /* def in dispatcher.h */
typedef struct mergepoint_s mergepoint_t;   /* defined in mergepoint.h */

EXTERNVAR PyObject* PyExc_PsycoError;


/*#undef PY_PSYCO_MODULE*/   /* nothing useful in psyco.py right now */
#ifdef PY_PSYCO_MODULE
EXTERNVAR PyObject* PyPsycoModule;
#endif


/* moved here from vcompiler.h because needed by numerous header files */
typedef bool (*compute_fn_t)(PsycoObject* po, vinfo_t* vi);
typedef struct {
  compute_fn_t compute_fn;
} source_virtual_t;


/* Build a PsycoObject "frame" corresponding to the call of a Python
   function. Raise a Python exception and return NULL in case of failure.
   Return BF_UNSUPPORTED if the bytecode contains unsupported instructions.
   The 'arginfo' array gives the number of arguments as well as
   additional information about them. It will be expanded with the
   default values of missing arguments, if any, and finally released.
   If 'sources!=NULL', it is set to an array of the sources of the values
   that must be pushed to make the call. */
EXTERNFN PsycoObject* psyco_build_frame(PyFunctionObject* function,
                                        vinfo_array_t* arginfo, int recursion,
                                        long** sources);
/* 'sources' is actually of type 'RunTimeSource**' */
#define BF_UNSUPPORTED  ((PsycoObject*) -1)

/* Encode a call to the given Python function, compiling it as needed. */
EXTERNFN vinfo_t* psyco_call_pyfunc(PsycoObject* po, PyFunctionObject* function,
                                    vinfo_t* arg_tuple, int recursion);


/* Psyco proxies for Python functions */
typedef struct {
  PyObject_HEAD
  PyFunctionObject* psy_func;   /* Python function object */
  int psy_recursion;    /* # levels to automatically compile called functions */
} PsycoFunctionObject;

#define PsycoFunction_Check(v)	((v)->ob_type == &PsycoFunction_Type)
EXTERNVAR PyTypeObject PsycoFunction_Type;

EXTERNFN PsycoFunctionObject* psyco_PsycoFunction_New(PyFunctionObject* func,
                                                      int rec);


#if defined(CODE_DUMP_FILE) && !defined(CODE_DUMP_AT_END_ONLY)
EXTERNFN void psyco_dump_code_buffers(void);
#else
# define psyco_dump_code_buffers()    do { } while (0) /* nothing */
#endif


/* defined in pycompiler.c */
#define GLOBAL_ENTRY_POINT	psyco_pycompiler_mainloop
EXTERNFN code_t* psyco_pycompiler_mainloop(PsycoObject* po);


/* XXX no handling of out-of-memory conditions. We have to define precisely
   what should occur in various cases, like when we run out of memory in the
   middle of writing code, when the beginning is already executing. When
   should we report the exception? */
#define OUT_OF_MEMORY()      Py_FatalError("psyco: out of memory")


#endif /* _PSYCO_H */
