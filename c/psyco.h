 /***************************************************************/
/***                 Global Psyco definitions                  ***/
 /***************************************************************/

#ifndef _PSYCO_H
#define _PSYCO_H


#include <Python.h>
#include <structmember.h>   /* for offsetof() */
#include <compile.h>        /* for PyCodeObject */


/*****************************************************************/
 /***   Various customizable parameters (use your compilers'    ***/
  /***   option to override them, e.g. -DXXX=value in gcc)       ***/

 /* set to 0 to disable all debugging checks and output */
#ifndef PSYCO_DEBUG
# define PSYCO_DEBUG   0
#endif


 /* define to 1 for extra assert()'s */
#ifndef ALL_CHECKS
# define ALL_CHECKS    (PSYCO_DEBUG ? 1 : 0)
#endif

 /* level of debugging outputs: 0 = none, 1 = a few, 2 = more,
    3 = detailled, 4 = full execution trace */
#ifndef VERBOSE_LEVEL
# define VERBOSE_LEVEL   (PSYCO_DEBUG ? 0 : 0)
#endif

 /* define for *heavy* memory checking: 0 = off, 1 = reasonably heavy,
                                        2 = unreasonably heavy */
#ifndef HEAVY_MEM_CHECK
# define HEAVY_MEM_CHECK   (PSYCO_DEBUG ? 0 : 0)
#endif
#ifdef MS_WIN32
# undef HEAVY_MEM_CHECK
# define HEAVY_MEM_CHECK   0  /* not supported on Windows */
#endif

 /* define to write produced blocks of code into a file; see 'xam.py'
       0 = off, 1 = only manually (from a debugger or with _psyco.dumpcodebuf()),
       2 = only when returning from Psyco,
       3 = every time a new code block is built */
#ifndef CODE_DUMP
# define CODE_DUMP         (PSYCO_DEBUG ? 1 : 0)
#endif

#if CODE_DUMP && !defined(CODE_DUMP_FILE)
# define CODE_DUMP_FILE    "psyco.dump"
#endif

 /* define to inline the most common functions in the produced code
    (should be enabled unless you want to trade code size for speed) */
#ifndef INLINE_COMMON_FUNCTIONS
# define INLINE_COMMON_FUNCTIONS     1
#endif

#if CODE_DUMP && defined(HAVE_DLFCN_H)
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
   a block of BIG_BUFFER_SIZE+sizeof(CodeBufferObject) bytes.
   Numbers too large might cause serious fragmentation of the heap.
   In debugging mode, we use a small size to stress the buffer-
   continuation coding routines. */
#ifndef BIG_BUFFER_SIZE
# define BIG_BUFFER_SIZE  (PSYCO_DEBUG ? 0x100+BUFFER_MARGIN : 0x3F00)
#endif

/* A safety margin for occasional overflows: we might write a few
   instructions too much before we realize we wrote past 'codelimit'.
   XXX carefully check that it is impossible to overflow by more
   We need more than 128 bytes because of the way conditional jumps
   are emitted; see pycompiler.c.
   XXX actually there are too many places that might emit an
   unbounded size of code. This is a Big Bug. I won't attempt to
   fix it now because it should be done together with Psyco's rewrite
   towards flexible code-emitting back-ends. For now just pretent that
   a quite large value will be OK. */
#ifndef BUFFER_MARGIN
# define BUFFER_MARGIN    (1920 + GUARANTEED_MINIMUM)
#endif

/* When emitting code, all called functions can assume that they
   have at least this amount of room to write their code. If they
   might need more, they have to allocate new buffers and write a
   jump to these from the original code (jumps can be done in less
   than GUARANTEED_MINIMUM bytes). */
#ifndef GUARANTEED_MINIMUM
# define GUARANTEED_MINIMUM    64
#endif


#ifndef ALL_STATIC
# define ALL_STATIC  0   /* make all functions static; set to 1 by hack.c */
#endif

#if ALL_STATIC
# define EXTERNVAR   staticforward
# define EXTERNFN    static
# define DEFINEVAR   statichere
# define DEFINEFN    static
# define INITIALIZATIONFN  inline
#else
# define EXTERNVAR
# define EXTERNFN
# define DEFINEVAR
# define DEFINEFN
# define INITIALIZATIONFN  DEFINEFN
#endif

#if ALL_CHECKS
# define MALLOC_CHECK_    2  /* GCC malloc() checks */
# undef NDEBUG
# include <assert.h>
# define extra_assert(x)  assert(x)
#else
# define extra_assert(x)  (void)0  /* nothing */
#endif

#if VERBOSE_LEVEL
# if defined(stdout)
/* cannot use the version below if stdout is a macro */
#  define debug_printf(args)    (printf args, fflush(stdout))
# else
#  define debug_printf(args)     do {       \
        FILE* __stdout_copy = stdout;       \
        stdout = stderr;                    \
        printf args;                        \
        stdout = __stdout_copy;             \
      } while (0)
# endif
#else
# define debug_printf(args)     do { } while (0) /* nothing */
#endif
#if VERBOSE_LEVEL >= 4
# define TRACE_EXECUTION(msg)   do {                                    \
                       BEGIN_CODE  EMIT_TRACE(msg);  END_CODE } while (0)
EXTERNFN void psyco_trace_execution(char* msg, void* code_position);
#else
# define TRACE_EXECUTION(msg)   do { } while (0) /* nothing */
#endif


#if INLINE_COMMON_FUNCTIONS
# define inline      __inline static
#else
# define inline      static
#endif

#if HEAVY_MEM_CHECK
# include "linuxmemchk.h"
# if HEAVY_MEM_CHECK > 1
#  define PSYCO_NO_LINKED_LISTS
# endif
#endif


#ifndef bool
typedef int bool;
#endif
#ifndef false
# define false   0
#endif
#ifndef true
# define true    1
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

#define PsycoFunction_Check(v)	((v)->ob_type == &PsycoFunction_Type)
EXTERNVAR PyTypeObject PsycoFunction_Type;

EXTERNFN PsycoFunctionObject* psyco_PsycoFunction_New(PyFunctionObject* func,
                                                      int rec);
EXTERNFN PsycoFunctionObject* psyco_PsycoFunction_NewEx(PyCodeObject* code,
                                                PyObject* globals,
                                                PyObject* defaults, /* or NULL */
                                                int rec);
EXTERNFN PyObject* psyco_PsycoFunction_New2(PyFunctionObject* func, 
					    int rec);


#if CODE_DUMP
EXTERNFN void psyco_dump_code_buffers(void);
#endif
#if CODE_DUMP >= 3
# define dump_code_buffers()    psyco_dump_code_buffers()
#else
# define dump_code_buffers()    do { } while (0) /* nothing */
#endif

/* to display code object names */
#define PyCodeObject_NAME(co)   (co->co_name ? PyString_AS_STRING(co->co_name)  \
                                 : "<anonymous code object>")


/* defined in pycompiler.c */
#define GLOBAL_ENTRY_POINT	psyco_pycompiler_mainloop
EXTERNFN code_t* psyco_pycompiler_mainloop(PsycoObject* po);


/* XXX no handling of out-of-memory conditions. We have to define precisely
   what should occur in various cases, like when we run out of memory in the
   middle of writing code, when the beginning is already executing. When
   should we report the exception? */
#define OUT_OF_MEMORY()      Py_FatalError("psyco: out of memory")


#endif /* _PSYCO_H */
