#include "frames.h"
#include "pyver.h"
#include "../codemanager.h"
#include "../vcompiler.h"
#include "../Objects/pobject.h"

/***************************************************************/
#define FRAME_STACK_ALLOC_BY	83   /* about 1KB */

DEFINEFN
stack_frame_info_t* psyco_finfo(PsycoObject* caller, PsycoObject* callee)
{
	static stack_frame_info_t* current = NULL;
	static stack_frame_info_t* end = NULL;
	
	Source sglobals;
	stack_frame_info_t* p;
	int inlining = caller != NULL && caller->pr.is_inlining;
	
	if (end - current <= inlining) {
		current = PyMem_NEW(stack_frame_info_t, FRAME_STACK_ALLOC_BY);
		if (current == NULL)
			OUT_OF_MEMORY();
		end = current + FRAME_STACK_ALLOC_BY;
	}
	p = current;
	current += inlining + 1;
#if NEED_STACK_FRAME_HACK
	p->link_stack_depth = -inlining;
#endif
	p->co = callee->pr.co;
	sglobals = callee->vlocals.items[INDEX_LOC_GLOBALS]->source;
	if (is_compiletime(sglobals))
		p->globals = (PyObject*) CompileTime_Get(sglobals)->value;
	else
		p->globals = NULL;  /* uncommon */
	if (inlining) {
		(p+1)->co = caller->pr.co;
		sglobals = caller->vlocals.items[INDEX_LOC_GLOBALS]->source;
		if (is_compiletime(sglobals))
			(p+1)->globals = (PyObject*)
				CompileTime_Get(sglobals)->value;
		else
			(p+1)->globals = NULL;  /* uncommon */
	}
	return p;
}

DEFINEFN
void PyFrameRuntime_dealloc(PyFrameRuntime* self)
{
	/* nothing */
}

PSY_INLINE PyFrameObject* psyco_build_pyframe(PyObject* co, PyObject* globals,
					      PyThreadState* tstate)
{
	PyFrameObject* back;
	PyFrameObject* result;
	
	/* frame objects are not created in stack order
	   with Psyco, so it's probably better not to
	   create plain wrong chained lists */
	back = tstate->frame;
	tstate->frame = NULL;
	result = PyFrame_New(tstate, (PyCodeObject*) co, globals, NULL);
	if (result == NULL)
		OUT_OF_MEMORY();
        result->f_lasti = -1;  /* can be used to identify emulated frames */
	tstate->frame = back;
	return result;
}

DEFINEFN
PyFrameObject* psyco_emulate_frame(PyObject* o)
{
	if (PyFrame_Check(o)) {
		/* a real Python frame */
		Py_INCREF(o);
		return (PyFrameObject*) o;
	}
	else {
		/* a Psyco frame: emulate it */
		PyObject* co = PyTuple_GetItem(o, 0);
		PyObject* globals = PyTuple_GetItem(o, 1);
		extra_assert(co != NULL);
		extra_assert(globals != NULL);
		return psyco_build_pyframe(co, globals, PyThreadState_GET());
	}
}

struct sfitmp_s {
	stack_frame_info_t** fi;
	struct sfitmp_s* next;
};

static PyObject* pvisitframes(PyObject*(*callback)(PyObject*,void*),
			      void* data)
{
        /* Whenever we run Psyco-produced machine code, we mark the current
           Python frame as the starting point of a chained list of Psyco
           frames. The machine code will update this chained list so that
           psyco_next_stack_frame() can be used to visit the list from
           the outermost to the innermost frames. Note that the list does
           not contain the first Psyco frame, the one directly run by a
           call to psyco_processor_run(). This still gives the expected
           result, because PsycoFunctionObjects are only supposed to be
           called by proxy codes (see psyco_proxycode()). This proxy
           code itself has a frame. It replaces the missing Psyco frame.
           XXX this would no longer work if we filled the emulated frames
               with more information, like local variables */

	PyObject* result = NULL;
	PyFrameRuntime* fstart;
	PyObject* tdict = psyco_thread_dict();
	PyFrameObject* f = PyThreadState_Get()->frame;

	RECLIMIT_SAFE_ENTER();
	while (f != NULL) {
		/* is this Python frame the starting point of a chained
		   list of Psyco frames ? */
		fstart = (PyFrameRuntime*) PyDict_GetItem(tdict, (PyObject*) f);
		if (fstart != NULL) {
			/* Yes. Get the list start. */
			struct sfitmp_s* revlist;
			struct sfitmp_s* p;
			PyObject* o;
			PyObject* g;
			long tag;
			stack_frame_info_t** f1;
			stack_frame_info_t** finfo;
			stack_frame_info_t* fdata;
			stack_frame_info_t* flimit;
			finfo = *(fstart->psy_frames_start);

			/* Enumerate the frames and store them in a
			   last-in first-out linked list. The end is marked by
			   a pointer with an odd integer value (actually with
                           i386 the least significant byte of the integer value
                           is -1, and with ivm the end pointer's value is
                           exactly 1; but real pointers cannot be odd at all
                           because they are aligned anyway). */
			revlist = NULL;
			for (f1 = finfo; (((long)(*f1)) & 1) == 0;
			     f1 = psyco_next_stack_frame(f1)) {
				p = (struct sfitmp_s*)
					PyMem_MALLOC(sizeof(struct sfitmp_s));
				if (p == NULL)
					OUT_OF_MEMORY();
				p->fi = f1;
				p->next = revlist;
				revlist = p;
#if NEED_STACK_FRAME_HACK
				if ((*f1)->link_stack_depth == 0)
					break; /* stack top is an inline frame */
#endif
			}

			/* now actually visit them in the correct order */
			while (revlist) {
				p = revlist;
				/* a Psyco frame is represented as
				   (co, globals, address_of(*fi)) */
				if (result == NULL) {
					tag = (long)(p->fi);
					fdata = *p->fi;
					flimit = finfo_last(fdata);
					while (1) {
						g = fdata->globals;
						if (g == NULL)
							g = f->f_globals;
						o = Py_BuildValue("OOl",
								  fdata->co, g,
								  tag);
						if (o == NULL)
							OUT_OF_MEMORY();
						result = callback(o, data);
						Py_DECREF(o);
						if (result != NULL)
							break;
						if (fdata == flimit)
							break;
						fdata++, tag--;
					}
				}
				revlist = p->next;
				PyMem_FREE(p);
			}
			if (result != NULL)
				break;

			/* there is still the real Python frame
			   which is shadowed by a Psyco frame, i.e. a
			   proxy function. Represented as
			   (co, globals, f) */
			o = Py_BuildValue("OOO",
					  fstart->psy_code,
					  fstart->psy_globals,
					  f);
			if (o == NULL)
				OUT_OF_MEMORY();
			result = callback(o, data);
			Py_DECREF(o);
		}
		else {
			/* a real unshadowed Python frame */
			result = callback((PyObject*) f, data);
		}
		if (result != NULL)
			break;
		f = f->f_back;
	}
        RECLIMIT_SAFE_LEAVE();
	return result;
}



static PyObject* visit_nth_frame(PyObject* o, void* n)
{
	/* count the calls to the function and return 'o' when
	   the counter reaches zero */
	if (!--*(int*)n) {
		Py_INCREF(o);
		return o;
	}
	return NULL;
}

static PyObject* visit_prev_frame(PyObject* o, void* data)
{
	PyObject* cmp = *(PyObject**) data;

	if (cmp != NULL) {
		/* still searching */
		if (PyFrame_Check(o) || PyFrame_Check(cmp)) {
			if (o != cmp) return NULL;
		}
		else {
			PyObject* p1;
			PyObject* p2;

			p1 = PyTuple_GetItem(o,   2);  /* tag */
			p2 = PyTuple_GetItem(cmp, 2);
			if (PyObject_Compare(p1, p2) != 0) return NULL;

			p1 = PyTuple_GetItem(o,   0);  /* code */
			p2 = PyTuple_GetItem(cmp, 0);
			if (p1 != p2) return NULL;

			p1 = PyTuple_GetItem(o,   1);  /* globals */
			p2 = PyTuple_GetItem(cmp, 1);
			if (p1 != p2) return NULL;
		}
		/* found it ! We will succeed the next time
		   visit_find_frame() is called. */
		*(PyObject**) data = NULL;
		return NULL;
	}
	else {
		/* found it the previous time, now return this next 'o' */
		Py_INCREF(o);
		return o;
	}
}

DEFINEFN
PyObject* psyco_find_frame(PyObject* o)
{
	void* result;
	if (PyInt_Check(o)) {
		int depth = PyInt_AsLong(o) + 1;
		if (depth <= 0)
			depth = 1;
		result = pvisitframes(visit_nth_frame, &depth);
	}
	else {
		result = pvisitframes(visit_prev_frame, (void*) &o);
		if (result == NULL && !PyErr_Occurred() && o != NULL)
			PyErr_SetString(PyExc_PsycoError,
					"f_back is invalid when frames are no longer active");
	}
	if (result == NULL && !PyErr_Occurred())
		PyErr_SetString(PyExc_ValueError,
				"call stack is not deep enough");
	return (PyObject*) result;
}

static PyObject* visit_get_globals(PyObject* o, void* ignored)
{
	if (PyFrame_Check(o))
		return ((PyFrameObject*) o)->f_globals;
	else
		return PyTuple_GetItem(o, 1);
}
DEFINEFN
PyObject* psyco_get_globals(void)
{
	PyObject* result = pvisitframes(visit_get_globals, NULL);
	if (result == NULL)
		psyco_fatal_msg("sorry, don't know what to do with no globals");
	return result;
}

static PyFrameObject* cached_frame = NULL;
static PyObject* visit_first_frame(PyObject* o, void* ts)
{
	if (PyFrame_Check(o)) {
		/* a real Python frame: don't return a new reference */
		return (PyObject*) o;
	}
	else {
		/* a Psyco frame: emulate it */
		/* we can't return a new reference, so we have to remember
		   the last frame we emulated and free it now.  This is
		   not too bad since we can use this as a cache and avoid
		   rebuilding the new emulated frame all the time. */
		PyFrameObject* f;
		PyFrameObject* newf;
		PyObject* co = PyTuple_GetItem(o, 0);
		PyObject* globals = PyTuple_GetItem(o, 1);
		PyThreadState* tstate = (PyThreadState*) ts;
		extra_assert(co != NULL);
		extra_assert(globals != NULL);
		while (cached_frame != NULL) {
			f = cached_frame;
			if ((PyObject*) f->f_code == co
			    && f->f_globals == globals) {
				/* reuse the cached frame */
				f->f_tstate = tstate;
				return (PyObject*) f;
			}
			cached_frame = NULL;
			Py_DECREF(f);  /* might set cached_frame again
					  XXX could this loop never end? */
		}
		newf = psyco_build_pyframe(co, globals, tstate);
		while (cached_frame != NULL) {
			/* worst-case safe...  this is unlikely */
			f = cached_frame;
			cached_frame = NULL;
			Py_DECREF(f);
		}
		cached_frame = newf;   /* transfer ownership */
		return (PyObject*) newf;
	}
}
static PyFrameObject* psyco_threadstate_getframe(PyThreadState* self)
{
	return (PyFrameObject*) pvisitframes(visit_first_frame, (void*)self);
}


 /***************************************************************/

INITIALIZATIONFN
void psyco_frames_init(void)
{
	_PyThreadState_GetFrame =
#  if PYTHON_API_VERSION < 1012
		(unaryfunc)
#  endif
		psyco_threadstate_getframe;
}
