#include "frames.h"
#include "pyver.h"
#include "../codemanager.h"
#include "../stats.h"
#include "../vcompiler.h"
#include "../Objects/pobject.h"


 /***************************************************************/

#if HAVE_DYN_COMPILE

/* turn a running frame into its Psyco equivalent, a PsycoObject.
   Return Py_None if the frame cannot be turned into a PsycoObject.
   Never sets an exception. */
inline PyObject* PsycoObject_FromFrame(PyFrameObject* f, int recursion)
{
	int i, extras;
	vinfo_t* v;
	PsycoObject* po;
	RunTimeSource rsrc;
	source_known_t* sk;
	PyCodeObject* co = f->f_code;
	PyObject* merge_points;

	if (f->f_stacktop == NULL) {
          /*#if version >= 2.2.2*/
		/* cannot patch a frame other than the top (running) one */
          /*#else*/
		/* feature requires Python version 2.2.2 or later */
          /*#endif*/
		goto fail;
	}
	merge_points = PyCodeStats_MergePoints(PyCodeStats_Get(co));
	if (merge_points == Py_None) {
		/* unsupported bytecode instructions */
		goto fail;
	}
	if (psyco_mp_flags(merge_points) & MP_FLAGS_HAS_FINALLY) {
		/* incompatible handling of 'finally' blocks */
		goto fail;
	}

	/* the local variables are assumed to be stored as 'fast' variables,
	   not in the f_locals dictionary.  This is currently asserted by
	   the fact that LOAD_NAME and STORE_NAME opcodes are not supported
	   at all.  XXX support LOAD_NAME / STORE_NAME / DELETE_NAME */
	extras = (f->f_valuestack - f->f_localsplus) + co->co_stacksize;

	po = PsycoObject_New(INDEX_LOC_LOCALS_PLUS + extras);
	po->stack_depth = INITIAL_STACK_DEPTH;
	po->vlocals.count = INDEX_LOC_LOCALS_PLUS + extras;
	po->last_used_reg = REG_LOOP_START;
	po->pr.auto_recursion = AUTO_RECURSION(recursion);

	/* initialize po->vlocals */
	Py_INCREF(f->f_globals);
	sk = sk_new((long) f->f_globals, SkFlagPyObj);
	LOC_GLOBALS = vinfo_new(CompileTime_NewSk(sk));
	
	/* move the current arguments, locals, and object stack into
	   their target place */
	for (i = f->f_stacktop - f->f_localsplus; i--; ) {
		PyObject* o = f->f_localsplus[i];
		po->stack_depth += sizeof(long);
		if (o == NULL) {
			/* uninitialized local variable,
			   the corresponding stack position is not used */
			v = psyco_vi_Zero();
		}
		else {
			/* XXX do something more intelligent for cell and
			       free vars */
			/* arguments get borrowed references */
			rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
					       false, false);
			v = vinfo_new(rsrc);
		}
		LOC_LOCALS_PLUS[i] = v;
	}
	/* the rest of the stack in LOC_LOCALS_PLUS is
	   initialized to NULL by PsycoObject_New() */

	/* store the code object */
	po->pr.co = co;
	Py_INCREF(co);  /* XXX never freed */
	po->pr.next_instr = f->f_lasti;
	pyc_data_build(po, merge_points);
	if (f->f_iblock) {
		po->pr.iblock = f->f_iblock;
		memcpy(po->pr.blockstack, f->f_blockstack,
		       sizeof(PyTryBlock)*po->pr.iblock);
	}

	/* set up the CALL return address */
	po->stack_depth += sizeof(long);
	rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
				false, false);
	LOC_CONTINUATION = vinfo_new(rsrc);
	psyco_assert_coherent(po);
	return (PyObject*) po;

 fail:
	Py_INCREF(Py_None);
	return Py_None;
}

/* same as PsycoObject_FromFrame() on any not-yet-started frame with the
   given code object */
inline PyObject* PsycoObject_FromCode(PyCodeObject* co,
                                      PyObject* globals,
                                      int recursion)
{
	int i, argc, ncells, nfrees, extras;
	PyObject* merge_points;
	PsycoObject* po;
	Source rsrc;
	source_known_t* sk;
	vinfo_t* v;
	
	merge_points = PyCodeStats_MergePoints(PyCodeStats_Get(co));
	if (merge_points == Py_None) {
		/* unsupported bytecode instructions */
		goto fail;
	}

	ncells = PyTuple_GET_SIZE(co->co_cellvars);
	nfrees = PyTuple_GET_SIZE(co->co_freevars);
	extras = co->co_stacksize + co->co_nlocals + ncells + nfrees;

	po = PsycoObject_New(INDEX_LOC_LOCALS_PLUS + extras);
	po->stack_depth = INITIAL_STACK_DEPTH;
	po->vlocals.count = INDEX_LOC_LOCALS_PLUS + extras;
	po->last_used_reg = REG_LOOP_START;
	po->pr.auto_recursion = AUTO_RECURSION(recursion);

	/* initialize po->vlocals */
	Py_INCREF(globals);
	sk = sk_new((long) globals, SkFlagPyObj);
	LOC_GLOBALS = vinfo_new(CompileTime_NewSk(sk));

	argc = co->co_argcount;
	if (co->co_flags & CO_VARARGS)
		argc++;
	if (co->co_flags & CO_VARKEYWORDS)
		argc++;

	/* initialize the free and cell vars */
	i = co->co_nlocals + ncells + nfrees;
	if (ncells || nfrees) {
		while (i > co->co_nlocals) {
			po->stack_depth += sizeof(long);
			/* borrowed references from the frame object */
			rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
						false, false);
			v = vinfo_new(rsrc);
			LOC_LOCALS_PLUS[--i] = v;
		}
		/* skip the unbound local variables */
		po->stack_depth += sizeof(long) * (i-argc);
	}
	/* initialize the local variables to zero (unbound) */
	while (i > argc) {
		v = psyco_vi_Zero();
		LOC_LOCALS_PLUS[--i] = v;
	}
	/* initialize the keyword arguments dict */
	if (co->co_flags & CO_VARKEYWORDS) {
		po->stack_depth += sizeof(long);
		rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
					false, false);
		v = vinfo_new(rsrc);
		/* known to be a dict */
                /*Psyco_AssertType(NULL, v, &PyDict_Type);*/
		rsrc = CompileTime_New((long) &PyDict_Type);
                v->array = array_new(FIELDS_TOTAL(OB_type));
                v->array->items[iOB_TYPE] = vinfo_new(rsrc);
		LOC_LOCALS_PLUS[--i] = v;
	}
	/* initialize the extra arguments tuple */
	if (co->co_flags & CO_VARARGS) {
		po->stack_depth += sizeof(long);
		rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
					false, false);
		v = vinfo_new(rsrc);
		/* known to be a tuple */
		rsrc = CompileTime_New((long) &PyTuple_Type);
		v->array = array_new(iOB_TYPE+1);
		v->array->items[iOB_TYPE] = vinfo_new(rsrc);
		LOC_LOCALS_PLUS[--i] = v;
	}
	/* initialize the regular arguments */
	while (i > 0) {
		/* XXX do something more intelligent for cell and
		       free vars */
		po->stack_depth += sizeof(long);
		/* arguments get borrowed references */
		rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
					false, false);
		v = vinfo_new(rsrc);
		LOC_LOCALS_PLUS[--i] = v;
	}
	/* the rest of the stack in LOC_LOCALS_PLUS is
	   initialized to NULL by PsycoObject_New() */

	/* store the code object */
	po->pr.co = co;
	Py_INCREF(co);  /* XXX never freed */
	pyc_data_build(po, merge_points);

	/* set up the CALL return address */
	po->stack_depth += sizeof(long);
	rsrc = RunTime_NewStack(po->stack_depth, REG_NONE,
				false, false);
	LOC_CONTINUATION = vinfo_new(rsrc);
	return (PyObject*) po;

 fail:
	Py_INCREF(Py_None);
	return Py_None;
}

DEFINEFN
PyObject* PsycoCode_CompileCode(PyCodeObject* co,
                                PyObject* globals,
                                int recursion)
{
	mergepoint_t* mp;
	PsycoObject* po;
	PyObject* o = PsycoObject_FromCode(co, globals, recursion);
	if (o == Py_None)
		return o;

	/* compile the function */
	po = (PsycoObject*) o;
	mp = PsycoObject_Ready(po);
	return (PyObject*) psyco_compile_code(po, mp);
}

DEFINEFN
PyObject* PsycoCode_CompileFrame(PyFrameObject* f, int recursion)
{
	mergepoint_t* mp;
	PsycoObject* po;
	PyObject* o = PsycoObject_FromFrame(f, recursion);
	if (o == Py_None)
		return o;

	/* compile the function */
	po = (PsycoObject*) o;
	mp = psyco_exact_merge_point(po->pr.merge_points, po->pr.next_instr);
	if (mp != NULL)
		psyco_delete_unused_vars(po, &mp->entries);
	return (PyObject*) psyco_compile_code(po, mp);
}

DEFINEFN
bool PsycoCode_Run(PyObject* codebuf, PyFrameObject* f)
{
	PyObject* tdict;
	PyFrameRuntime* fruntime;
	stack_frame_info_t** finfo;
	int err;
	long* initial_stack;
	PyObject* result;
        PyCodeObject* co = f->f_code;

	extra_assert(codebuf != NULL);
	extra_assert(CodeBuffer_Check(codebuf));
	
	/* over the current Python frame, a lightweight chained list of
	   Psyco frames will be built. Mark the current Python frame as
	   the starting point of this chained list. */
	tdict = psyco_thread_dict();
	if (tdict==NULL) return false;
	fruntime = PyCStruct_NEW(PyFrameRuntime, PyFrameRuntime_dealloc);
        Py_INCREF(f);
        fruntime->cs_key = (PyObject*) f;
        fruntime->psy_frames_start = &finfo;
        fruntime->psy_code = co;
        fruntime->psy_globals = f->f_globals;
	extra_assert(PyDict_GetItem(tdict, (PyObject*) f) == NULL);
	err = PyDict_SetItem(tdict, (PyObject*) f, (PyObject*) fruntime);
	Py_DECREF(fruntime);
	if (err) return false;
	/* Warning, no 'return' between this point and the PyDict_DelItem()
	   below */
        
	/* get the actual arguments */
	initial_stack = (long*) f->f_localsplus;

	/* run! */
        Py_INCREF(codebuf);
	result = psyco_processor_run((CodeBufferObject*) codebuf,
				     initial_stack, &finfo);
	Py_DECREF(codebuf);
	psyco_trash_object(NULL);  /* free any trashed object now */

#if CODE_DUMP >= 2
        psyco_dump_code_buffers();
#endif
	if (PyDict_DelItem(tdict, (PyObject*) f)) {
		Py_XDECREF(result);
		result = NULL;
	}
	if (result == NULL) {
		extra_assert(PyErr_Occurred());
		return false;  /* exception */
	}
	else {
		/* to emulate the return, move the current position to
		   the end of the function code.  We assume that the
		   last instruction of any code object is a RETURN_VALUE. */
		PyObject** p;
		int new_i = PyString_GET_SIZE(co->co_code) - 1;
		   /* RETURN_VALUE */
		psyco_assert(PyString_AS_STRING(co->co_code)[new_i] == 83);
		f->f_lasti = new_i;
		f->f_iblock = 0;

		/* free the stack */
		for (p=f->f_stacktop; --p >= f->f_valuestack; ) {
			Py_XDECREF(*p);
			*p = NULL;
		}
		/* push the result alone on the stack */
		p = f->f_valuestack;
		*p++ = result;  /* consume a ref */
		f->f_stacktop = p;

		extra_assert(!PyErr_Occurred());
		return true;
	}
}

#endif /* HAVE_DYN_COMPILE */


 /***************************************************************/

#define FRAME_STACK_ALLOC_BY	83   /* about 1KB */

DEFINEFN
stack_frame_info_t* psyco_finfo(PsycoObject* callee)
{
	Source sglobals;
	static stack_frame_info_t* current = NULL;
	static stack_frame_info_t* end = NULL;
	if (current == end) {
		psyco_memory_usage += sizeof(stack_frame_info_t) *
			FRAME_STACK_ALLOC_BY;
		current = PyMem_NEW(stack_frame_info_t, FRAME_STACK_ALLOC_BY);
		if (current == NULL)
			OUT_OF_MEMORY();
		end = current + FRAME_STACK_ALLOC_BY;
	}
	current->co = callee->pr.co;
	sglobals = callee->vlocals.items[INDEX_LOC_GLOBALS]->source;
	if (is_compiletime(sglobals))
		current->globals = (PyObject*) CompileTime_Get(sglobals)->value;
	else
		current->globals = NULL;  /* uncommon */
	
	return current++;
}

DEFINEFN
void PyFrameRuntime_dealloc(PyFrameRuntime* self)
{
	/* nothing */
}

DEFINEFN
PyFrameObject* psyco_emulate_frame(stack_frame_info_t* finfo,
				   PyObject* default_globals)
{
	PyFrameObject* back;
	PyFrameObject* result;
	PyThreadState* tstate = PyThreadState_GET();
	
	/* frame objects are not created in stack order
	   with Psyco, so it's probably better not to
	   create plain wrong chained lists */
	back = tstate->frame;
	tstate->frame = NULL;
	result = PyFrame_New(tstate, finfo->co,
			     finfo->globals!=NULL?finfo->globals:default_globals,
			     NULL);
        result->f_lasti = -1;  /* can be used to identify emulated frames */
	tstate->frame = back;
	return result;
}

static PyFrameObject* pgetframe(int depth, struct stack_frame_info_s* psy_frame)
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

	PyFrameRuntime* fstart;
	PyObject* tdict = psyco_thread_dict();
	PyFrameObject* f = PyThreadState_Get()->frame;

	if (depth < 0)
		depth = 0;
	
	while (f != NULL) {
		/* is this Python frame the starting point of a chained
		   list of Psyco frames ? */
		fstart = (PyFrameRuntime*) PyDict_GetItem(tdict, (PyObject*) f);
		if (fstart != NULL) {
			/* Yes. Get the list start. */
			stack_frame_info_t** f1;
			stack_frame_info_t** finfo;
			finfo = *(fstart->psy_frames_start);

			/* Count the frames in the list. The end is marked by
			   a pointer with an odd integer value (actually the
			   least significant byte of the integer value is -1,
			   but real pointers cannot be odd at all because they
			   are aligned anyway). */
			for (f1 = finfo; (((long)(*f1)) & 1) == 0;
			     f1 = psyco_next_stack_frame(f1))
				depth--;

			if (depth < 0) {
				/* The requested frame is one of these Psyco
				   frames */
				while (++depth != 0)
					finfo = psyco_next_stack_frame(finfo);
				*psy_frame = **finfo;  /* copy structure */
				return f;
			}
			if (depth == 0) {
				/* the result is a real Python frame
				   shadowed by a Psyco frame, i.e. a
				   proxy function. */
				psy_frame->co = fstart->psy_code;
				psy_frame->globals = fstart->psy_globals;
				return f;
			}
		}
		if (depth-- == 0) {
			/* the result is a real Python frame */
			return f;
		}
		f = f->f_back;
	}
	return NULL;
}

DEFINEFN PyFrameObject *
psyco_get_frame(int depth, struct stack_frame_info_s* psy_frame)
{
	PyFrameObject* result = pgetframe(depth, psy_frame);
	if (result == NULL && !PyErr_Occurred())
		PyErr_SetString(PyExc_ValueError,
				"call stack is not deep enough");
	return result;
}


#if HAVE_PYTHON_SUPPORT
static PyFrameObject* psyco_threadstate_getframe(PyThreadState* self)
{
	PyFrameObject* f;
	stack_frame_info_t finfo;
	
	finfo.co = NULL;
	f = pgetframe(0, &finfo);
	if (f == NULL) {
		return NULL;  /* no current frame */
	}

	if (finfo.co != NULL) {
		/* a Psyco frame: emulate it */
		f = psyco_emulate_frame(&finfo, f->f_globals);
		psyco_trash_object((PyObject*) f);
	}
	/* else a real Python frame: don't return a new reference */
	return f;
}
#endif


 /***************************************************************/

INITIALIZATIONFN
void psyco_frames_init(void)
{
#if HAVE_PYTHON_SUPPORT
	_PyThreadState_GetFrame =
#  if PYTHON_API_VERSION < 1012
		(unaryfunc)
#  endif
		psyco_threadstate_getframe;
#endif
}
