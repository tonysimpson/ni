 /***************************************************************/
/***            Python frames and virtual frames               ***/
 /***************************************************************/

#ifndef _FRAMES_H
#define _FRAMES_H


#include "../psyco.h"
#include "../cstruct.h"
#include "pyver.h"
#include <compile.h>
#include <frameobject.h>


/*** the PsycoCode_Xxx interface requires Python >= 2.2.2 ***/
#define HAVE_DYN_COMPILE     HAVE_PYTHON_SUPPORT

#if HAVE_DYN_COMPILE


/* Basic interface to compile the given code object.  Return the
   code buffer or Py_None if this code cannot be compiled.
   Never sets an exception. */
EXTERNFN PyObject* PsycoCode_CompileCode(PyCodeObject* co,
                                         PyObject* globals,
                                         int recursion);

/* Same as PsycoCode_CompileCode, but starts compiling from the middle
   of a frame */
EXTERNFN PyObject* PsycoCode_CompileFrame(PyFrameObject* f, int recursion);

/* Run a compiled buffer returned by PsycoCode_CompileXxx().
   The submitted frame object will be modified to reflect any progress done
   in running this frame by Psyco.  Return 'false' if an exception is set.
   For code compiled by PsycoCode_CompileCode, the frame must just have been
   initialized by ceval.c but not yet run.  For code compiled by
   PsycoCode_CompileFrame, the frame must be in the same state as when it
   was compiled. */
EXTERNFN bool PsycoCode_Run(PyObject* codebuf, PyFrameObject* f);

#endif /* HAVE_DYN_COMPILE */


/* Return the nth frame (0=top). If it is a Python frame, psy_frame is
   unmodified. If it is a Psyco frame, *psy_frame is filled and the
   return value is only the next Python frame of the stack. */
EXTERNFN PyFrameObject* psyco_get_frame(int depth,
                                        struct stack_frame_info_s* psy_frame);
EXTERNFN PyObject* psyco_get_globals(void);
EXTERNFN PyObject* psyco_get_locals(void);


/* to keep a trace of the Psyco stack frames */
struct stack_frame_info_s {
	int stack_depth;
	PyCodeObject* co;
	PyObject* globals;  /* NULL if not compile-time */
};

EXTERNFN stack_frame_info_t* psyco_finfo(PsycoObject* callee);

EXTERNFN PyFrameObject* psyco_emulate_frame(stack_frame_info_t* finfo,
					    PyObject* default_globals);

/* extra run-time data attached to the Python frame objects which are
   used as starting point for Psyco frames */
typedef struct {
  PyCStruct_HEAD             /* cs_key is the frame object */
  stack_frame_info_t*** psy_frames_start;
  PyCodeObject* psy_code;
  PyObject* psy_globals;
} PyFrameRuntime;

EXTERNFN void PyFrameRuntime_dealloc(PyFrameRuntime* self);


#endif /* _FRAMES_H */
