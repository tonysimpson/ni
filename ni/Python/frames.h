 /***************************************************************/
/***            Python frames and virtual frames               ***/
 /***************************************************************/

#ifndef _FRAMES_H
#define _FRAMES_H


#include "../psyco.h"
#include "../cstruct.h"
#include <idispatcher.h>
#include "pyver.h"
#include <compile.h>
#include <frameobject.h>

/* Find a frame. If 'o' is an integer, it is the 'o'th frame (0=top).
   If 'o' was returned by a previous call to psyco_get_frame(), find
   the previous frame (as reading f_back does).
   The return value is either a normal Python frame object, or
   a tuple (code, globals, tag). tag is only used for the next call
   to psyco_get_frame(). */
EXTERNFN PyObject* psyco_find_frame(PyObject* o);
EXTERNFN PyFrameObject* psyco_emulate_frame(PyObject* o);

EXTERNFN PyObject* psyco_get_globals(void);
/* PyObject* psyco_get_locals(void); this one implemented in psyco.c */


/* to keep a trace of the Psyco stack frames */
struct stack_frame_info_s {
#if NEED_STACK_FRAME_HACK
	int link_stack_depth;  /* -1 if there is an inline frame following */
#endif
	PyCodeObject* co;
	PyObject* globals;  /* NULL if not compile-time */
};
PSY_INLINE stack_frame_info_t* finfo_last(stack_frame_info_t* finfo) {
#if NEED_STACK_FRAME_HACK
	if (finfo->link_stack_depth < 0) finfo -= finfo->link_stack_depth;
#endif
	return finfo;
}

EXTERNFN stack_frame_info_t* psyco_finfo(PsycoObject* caller,
					 PsycoObject* callee);

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
