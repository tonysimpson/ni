#include "pycompiler.h"
#include "../pycencoding.h"
#include "../processor.h"
#include "../dispatcher.h"
#include "../mergepoints.h"
#include "../codemanager.h"

#include "../Objects/pabstract.h"
#include "../Objects/pintobject.h"
#include "../Objects/plongobject.h"
#include "../Objects/ptupleobject.h"
#include "../Objects/plistobject.h"
#include "../Objects/pdictobject.h"
#include "../Objects/pfuncobject.h"

#include <eval.h>
#include "pycinternal.h"


#define KNOWN_VAR(type, varname, loc)   \
    type varname = (type)(KNOWN_SOURCE(loc)->value)

#ifdef FOR_ITER
# define OLD_STYLE_LOOP   0
#else
# define OLD_STYLE_LOOP   1
#endif


/*****************************************************************/
 /***   Meta functions                                          ***/

DEFINEVAR PyObject* Psyco_Meta_Dict = NULL;

DEFINEFN
void Psyco_DefineMeta(void* c_function, void* psyco_function)
{
	PyObject* key;
	PyObject* value;
	if (Psyco_Meta_Dict == NULL) {
		Psyco_Meta_Dict = PyDict_New();
		if (Psyco_Meta_Dict == NULL)
			return;
	}
	if (c_function == NULL) {
		debug_printf(2, ("init: C function pointer NULL in CPython\n"));
		return;
	}
	key = PyInt_FromLong((long) c_function);
	if (key != NULL) {
		value = PyInt_FromLong((long) psyco_function);
		if (value != NULL) {
			PyDict_SetItem(Psyco_Meta_Dict, key, value);
			Py_DECREF(value);
		}
		Py_DECREF(key);
	}
}

DEFINEFN
PyObject* Psyco_DefineMetaModule(char* modulename)
{
	PyObject* module = PyImport_ImportModule(modulename);
	if (module == NULL) {
		PyErr_Clear();
		debug_printf(1, ("init: module %s not found\n",
				 modulename));
	}
	else {
		debug_printf(2, ("init: activated module %s\n", modulename));
	}
	return module;
}

DEFINEFN
PyObject* Psyco_GetModuleObject(PyObject* module, char* name,
				PyTypeObject* expected_type)
{
	PyObject* fobj;
	if (module == NULL)
		return NULL;
	
	fobj = PyObject_GetAttrString(module, name);
	if (fobj == NULL) {
		debug_printf(1, ("init: %s.%s not found\n",
				 PyModule_GetName(module), name));
		PyErr_Clear();
		return NULL;
	}
	if (expected_type != NULL && !PyObject_TypeCheck(fobj, expected_type)) {
		debug_printf(1, ("init: %s.%s is of type %.200s instead of "
				 "%.200s\n", PyModule_GetName(module), name,
				 fobj->ob_type->tp_name,
				 expected_type->tp_name));
		Py_DECREF(fobj);
		fobj = NULL;
	}
	return fobj;
}

DEFINEFN
PyCFunction Psyco_DefineModuleFn(PyObject* module, char* meth_name,
				 int meth_flags, void* meta_fn)
{
	PyCFunction f;
	PyObject* fobj = Psyco_GetModuleObject(module, meth_name,
					       &PyCFunction_Type);
	if (fobj == NULL)
		return NULL;

	if (PyCFunction_GET_FLAGS(fobj) != meth_flags) {
		f = NULL;
		debug_printf(1, ("init: %s.%s built-in has wrong "
				 "meth_flags\n", PyModule_GetName(module),
				 meth_name));
	}
	else {
		f = PyCFunction_GET_FUNCTION(fobj);
		Psyco_DefineMeta(f, meta_fn);
	}
	Py_DECREF(fobj);
	return f;
}

#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
DEFINEFN
PyCFunction Psyco_DefineModuleC(PyObject* module, char* meth_name,
				int meth_flags, void* meta_fn,
				void* meta_type_new)
{
	PyObject* o = Psyco_GetModuleObject(module, meth_name, NULL);
	if (o == NULL)
		return NULL;
	if (PyType_Check(o) &&
	    PyType_HasFeature((PyTypeObject*) o, Py_TPFLAGS_HAVE_CLASS) &&
	    ((PyTypeObject*)o)->tp_new != NULL) {
		/* maps a callable type */
		Psyco_DefineMeta(((PyTypeObject*)o)->tp_new, meta_type_new);
		return NULL;
	}
	else
		return Psyco_DefineModuleFn(module, meth_name,
					    meth_flags, meta_fn);
}
#endif


#define FORGET_REF     if (has_rtref(vi->source))			\
			    vi->source = remove_rtref(vi->source)

DEFINEFN
vinfo_t* generic_call_check(PsycoObject* po, int flags, vinfo_t* vi)
{
	condition_code_t cc;
	
	switch (flags & CfPyErrMask) {

	case CfPyErrIfNull:   /* a return == 0 (or NULL) means an error */
                cc = integer_cmp_i(po, vi, 0, Py_EQ);
		break;

	case CfPyErrIfNonNull:   /* a return != 0 means an error */
                cc = integer_cmp_i(po, vi, 0, Py_NE);
		break;

	case CfPyErrIfNeg:   /* a return < 0 means an error */
		cc = integer_cmp_i(po, vi, 0, Py_LT);
		break;

	case CfPyErrIfMinus1:     /* only -1 means an error */
		cc = integer_cmp_i(po, vi, -1, Py_EQ);
		break;

	case CfPyErrCheck:    /* always check with PyErr_Occurred() */
		cc = integer_NON_NULL(po, psyco_PyErr_Occurred(po));
		break;

	case CfPyErrCheckMinus1:   /* use PyErr_Occurred() if return is -1 */
		cc = integer_cmp_i(po, vi, -1, Py_NE);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_t(po, cc))
			return vi;   /* result is not -1, ok */
		cc = integer_NON_NULL(po, psyco_PyErr_Occurred(po));
		break;

        case CfPyErrCheckNeg:   /* use PyErr_Occurred() if return is < 0 */
		cc = integer_cmp_i(po, vi, 0, Py_GE);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_t(po, cc))
			return vi;   /* result is >= 0, ok */
		cc = integer_NON_NULL(po, psyco_PyErr_Occurred(po));
		break;

	case CfPyErrNotImplemented:   /* test for a Py_NotImplemented result */
		cc = integer_cmp_i(po, vi, (long) Py_NotImplemented, Py_EQ);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_f(po, cc)) {
			/* result is Py_NotImplemented */
			vinfo_decref(vi, po);
			return psyco_vi_NotImplemented();
		}
		cc = integer_cmp_i(po, vi, 0, Py_EQ);
		break;

#if HAVE_GENERATORS
	case CfPyErrIterNext:    /* specially for tp_iternext slots */
		cc = integer_cmp_i(po, vi, 0, Py_NE);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_t(po, cc))
			return vi;   /* result is not 0, ok */
		
		FORGET_REF;  /* NULL result */
		vinfo_decref(vi, po);
		cc = integer_NON_NULL(po, psyco_PyErr_Occurred(po));
		if (cc == CC_ERROR || runtime_condition_f(po, cc))
			goto PythonError;  /* PyErr_Occurred() returns true */

		/* NULL result with no error set; it is the end of the
		   iteration. Raise a pseudo PyErr_StopIteration. */
		PycException_SetVInfo(po, PyExc_StopIteration, psyco_vi_None());
		return NULL;
#endif

	case CfPyErrAlways:   /* always set an exception */
		cc = CC_ALWAYS_TRUE;
		break;

	default:
		return vi;
	}
	
	if (cc != CC_ERROR && !runtime_condition_f(po, cc))
		return vi;   /* no error */

   Error:
	if ((flags & CfReturnMask) == CfReturnRef) {
		/* in case of error, 'vi' is not a real
		   reference, so forget it */
		FORGET_REF;
	}
	vinfo_decref(vi, po);
		
	/* We have detected that a Python exception must be set at
	   this point. */
   PythonError:
	PycException_Raise(po, vinfo_new(VirtualTime_New(&ERtPython)),
			   NULL);
	return NULL;
}
#undef FORGET_REF

DEFINEFN
vinfo_t* generic_call_ct(int flags, long result)
{
	switch (flags & CfPyErrMask) {
		
	case CfPyErrNotImplemented:   /* test for a Py_NotImplemented result */
		if ((PyObject*) result == Py_NotImplemented)
			return psyco_vi_NotImplemented();
		break;
	}
	return NULL;  /* meaning: nothing particular */
}

DEFINEFN
vinfo_t* Psyco_Meta1x(PsycoObject* po, void* c_function, int flags,
		      const char* arguments, long a1)
{
	void* psyco_fn = Psyco_Lookup(c_function);
	if (psyco_fn == NULL) {
		return psyco_generic_call(po, c_function, flags, arguments,
					  a1);
        }
	else
		return ((vinfo_t*(*)(PsycoObject*, long))(psyco_fn))
			(po, a1);
}

DEFINEFN
vinfo_t* Psyco_Meta2x(PsycoObject* po, void* c_function, int flags,
		      const char* arguments, long a1, long a2)
{
	void* psyco_fn = Psyco_Lookup(c_function);
	if (psyco_fn == NULL)
		return psyco_generic_call(po, c_function, flags, arguments,
					  a1, a2);
	else
		return ((vinfo_t*(*)(PsycoObject*, long, long))(psyco_fn))
			(po, a1, a2);
}

DEFINEFN
vinfo_t* Psyco_Meta3x(PsycoObject* po, void* c_function, int flags,
		      const char* arguments, long a1, long a2, long a3)
{
	void* psyco_fn = Psyco_Lookup(c_function);
	if (psyco_fn == NULL)
		return psyco_generic_call(po, c_function, flags, arguments,
					  a1, a2, a3);
	else
		return ((vinfo_t*(*)(PsycoObject*, long, long, long))(psyco_fn))
			(po, a1, a2, a3);
}


/***************************************************************/
 /***   pyc_data_t                                            ***/


DEFINEFN
void pyc_data_build(PsycoObject* po, PyObject* merge_points)
{
	/* rebuild the data in the pyc_data_t */
	int i;
        PyCodeObject* co = po->pr.co;
	int stack_base = po->vlocals.count - co->co_stacksize;
	for (i=stack_base; i<po->vlocals.count; i++)
		if (po->vlocals.items[i] == NULL)
			break;
	po->pr.stack_base = stack_base;
	po->pr.stack_level = i - stack_base;
	po->pr.merge_points = merge_points;
}

static void block_setup(PsycoObject* po, int type, int handler, int level)
{
  PyTryBlock *b;
  if (po->pr.iblock >= CO_MAXBLOCKS)
    Py_FatalError("block stack overflow");
  b = &po->pr.blockstack[po->pr.iblock++];
  b->b_type = type;
  b->b_level = level;
  b->b_handler = handler;
}

static PyTryBlock* block_pop(PsycoObject* po)
{
  if (po->pr.iblock <= 0)
    Py_FatalError("block stack underflow");
  return &po->pr.blockstack[--po->pr.iblock];
}


/*****************************************************************/
 /***   Compile-time Pseudo exceptions                          ***/


DEFINEVAR source_virtual_t ERtPython;  /* Exception raised by Python */
DEFINEVAR source_virtual_t EReturn;    /* 'return' statement */
DEFINEVAR source_virtual_t EBreak;     /* 'break' statement */
DEFINEVAR source_virtual_t EContinue;  /* 'continue' statement */

DEFINEFN
void PycException_SetString(PsycoObject* po, PyObject* e, const char* text)
{
	PyObject* s = PyString_FromString(text);
	if (s == NULL)
		OUT_OF_MEMORY();
	PycException_SetObject(po, e, s);
}

DEFINEFN
void PycException_SetObject(PsycoObject* po, PyObject* e, PyObject* v)
{
	PycException_Raise(po, vinfo_new(CompileTime_New((long) e)),
			   vinfo_new(CompileTime_NewSk(sk_new((long) v,
                                                              SkFlagPyObj))));
}

DEFINEFN
void PycException_SetVInfo(PsycoObject* po, PyObject* e, vinfo_t* v)
{
	PycException_Raise(po, vinfo_new(CompileTime_New((long) e)), v);
}

DEFINEFN
void PycException_Promote(PsycoObject* po, vinfo_t* vi, c_promotion_t* promotion)
{
	vinfo_incref(vi);
	PycException_Raise(po, vinfo_new(VirtualTime_New(&promotion->header)),
			   vi);
}

#if !HAVE_PyString_FromFormatV
/* This code is copied from Python 2.2. */
DEFINEFN PyObject *
PyString_FromFormatV(const char *format, va_list vargs)
{
	va_list count;
	int n = 0;
	const char* f;
	char *s;
	PyObject* string;

#ifdef VA_LIST_IS_ARRAY
	memcpy(count, vargs, sizeof(va_list));
#else
	count = vargs;
#endif
	/* step 1: figure out how large a buffer we need */
	for (f = format; *f; f++) {
		if (*f == '%') {
			const char* p = f;
			while (*++f && *f != '%' && !isalpha(Py_CHARMASK(*f)))
				;

			/* skip the 'l' in %ld, since it doesn't change the
			   width.  although only %d is supported (see
			   "expand" section below), others can be easily
			   added */
			if (*f == 'l' && *(f+1) == 'd')
				++f;
			
			switch (*f) {
			case 'c':
				(void)va_arg(count, int);
				/* fall through... */
			case '%':
				n++;
				break;
			case 'd': case 'i': case 'x':
				(void) va_arg(count, int);
				/* 20 bytes is enough to hold a 64-bit
				   integer.  Decimal takes the most space.
				   This isn't enough for octal. */
				n += 20;
				break;
			case 's':
				s = va_arg(count, char*);
				n += strlen(s);
				break;
			case 'p':
				(void) va_arg(count, int);
				/* maximum 64-bit pointer representation:
				 * 0xffffffffffffffff
				 * so 19 characters is enough.
				 * XXX I count 18 -- what's the extra for?
				 */
				n += 19;
				break;
			default:
				/* if we stumble upon an unknown
				   formatting code, copy the rest of
				   the format string to the output
				   string. (we cannot just skip the
				   code, since there's no way to know
				   what's in the argument list) */ 
				n += strlen(p);
				goto expand;
			}
		} else
			n++;
	}
 expand:
	/* step 2: fill the buffer */
	/* Since we've analyzed how much space we need for the worst case,
	   use sprintf directly instead of the slower PyOS_snprintf. */
	string = PyString_FromStringAndSize(NULL, n);
	if (!string)
		return NULL;
	
	s = PyString_AsString(string);

	for (f = format; *f; f++) {
		if (*f == '%') {
			const char* p = f++;
			int i, longflag = 0;
			/* parse the width.precision part (we're only
			   interested in the precision value, if any) */
			n = 0;
			while (isdigit(Py_CHARMASK(*f)))
				n = (n*10) + *f++ - '0';
			if (*f == '.') {
				f++;
				n = 0;
				while (isdigit(Py_CHARMASK(*f)))
					n = (n*10) + *f++ - '0';
			}
			while (*f && *f != '%' && !isalpha(Py_CHARMASK(*f)))
				f++;
			/* handle the long flag, but only for %ld.  others
			   can be added when necessary. */
			if (*f == 'l' && *(f+1) == 'd') {
				longflag = 1;
				++f;
			}
			
			switch (*f) {
			case 'c':
				*s++ = va_arg(vargs, int);
				break;
			case 'd':
				if (longflag)
					sprintf(s, "%ld", va_arg(vargs, long));
				else
					sprintf(s, "%d", va_arg(vargs, int));
				s += strlen(s);
				break;
			case 'i':
				sprintf(s, "%i", va_arg(vargs, int));
				s += strlen(s);
				break;
			case 'x':
				sprintf(s, "%x", va_arg(vargs, int));
				s += strlen(s);
				break;
			case 's':
				p = va_arg(vargs, char*);
				i = strlen(p);
				if (n > 0 && i > n)
					i = n;
				memcpy(s, p, i);
				s += i;
				break;
			case 'p':
				sprintf(s, "%p", va_arg(vargs, void*));
				/* %p is ill-defined:  ensure leading 0x. */
				if (s[1] == 'X')
					s[1] = 'x';
				else if (s[1] != 'x') {
					memmove(s+2, s, strlen(s)+1);
					s[0] = '0';
					s[1] = 'x';
				}
				s += strlen(s);
				break;
			case '%':
				*s++ = '%';
				break;
			default:
				strcpy(s, p);
				s += strlen(s);
				goto end;
			}
		} else
			*s++ = *f;
	}
	
 end:
	_PyString_Resize(&string, s - PyString_AS_STRING(string));
	return string;
}
#endif /* !HAVE_PyString_FromFormatV */

DEFINEFN
void PycException_SetFormat(PsycoObject* po, PyObject* e, const char* fmt, ...)
{
	PyObject* s;
	va_list vargs;

#ifdef HAVE_STDARG_PROTOTYPES
	va_start(vargs, fmt);
#else
	va_start(vargs);
#endif
	s = PyString_FromFormatV(fmt, vargs);
	va_end(vargs);

	if (s == NULL)
		OUT_OF_MEMORY();
	PycException_SetObject(po, e, s);
}

DEFINEFN
vinfo_t* PycException_Matches(PsycoObject* po, PyObject* e)
{
	vinfo_t* result;
	if (PycException_Is(po, &ERtPython)) {
		/* Exception raised by Python, emit a call to
		   PyErr_ExceptionMatches() */
		result = psyco_generic_call(po, PyErr_ExceptionMatches,
					    CfReturnNormal, "l", (long) e);
	}
	else if (PycException_IsPython(po)) {
		/* Exception virtually set, that is, present in the PsycoObject
		   but not actually set at run-time by Python's PyErr_SetXxx */
		result = psyco_generic_call(po, PyErr_GivenExceptionMatches,
					    CfPure | CfReturnNormal,
					    "vl", po->pr.exc, (long) e);
	}
	else {   /* pseudo exceptions don't match real Python ones */
		result = psyco_vi_Zero();
	}
	return result;
}

inline void clear_pseudo_exception(PsycoObject* po)
{
	extra_assert(PycException_Occurred(po));
	if (po->pr.tb != NULL) {
		vinfo_decref(po->pr.tb, po);
		po->pr.tb = NULL;
	}
        if (po->pr.val != NULL) {
		vinfo_decref(po->pr.val, po);
		po->pr.val = NULL;
	}
	vinfo_decref(po->pr.exc, po);
	po->pr.exc = NULL;
}

DEFINEFN
void PycException_Clear(PsycoObject* po)
{
	if (PycException_Is(po, &ERtPython)) {
		/* Clear the Python exception set at run-time */
		psyco_call_void(po, PyErr_Clear);
	}
	clear_pseudo_exception(po);
}

DEFINEFN
void psyco_virtualize_exception(PsycoObject* po)
{
	/* fetch a Python exception set at compile-time (that is, now)
	   and turn into a pseudo-exception (typically to be re-raised
	   at run-time). */
	PyObject *exc, *val, *tb;
	vinfo_t *vexc, *vval, *vtb;
	PyErr_Fetch(&exc, &val, &tb);
	extra_assert(exc != NULL);

	vexc = vinfo_new(CompileTime_NewSk(sk_new((long) exc, SkFlagPyObj)));
	vval = vinfo_new(CompileTime_NewSk(sk_new((long) val, SkFlagPyObj)));
	vtb  = tb == NULL ? NULL :
	       vinfo_new(CompileTime_NewSk(sk_new((long) tb,  SkFlagPyObj)));
	PycException_Restore(po, vexc, vval, vtb);
}

static void cimpl_pyerr_fetch(PyObject* target[])
{
        extra_assert(PyErr_Occurred());
	PyErr_Fetch(target+0, target+1, target+2);
	if (target[0] == NULL) {
		target[0] = Py_None;
		Py_INCREF(Py_None);
	}
	if (target[1] == NULL) {
		target[1] = Py_None;
		Py_INCREF(Py_None);
	}
	if (target[2] == NULL) {
		target[2] = Py_None;
		Py_INCREF(Py_None);
	}
}

DEFINEFN
void cimpl_finalize_frame_locals(PyObject* f_exc_type,
				 PyObject* f_exc_value,
				 PyObject* f_exc_traceback)
{
	/* Called by code emitted by pycencoding.h when a function exits,
	   but only if f_exc_type!=NULL. Works like reset_exc_info() of
	   Python. */
	PyThreadState *tstate = PyThreadState_GET();
	PyObject *tmp_type, *tmp_value, *tmp_tb;

	/* This frame caught an exception */
	tmp_type = tstate->exc_type;
	tmp_value = tstate->exc_value;
	tmp_tb = tstate->exc_traceback;
	tstate->exc_type = f_exc_type;  /* references are transferred */
	tstate->exc_value = f_exc_value;
	tstate->exc_traceback = f_exc_traceback;
	Py_XDECREF(tmp_type);
	Py_XDECREF(tmp_value);
	Py_XDECREF(tmp_tb);
	/* For b/w compatibility */
	PySys_SetObject("exc_type", f_exc_type);
	PySys_SetObject("exc_value", f_exc_value);
	PySys_SetObject("exc_traceback", f_exc_traceback);
}

inline void cimpl_set_exc_info(PyObject* target[],
			       PyObject** f_exc_type,
			       PyObject** f_exc_value,
			       PyObject** f_exc_traceback)
{
	/* Equivalent of PyErr_NormalizeException() + set_exc_info() */
	PyThreadState *tstate = PyThreadState_GET();
        PyObject *type, *value, *tb;
	PyObject *tmp_type, *tmp_value, *tmp_tb;

        PyErr_NormalizeException(target+0, target+1, target+2);
        type = target[0];
        value = target[1];
	tb = target[2];

	if (*f_exc_type == NULL) {
		/* This frame didn't catch an exception before */
		/* Save previous exception of this thread in this frame */
		if (tstate->exc_type == NULL) {
			Py_INCREF(Py_None);
			tstate->exc_type = Py_None;
		}
		Py_INCREF(tstate->exc_type);
		Py_XINCREF(tstate->exc_value);
		Py_XINCREF(tstate->exc_traceback);
		*f_exc_type = tstate->exc_type;
		*f_exc_value = tstate->exc_value;
		*f_exc_traceback = tstate->exc_traceback;
	}
	/* Set new exception for this thread */
	tmp_type = tstate->exc_type;
	tmp_value = tstate->exc_value;
	tmp_tb = tstate->exc_traceback;
	Py_XINCREF(type);
	Py_XINCREF(value);
	Py_XINCREF(tb);
	tstate->exc_type = type;
	tstate->exc_value = value;
	tstate->exc_traceback = tb;
	Py_XDECREF(tmp_type);
	Py_XDECREF(tmp_value);
	Py_XDECREF(tmp_tb);
	/* For b/w compatibility */
	PySys_SetObject("exc_type", type);
	PySys_SetObject("exc_value", value);
	PySys_SetObject("exc_traceback", tb);
}

static void cimpl_pyerr_fetch_and_normalize(PyObject* target[],
					    PyObject** f_exc_type,
					    PyObject** f_exc_value,
					    PyObject** f_exc_traceback)
{
        extra_assert(PyErr_Occurred());
	PyErr_Fetch(target+0, target+1, target+2);
        cimpl_set_exc_info(target,
			   f_exc_type, f_exc_value, f_exc_traceback);
}

static void cimpl_pyerr_normalize(PyObject* exc, PyObject* val, PyObject* tb,
				  PyObject* target[],
				  PyObject** f_exc_type,
				  PyObject** f_exc_value,
				  PyObject** f_exc_traceback)
{
	target[0] = exc;  Py_INCREF(exc);
	target[1] = val;  Py_XINCREF(val);
	target[2] = tb;   Py_XINCREF(tb);
        cimpl_set_exc_info(target,
			   f_exc_type, f_exc_value, f_exc_traceback);
}

DEFINEFN
void PycException_Fetch(PsycoObject* po)
{
	if (PycException_Is(po, &ERtPython)) {
/* 		vinfo_t* exc = vinfo_new(SOURCE_DUMMY_WITH_REF); */
/* 		vinfo_t* val = vinfo_new(SOURCE_DUMMY_WITH_REF); */
/* 		vinfo_t* tb  = vinfo_new(SOURCE_DUMMY_WITH_REF); */
/* 		psyco_generic_call(po, PyErr_Fetch, CfNoReturnValue, */
/* 				   "rrr", exc, val, tb); */
/* 		vinfo_decref(tb, po); */
		vinfo_array_t* array = array_new(3);
		psyco_generic_call(po, cimpl_pyerr_fetch,
				   CfNoReturnValue, "A", array);
		clear_pseudo_exception(po);
		po->pr.exc = array->items[0];
		po->pr.val = array->items[1];  /* po->pr.val!=NULL after this */
		po->pr.tb  = array->items[2];  /* po->pr.tb!=NULL after this */
		array_release(array);
	}
}

inline bool PycException_FetchNormalize(PsycoObject* po)
{
	vinfo_t* result;
	vinfo_array_t* array = array_new(3);
	vinfo_array_t* f_exc = LOC_CONTINUATION->array;

        /* At runtime, we load the following data into the array:

           array[0] -- exc_type       new exception, normalized
           array[1] -- exc_value      new exception, normalized
           array[2] -- exc_traceback  new exception, normalized
        */
	extra_assert(f_exc->count >= 3);
	if (PycException_Is(po, &ERtPython)) {
		/* fetch and normalize the exception */
		result = psyco_generic_call(po, cimpl_pyerr_fetch_and_normalize,
					    CfNoReturnValue, "Arrr", array,
					    f_exc->items[0],
					    f_exc->items[1],
					    f_exc->items[2]);
	}
	else {
		char args[8];
		args[0] = 'v';
		args[1] = po->pr.val == NULL ? 'l' : 'v';
		args[2] = po->pr.tb  == NULL ? 'l' : 'v';
		args[3] = 'A';
		args[4] = 'r';
		args[5] = 'r';
		args[6] = 'r';
		args[7] = 0;
		/* normalize the already-given exception */
		result = psyco_generic_call(po, cimpl_pyerr_normalize,
					    CfNoReturnValue, args,
					    po->pr.exc, po->pr.val, po->pr.tb,
					    array,
					    f_exc->items[0],
					    f_exc->items[1],
					    f_exc->items[2]);
	}
	if (result == NULL) {
		array_release(array);
		return false;
	}
	clear_pseudo_exception(po);
	po->pr.exc = array->items[0];
	po->pr.val = array->items[1];  /* po->pr.val!=NULL after this */
	po->pr.tb  = array->items[2];  /* po->pr.tb!=NULL after this */
	array_release(array);
	return true;
}

static PyFrameObject* cimpl_new_frame(PyThreadState* tstate, PyCodeObject* code,
				      PyObject* globals, int lasti, int lineno)
{
	/* Make a minimalistic frame object, working around the specific
	   expectations of PyFrame_New(), which is the only reasonable way
	   to create frame objects (because of free lists etc) */
	PyFrameObject* result;
	PyFrameObject* back = tstate->frame;
	tstate->frame = NULL;  /* frame objects are not created in stack order
				  with Psyco, so it's probably better not to
				  create plain wrong chained lists */
	result = PyFrame_New(tstate, code, globals, NULL);
	tstate->frame = back;
	if (result != NULL) {
		result->f_lasti = lasti;
		result->f_lineno = lineno;
	}
	return result;
}

static void cimpl_rt_traceback(PyCodeObject* code, PyObject* globals,
			       int lasti, int lineno)
{
	PyThreadState* tstate = PyThreadState_GET();
	PyFrameObject* f = cimpl_new_frame(tstate, code, globals, lasti, lineno);
	/* an out-of-memory error just replaces the current exception */
	if (f != NULL) {
		PyTraceBack_Here(f);
		Py_DECREF(f);
	}
}

static PyObject* cimpl_vt_traceback(PyCodeObject* code, PyObject* globals,
				    int lasti, int lineno)
{
	/* This is a hack around the limited interface to tracebacks.
	   PyTraceBack_Here() seems to be the only clean way to create new
	   traceback objects. */
	PyObject* oldtb;
	PyObject* newtb;
	PyThreadState* tstate = PyThreadState_GET();
	PyFrameObject* f = cimpl_new_frame(tstate, code, globals, lasti, lineno);
	if (f == NULL) {
		/* out-of-memory error */
		Py_INCREF(Py_None);
		return Py_None;
	}
	oldtb = tstate->curexc_traceback;  /* might be NULL */
	Py_XINCREF(oldtb);
	if (PyTraceBack_Here(f)) {
		/* out-of-memory error */
		Py_XDECREF(oldtb);
		Py_DECREF(f);
		Py_INCREF(Py_None);
		return Py_None;
	}
	/* the new traceback is now in curexc_traceback */
	newtb = tstate->curexc_traceback;  /* extracts the reference */
	tstate->curexc_traceback = oldtb;  /* consumes the reference */
	Py_DECREF(f);
	return newtb;
}

inline void PsycoTraceBack_Here(PsycoObject* po, int lasti)
{
	int lineno = PyCode_Addr2Line(po->pr.co, lasti);
	
	if (PycException_Is(po, &ERtPython)) {
		/* Python exception is actually set at run-time */
		psyco_generic_call(po, cimpl_rt_traceback,
				   CfNoReturnValue, "lvll",
				   (long) po->pr.co, LOC_GLOBALS, lasti, lineno);
	}
	else {
		/* We only have a virtual-time exception (not set in Python),
		   so we build po->pr.tb without actually setting it either */
		extra_assert(po->pr.tb == NULL);
		po->pr.tb = psyco_generic_call(po, cimpl_vt_traceback,
					       CfReturnRef, "lvll",
					       (long) po->pr.co, LOC_GLOBALS,
					       lasti, lineno);
	}
}


 /***************************************************************/
/***                      Initialization                       ***/
 /***************************************************************/

DEFINEVAR source_known_t psyco_skZero;   /* known value 0 */
DEFINEVAR source_known_t psyco_skOne;     /* known value 1 */
DEFINEVAR source_known_t psyco_skNone;     /* known value 'Py_None' */
DEFINEVAR source_known_t psyco_skPy_False;  /* known value 'Py_False' */
DEFINEVAR source_known_t psyco_skPy_True;    /* known value 'Py_True' */
DEFINEVAR source_known_t psyco_skNotImplemented;

static PyObject* s_builtin_object;   /* intern string '__builtins__' */

INITIALIZATIONFN
void psyco_pycompiler_init(void)
{
	s_builtin_object = PyString_InternFromString("__builtins__");
        
        psyco_skZero          .refcount1_flags = SkFlagFixed;
        psyco_skZero          .value           = (long) 0;
        psyco_skOne           .refcount1_flags = SkFlagFixed;
        psyco_skOne           .value           = (long) 1;
        psyco_skNone          .refcount1_flags = SkFlagFixed;
        psyco_skNone          .value           = (long) Py_None;
        psyco_skPy_False      .refcount1_flags = SkFlagFixed;
        psyco_skPy_False      .value           = (long) Py_False;
        psyco_skPy_True       .refcount1_flags = SkFlagFixed;
        psyco_skPy_True       .value           = (long) Py_True;
        psyco_skNotImplemented.refcount1_flags = SkFlagFixed;
        psyco_skNotImplemented.value           = (long) Py_NotImplemented;

	ERtPython = psyco_vsource_not_important;
	EReturn   = psyco_vsource_not_important;
	EContinue = psyco_vsource_not_important;
	EBreak    = psyco_vsource_not_important;
}


 /***************************************************************/
/***                          Compiler                         ***/
 /***************************************************************/

#define CHKSTACK(n)     extra_assert(0 <= po->pr.stack_level+(n) &&             \
                                     po->pr.stack_base+po->pr.stack_level+(n) < \
                                     po->vlocals.count)

#define NEXTOP()	(bytecode[next_instr++])
#define NEXTARG()	(next_instr += 2,                                       \
                         (bytecode[next_instr-1]<<8) + bytecode[next_instr-2])

#define PUSH(v)         (CHKSTACK(0), stack_a[po->pr.stack_level++] = v)
#define POP(targ)       (CHKSTACK(-1), targ = stack_a[--po->pr.stack_level],   \
                         stack_a[po->pr.stack_level] = NULL)
#define NTOP(n)         (CHKSTACK(-(n)), stack_a[po->pr.stack_level-(n)])
#define TOP()           NTOP(1)
#define POP_DECREF()    do { vinfo_t* v1; POP(v1);			\
				vinfo_decref(v1, po); } while (0)

/*#define GETCODEOBJ()    ((PyCodeObject*)(KNOWN_SOURCE(LOC_CODE)->value))*/
#define GETCONST(i)     (PyTuple_GET_ITEM(co->co_consts, i))
#define GETNAMEV(i)     (PyTuple_GET_ITEM(co->co_names, i))

#define GETLOCAL(i)	(LOC_LOCALS_PLUS[i])
#define SETLOCAL(i, v)	do { vinfo_decref(GETLOCAL(i), po); \
                             GETLOCAL(i) = v; } while (0)

#define STACK_POINTER() (stack_a + po->pr.stack_level)
#define INSTR_OFFSET()  (next_instr)
#define STACK_LEVEL()   (po->pr.stack_level)
#define JUMPBY(offset)  (next_instr += (offset))
#define JUMPTO(target)  (next_instr = (target))

#define SAVE_NEXT_INSTR(nextinstr1)   (po->pr.next_instr = (nextinstr1))

/* #define MISSING_OPCODE(opcode)					 */
/* 	case opcode:						 */
/* 		PycException_SetString(po, PyExc_PsycoError, */
/* 				  "opcode '" #opcode "' not implemented"); */
/* 		break */


/***************************************************************/
 /***   LOAD_GLOBAL tricks                                    ***/


static void mark_varying(PsycoObject* po, PyObject* key)
{
	if (po->pr.changing_globals == NULL) {
		po->pr.changing_globals = PyDict_New();
		if (po->pr.changing_globals == NULL)
			OUT_OF_MEMORY();
	}
	if (PyDict_SetItem(po->pr.changing_globals, key, Py_True))
		OUT_OF_MEMORY();
}

/* 'compilation pause' stuff, similar to psyco_coding_pause() */
typedef struct {
	CodeBufferObject* self;
	PsycoObject* po;
	PyObject* varname;
	PyObject* previousvalue;
	code_t* originalmacrocode;
} changed_global_t;

static code_t* do_changed_global(changed_global_t* cg)
{
	PsycoObject* po = cg->po;
	PyObject* key = cg->varname;
	code_t* code = cg->originalmacrocode;
	KNOWN_VAR(PyDictObject*, globals, LOC_GLOBALS);
	PyDictEntry* ep;
	code_t* target;

	/* first check that the value really changed; it could merely
	   have moved in the dictionary table (reallocations etc.) */
	ep = (globals->ma_lookup)(globals, key,
				  ((PyStringObject*) key)->ob_shash);
	
	if (ep->me_value == cg->previousvalue) {
		int index = ep - globals->ma_table;
		/* no real change; update the original macro code
		   and that's it */
                code += SIZE_OF_LOAD_REG_FROM_IMMED;
		DICT_ITEM_UPDCHANGED(code, index);
		return code;  /* execution continues after the macro code */
	}

	/* Mark the global variable as varying and compile again */
	mark_varying(po, key);

	/* 'v' is now run-time, recompile */
	target = (code_t*) psyco_compile_code(po, NULL)->codestart;
	/* XXX don't know what to do with the reference returned by
	   XXX psyco_compile_code() */

	extra_assert(target != code);
	JUMP_TO(target);  /* code a jump from the original code */

        Py_DECREF(cg->varname);
	Py_DECREF(cg->previousvalue);
  /* cannot Py_DECREF(cg->self) because the current function is returning into
     that code now, but any time later is fine: use the trash of codemanager.h */
	psyco_trash_object((PyObject*) cg->self);
	return target;
}

PyObject* psy_get_builtins(PyObject* globals)
{
	PyObject* builtins;
	/* code copied from frameobject.c */
	/* XXX we currently consider the absence
	   of builtins to be a fatal error */
	builtins = PyDict_GetItem((PyObject*) globals, s_builtin_object);
	psyco_assert(builtins != NULL);
	if (PyModule_Check(builtins)) {
		builtins = PyModule_GetDict(builtins);
		psyco_assert(builtins != NULL);
	}
	psyco_assert(PyDict_Check(builtins));
        return builtins;
}


#define GLOBAL_NAME_ERROR_MSG \
	"global name '%.200s' is not defined"
#define UNBOUNDLOCAL_ERROR_MSG \
	"local variable '%.200s' referenced before assignment"

/* Load the global variable whose name is in 'key' (an interned string).
   Returns a new reference to the result or NULL. */
static PyObject* load_global(PsycoObject* po, PyObject* key, int next_instr)
{
	/* XXX this assumes that builtins never change, and that
	   no global variable shadowing a builtin variable is added
           or removed after Psyco has compiled a piece of code.

	   Idea of the current implementation: we perform now the
           look-up in the globals() and remember where it was found
           in the ma_table of the dictionary. We then emit machine
           code that performs a speeded-up version of the lookup by
           checking directly at that place if the same value is still
           in place. If it is not, we call a helper function to redo
           a complete look-up, and if the value is found to have
	   changed, un-promote the variable from compile-time to
           run-time.
	   
	   This requires knowledge of the internal workings of a
	   dictionary.

           This also assumes that a majority of global variables
           never change, which should be the case (typically,
           global functions, classes, etc. do not change).

           A more correct but more involved solution could be
           implemented by adding to dictobject.c some kind of
           'on-change' hooks. The hooks might be called at any
           time; we would then have to insert an espace sequence
           inside of the machine code to trigger recompilation the
           next time the execution reaches this point. This is not
           easy to do because it must be done in a single byte to
           prevent accidental overrides of the next instruction.
           Processors typically have such a minimally-sized
           instruction for debugging purposes but it is not
           supposed to be used outside of debuggers -- it
	   triggers some kind of OS-dependent signal which must
           be caught. Not portable at all. (insert dummy NOPs in
           the first place to make room for a future JMP?)
	*/
	KNOWN_VAR(PyDictObject*, globals, LOC_GLOBALS);
	PyDictEntry* ep;
	PyObject* result;
	/* the compiler only puts interned strings in op_names */
	extra_assert(PyString_CheckExact(key));
	/*extra_assert(((PyStringObject*) key)->ob_sinterned != NULL);*/
	extra_assert(((PyStringObject*) key)->ob_shash != -1);

	ep = (globals->ma_lookup)(globals, key,
				  ((PyStringObject*) key)->ob_shash);
	if (ep->me_value != NULL) {
		/* found in the globals() */
		int index = ep - globals->ma_table;
		CodeBufferObject* onchangebuf;
		PsycoObject* po1 = po;
		changed_global_t* cg;
		code_t* code;
                reg_t mprg;
		
		result = ep->me_value;
                BEGIN_CODE
                NEED_CC();
                NEED_FREE_REG(mprg);
                END_CODE
		
		/* if the object is changed later we will jump to
		   a proxy which we prepare now */
		po = PsycoObject_Duplicate(po);
		onchangebuf = psyco_new_code_buffer(NULL, NULL, &po->codelimit);
		code = (code_t*) onchangebuf->codestart;
		TEMP_SAVE_REGS_FN_CALLS;
		po->code = code;
		cg = (changed_global_t*) psyco_jump_proxy(po,
						   &do_changed_global, 1, 1);
		cg->self = onchangebuf;
		cg->po = po;
		cg->varname = key;             Py_INCREF(key);
                cg->previousvalue = result;    Py_INCREF(result);
		SHRINK_CODE_BUFFER(onchangebuf, (code_t*)(cg+1), "load_global");

		/* go on in the main code sequence */
		po = po1;
		/* write code that quickly checks that the same
		   object is still in place in the dictionary */
                code = po->code;
		cg->originalmacrocode = code;
                LOAD_REG_FROM_IMMED(mprg, (long) globals);
		DICT_ITEM_IFCHANGED(code, index, key, result,
				    (code_t*) onchangebuf->codestart, mprg);
		po->code = code;
                dump_code_buffers();
	}
	else if (strcmp(PyString_AS_STRING(key), "__in_psyco__") == 0) {
		/* special-case __in_psyco__ to always return 1, although
		   its value in the builtins is always 0. This variable
		   can be used by a function to know that it is compiled
		   by Psyco. */
		result = Py_True;
	}
	else {
		/* no such global variable, get the builtins */
		if (po->pr.f_builtins == NULL) {
			po->pr.f_builtins = psy_get_builtins((PyObject*)globals);
		}
		result = PyDict_GetItem(po->pr.f_builtins, key);
		
		if (result == NULL) {
			/* name not found. Maybe it will exist at run-time
			   in the globals. Fall back to the safe
			   cimpl_load_global(). */
			return NULL;
		}
	}
	Py_INCREF(result);
	return result;
}


/***************************************************************/
 /***   Slicing                                               ***/

static vinfo_t* _PsycoEval_SliceIndex(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* result;
	/* TypeSwitch */
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;

	if (PyType_TypeCheck(vtp, &PyInt_Type)) {
		result = PsycoInt_AS_LONG(po, v);
		vinfo_incref(result);
	}
	else if (PyType_TypeCheck(vtp, &PyLong_Type)) {
		result = PsycoLong_AsLong(po, v);
		if (result == NULL) {
			vinfo_t* vi_zero;
			PyObject* long_zero;
			long x;

			if (!PycException_Matches(po, PyExc_OverflowError))
				return NULL;
			/* It's an overflow error, so we need to
			   check the sign of the long integer,
			   set the value to INT_MAX or 0, and clear
			   the error. */
			PycException_Clear(po);

			long_zero = PyLong_FromLong(0L);
			if (long_zero == NULL)
				OUT_OF_MEMORY();
			vi_zero = vinfo_new(CompileTime_NewSk(sk_new
					      ((long) long_zero, SkFlagPyObj)));
			result = PsycoObject_RichCompareBool(po, v, vi_zero,
							     Py_GT);
			vinfo_decref(vi_zero, po);
		        switch (runtime_NON_NULL_t(po, result)) {
			case true:
				x = INT_MAX;
				break;
			case false:
				x = 0;
				break;
			default:
				return NULL;
			}
			result = vinfo_new(CompileTime_New(x));
		}
	}
	else {
		/* no error set */
		result = NULL;
	}
	return result;
}

static vinfo_t* psyco_apply_slice(PsycoObject* po, vinfo_t* u,
				  vinfo_t* v, vinfo_t* w)
{	 /* u[v:w] */
	PyTypeObject *tp;
	PySequenceMethods *sq;

	tp = Psyco_NeedType(po, u);
	if (tp == NULL)
		return NULL;
	sq = tp->tp_as_sequence;

	if (sq && sq->sq_slice) {
		vinfo_t* ilow;
		vinfo_t* ihigh;
		if (v == NULL) {
			ilow = psyco_vi_Zero();
		}
		else {
			ilow = _PsycoEval_SliceIndex(po, v);
			if (ilow == NULL) {
				if (PycException_Occurred(po))
					return NULL;
				goto with_slice_object;
			}
		}
		if (w == NULL)
			ihigh = vinfo_new(CompileTime_New(INT_MAX));
		else {
			ihigh = _PsycoEval_SliceIndex(po, w);
			if (ihigh == NULL) {
				vinfo_decref(ilow, po);
				if (PycException_Occurred(po))
					return NULL;
				goto with_slice_object;
			}
		}
		u = PsycoSequence_GetSlice(po, u, ilow, ihigh);
		vinfo_decref(ihigh, po);
		vinfo_decref(ilow, po);
		return u;
	}

   with_slice_object:
	{
		char modes[4];
		vinfo_t* vslice;
		modes[0] = v == NULL ? 'l' : 'v';
		modes[1] = w == NULL ? 'l' : 'v';
		modes[2] = 'l';
		modes[3] = 0;
		vslice = psyco_generic_call(po, PySlice_New,
					    CfReturnRef|CfPyErrIfNull,
					    modes, v, w, NULL);
		if (vslice != NULL) {
			u = PsycoObject_GetItem(po, u, vslice);
			vinfo_decref(vslice, po);
			return u;
		}
		else
			return NULL;
	}
}

static bool psyco_assign_slice(PsycoObject* po, vinfo_t* u,
			       vinfo_t* v, vinfo_t* w, vinfo_t* x)
{	 /* u[v:w] = x  or  del u[v:w] if x==NULL */
	PyTypeObject *tp;
	PySequenceMethods *sq;
	bool ok;

	tp = Psyco_NeedType(po, u);
	if (tp == NULL)
		return false;
	sq = tp->tp_as_sequence;

	if (sq && sq->sq_slice) {
		vinfo_t* ilow;
		vinfo_t* ihigh;
		if (v == NULL) {
			ilow = psyco_vi_Zero();
		}
		else {
			ilow = _PsycoEval_SliceIndex(po, v);
			if (ilow == NULL) {
				if (PycException_Occurred(po))
					return false;
				goto with_slice_object;
			}
		}
		if (w == NULL)
			ihigh = vinfo_new(CompileTime_New(INT_MAX));
		else {
			ihigh = _PsycoEval_SliceIndex(po, w);
			if (ihigh == NULL) {
				vinfo_decref(ilow, po);
				if (PycException_Occurred(po))
					return false;
				goto with_slice_object;
			}
		}
		ok = PsycoSequence_SetSlice(po, u, ilow, ihigh, x);
		vinfo_decref(ihigh, po);
		vinfo_decref(ilow, po);
		return ok;
	}

   with_slice_object:
	{
		char modes[4];
		vinfo_t* vslice;
		modes[0] = v == NULL ? 'l' : 'v';
		modes[1] = w == NULL ? 'l' : 'v';
		modes[2] = 'l';
		modes[3] = 0;
		vslice = psyco_generic_call(po, PySlice_New,
					    CfReturnRef|CfPyErrIfNull,
					    modes, v, w, NULL);
		if (vslice != NULL) {
			ok = PsycoObject_SetItem(po, u, vslice, x);
			vinfo_decref(vslice, po);
			return ok;
		}
		else
			return false;
	}
}

#if OLD_STYLE_LOOP
static vinfo_t* psyco_loop_subscript(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	PySequenceMethods* sq;
	vinfo_t* vi;
	PyTypeObject* vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return NULL;

	sq = vtp->tp_as_sequence;
	if (sq == NULL || sq->sq_item == NULL) {
		PycException_SetString(po, PyExc_TypeError,
				       "loop over non-sequence");
		return NULL;
	}
	vi = PsycoInt_AS_LONG(po, w);
	if (vi == NULL)
		return NULL;
	return Psyco_META2(po, sq->sq_item, CfReturnRef|CfPyErrIfNull,
                           "vv", v, vi);
}
#endif

#define CALL_FLAG_VAR 1
#define CALL_FLAG_KW 2
static vinfo_t* psyco_ext_do_calls(PsycoObject* po, int opcode, int oparg,
				   vinfo_t** stack_top, int* stack_to_pop)
{
	int na = oparg & 0xff;
	int nk = (oparg>>8) & 0xff;
	int flags = (opcode - CALL_FUNCTION) & 3;
	int n = na + 2 * nk;
	vinfo_t** args;
	vinfo_t* vargs = NULL;
	vinfo_t* wdict = NULL;
	vinfo_t* result = NULL;
	
	if (flags & CALL_FLAG_VAR)
		n++;
	if (flags & CALL_FLAG_KW)
		n++;
	args = stack_top - n;
	*stack_to_pop = n+1;

	/* reminder: the stack layout, top-to-bottom, is:

	   - keyword dictionary (for '**kw' call syntax)
	   - arguments tuple    (for '*args' call syntax)
	   - kw value (nk-1)
	   - kw key   (nk-1)
	   ...
	   - kw value (0)
	   - kw key   (0)
	   - argument value (na-1)
	   ...
	   - argument value (0)
	   - callable object

	   We use 'n' as the current stack depth (not including
	   the callable object) while processing arguments.
	*/

	/* keyword arguments */
	if (nk == 0 && !(flags & CALL_FLAG_KW))
		wdict = psyco_vi_Zero();	/* no keyword arguments */
	else {
		int i;
		if (flags & CALL_FLAG_KW) {   /*  '**kw' call syntax */
			vinfo_t* w = args[--n];  /* pop keyword dictionary */
			/* check that it is a dictionary */
			switch (Psyco_VerifyType(po, w, &PyDict_Type)) {
			case true:   /* fine */
				break;
			case false:  /* not a dict */
				/* don't bother displaying the function name
				   which might not be known yet */
				PycException_SetString(po, PyExc_TypeError,
						       "argument after ** "
						       "must be a dictionary");
			default:  /* fall through */
				goto fail;
			}
			if (nk != 0) {
				/* make a copy of the dictionary;
				   the original one must not be modified */
				wdict = PsycoDict_Copy(po, w);
			}
			else {
				wdict = w;
				vinfo_incref(wdict);
			}
		}
		else
			wdict = PsycoDict_New(po);
		
		if (wdict == NULL)
			goto fail;

		/* update 'wdict' with the explicit keyword arguments */
		/* XXX do something closer to
		   update_keyword_args() in ceval.c, e.g.
		   check for duplicate keywords */
		for (i = na + 2*nk; i > na; ) {
			i -= 2;
			if (!psyco_generic_call(po, PyDict_SetItem,
					CfNoReturnValue|CfPyErrIfNonNull,
					"vvv", wdict, args[i], args[i+1]))
				break;
		}
	}

	/* non-keyword arguments */
	vargs = PsycoTuple_New(na, args);  /* virtual tuple */

	if (flags & CALL_FLAG_VAR) {
		vinfo_t* vtotal;
		vinfo_t* v = args[--n];  /* pop argument tuple */
		PyTypeObject* vt = Psyco_NeedType(po, v);
		if (vt == NULL)
			goto fail;
		if (!PyType_TypeCheck(vt, &PyTuple_Type)) {
			/* 'v' is not a tuple */
			if (!PsycoSequence_Check(vt)) {
				/* don't bother displaying the function name
				   which might not be known yet */
				PycException_SetFormat(po, PyExc_TypeError,
						       "argument after * "
						       "must be a sequence");
				goto fail;
			}
			v = PsycoSequence_Tuple(po, v);
			if (v == NULL)
				goto fail;
		}
		else
			vinfo_incref(v);
		
		vtotal = PsycoTuple_Concat(po, vargs, v);
		vinfo_decref(v, po);
		vinfo_decref(vargs, po);
		vargs = vtotal;
		if (vargs == NULL)
			goto fail;
	}

	result = PsycoObject_Call(po, args[-1], vargs, wdict);
	/* fall through */
 fail:
	vinfo_xdecref(vargs, po);
	vinfo_xdecref(wdict, po);
	return result;
}


/***************************************************************/
 /***   Run-time implementation of various opcodes            ***/

/* the code of the following functions is "copy-pasted" from ceval.c */

static PyObject* cimpl_load_global(PyObject* globals, PyObject* w)
{
	PyObject* x = PyDict_GetItem(globals, w);
	if (x == NULL) {
		x = PyDict_GetItem(psy_get_builtins(globals), w);
		if (x == NULL) {
			char* obj_str = PyString_AsString(w);
			if (obj_str)
				PyErr_Format(PyExc_NameError,
					     GLOBAL_NAME_ERROR_MSG,
					     obj_str);
			return NULL;
		}
	}
	Py_INCREF(x);
	return x;
}

static int cimpl_print_expr(PyObject* v)
{
	PyObject* x;
	PyObject* w = PySys_GetObject("displayhook");
	if (w == NULL) {
		PyErr_SetString(PyExc_RuntimeError,
				"lost sys.displayhook");
		return -1;
	}
	x = Py_BuildValue("(O)", v);
	if (x == NULL)
		return -1;
	w = PyEval_CallObject(w, x);
	Py_XDECREF(w);
	Py_DECREF(x);
	if (w == NULL)
		return -1;
	return 0;
}

static int cimpl_print_item_to(PyObject* v, PyObject* stream)
{
	/* XXX update to Python 2.3's implementation */
	if (stream == NULL || stream == Py_None) {
		stream = PySys_GetObject("stdout");
		if (stream == NULL) {
			PyErr_SetString(PyExc_RuntimeError,
					"lost sys.stdout");
			return -1;
		}
	}
	if (PyFile_SoftSpace(stream, 1))
		if (PyFile_WriteString(" ", stream))
			return -1;
	if (PyFile_WriteObject(v, stream, Py_PRINT_RAW))
		return -1;
	if (PyString_Check(v)) {
		/* move into writeobject() ? */
		char *s = PyString_AsString(v);
		int len = PyString_Size(v);
		if (len > 0 &&
		    isspace(Py_CHARMASK(s[len-1])) &&
		    s[len-1] != ' ')
			PyFile_SoftSpace(stream, 0);
	}
	return 0;
}

static int cimpl_print_newline_to(PyObject* stream)
{
	if (stream == NULL || stream == Py_None) {
		stream = PySys_GetObject("stdout");
		if (stream == NULL) {
			PyErr_SetString(PyExc_RuntimeError,
					"lost sys.stdout");
			return -1;
		}
	}
	if (PyFile_WriteString("\n", stream))
		return -1;
	PyFile_SoftSpace(stream, 0);
	return 0;
}

static int cimpl_unpack_iterable(PyObject* v, int argcnt, PyObject** sp)
{
#if HAVE_GENERATORS
	int i = 0;
	PyObject *it;  /* iter(v) */
	PyObject *w;

	extra_assert(v != NULL);

	it = PyObject_GetIter(v);
	if (it == NULL)
		goto Error;

	for (; i < argcnt; i++) {
		w = PyIter_Next(it);
		if (w == NULL) {
			/* Iterator done, via error or exhaustion. */
			if (!PyErr_Occurred()) {
				PyErr_Format(PyExc_ValueError,
					"need more than %d value%s to unpack",
					i, i == 1 ? "" : "s");
			}
			goto Error;
		}
		*sp++ = w;
	}

	/* We better have exhausted the iterator now. */
	w = PyIter_Next(it);
	if (w == NULL) {
		if (PyErr_Occurred())
			goto Error;
		Py_DECREF(it);
		return 0;
	}
	PyErr_SetString(PyExc_ValueError, "too many values to unpack");
	Py_DECREF(w);
	/* fall through */
Error:
	for (; i > 0; i--) {
		--sp;
		Py_DECREF(*sp);
	}
	Py_XDECREF(it);
	return -1;
        
#else /* !HAVE_GENERATORS */
        if (PySequence_Check(v)) {
		/* This is copied from Python 2.1's unpack_sequence() */
		int i;
		PyObject *w;

		for (i = 0; i < argcnt; i++) {
			if (! (w = PySequence_GetItem(v, i))) {
				if (PyErr_ExceptionMatches(PyExc_IndexError))
					PyErr_SetString(PyExc_ValueError,
					      "unpack sequence of wrong size");
				goto finally;
			}
			*sp++ = w;
		}
		/* we better get an IndexError now */
		if (PySequence_GetItem(v, i) == NULL) {
			if (PyErr_ExceptionMatches(PyExc_IndexError)) {
				PyErr_Clear();
				return 0;
			}
		     /* some other exception occurred. fall through to finally */
		}
		else
			PyErr_SetString(PyExc_ValueError,
					"unpack sequence of wrong size");
		/* fall through */
	   finally:
		for (; i > 0; i--) {
			--sp;
			Py_DECREF(*sp);
		}
		return -1;
	}
	else {
		PyErr_SetString(PyExc_TypeError,
				"unpack non-sequence");
		return -1;
	}
        
#endif /* HAVE_GENERATORS */
}

static int cimpl_unpack_list(PyObject* listobject, int argcnt, PyObject** sp)
{
	int i;
	extra_assert(PyList_Check(listobject));

	if (PyList_GET_SIZE(listobject) != argcnt) {
		PyErr_SetString(PyExc_ValueError, "unpack list of wrong size");
		return -1;
	}
	for (i=argcnt; i--; ) {
		PyObject* v = sp[i] = PyList_GET_ITEM(listobject, i);
		Py_INCREF(v);
	}
	return 0;
}

/* Logic for the raise statement.
   XXX a meta-implementation would be nice (e.g. to know at compile-time
   which exception class we got, so that we can know at compile-time which
   except: statements match) */
static void cimpl_do_raise(PyObject *type, PyObject *value, PyObject *tb)
{
	if (type == NULL) {
		/* Reraise */
		PyThreadState *tstate = PyThreadState_Get();
		type = tstate->exc_type == NULL ? Py_None : tstate->exc_type;
		value = tstate->exc_value;
		tb = tstate->exc_traceback;
	}

	/* unlike the ceval.c version, this never consumes a reference
	   to its arguments (psyco_generic_call cannot handle this). */
	Py_XINCREF(type);
	Py_XINCREF(value);
	Py_XINCREF(tb);

	/* We support the following forms of raise:
	   raise <class>, <classinstance>
	   raise <class>, <argument tuple>
	   raise <class>, None
	   raise <class>, <argument>
	   raise <classinstance>, None
	   raise <string>, <object>
	   raise <string>, None

	   An omitted second argument is the same as None.

	   In addition, raise <tuple>, <anything> is the same as
	   raising the tuple's first item (and it better have one!);
	   this rule is applied recursively.

	   Finally, an optional third argument can be supplied, which
	   gives the traceback to be substituted (useful when
	   re-raising an exception after examining it).  */

	/* First, check the traceback argument, replacing None with
	   NULL. */
	if (tb == Py_None) {
		Py_DECREF(tb);
		tb = NULL;
	}
	else if (tb != NULL && !PyTraceBack_Check(tb)) {
		PyErr_SetString(PyExc_TypeError,
			   "raise: arg 3 must be a traceback or None");
		goto raise_error;
	}

	/* Next, replace a missing value with None */
	if (value == NULL) {
		value = Py_None;
		Py_INCREF(value);
	}

	/* Next, repeatedly, replace a tuple exception with its first item */
	while (PyTuple_Check(type) && PyTuple_Size(type) > 0) {
		PyObject *tmp = type;
		type = PyTuple_GET_ITEM(type, 0);
		Py_INCREF(type);
		Py_DECREF(tmp);
	}

	if (PyString_Check(type))
		;

	else if (PyClass_Check(type))
		PyErr_NormalizeException(&type, &value, &tb);

	else if (PyInstance_Check(type)) {
		/* Raising an instance.  The value should be a dummy. */
		if (value != Py_None) {
			PyErr_SetString(PyExc_TypeError,
			  "instance exception may not have a separate value");
			goto raise_error;
		}
		else {
			/* Normalize to raise <class>, <instance> */
			Py_DECREF(value);
			value = type;
			type = (PyObject*) ((PyInstanceObject*)type)->in_class;
			Py_INCREF(type);
		}
	}
	else {
		/* Not something you can raise.  You get an exception
		   anyway, just not what you specified :-) */
		PyErr_Format(PyExc_TypeError,
			     "exceptions must be strings, classes, or "
			     "instances, not %s", type->ob_type->tp_name);
		goto raise_error;
	}
	PyErr_Restore(type, value, tb);
	/*if (tb == NULL)
		return WHY_EXCEPTION;
	else
		return WHY_RERAISE;*/
	return;
	
 raise_error:
	Py_XDECREF(value);
	Py_XDECREF(type);
	Py_XDECREF(tb);
	/*return WHY_EXCEPTION;*/
}

#if HAVE_PYTHON_SUPPORT
# define PyClass_NewInGlobals(g, bases, dict, name) \
			PyClass_New(bases, dict, name)
#else  /* !HAVE_PYTHON_SUPPORT */
static PyObject*
PyClass_NewInGlobals(PyObject* g,  /* globals */
		     PyObject *bases, PyObject *dict, PyObject *name)
{
	/* workaround for the use of PyEval_GetGlobals() in
	   PyClass_New() */
	if (PyDict_GetItemString(dict, "__module__") == NULL) {
		PyObject *modname = PyDict_GetItemString(g, "__name__");
		if (modname != NULL) {
			if (PyDict_SetItemString(dict, "__module__", modname)<0)
				return NULL;
		}
	}
	return PyClass_New(bases, dict, name);
}
#endif  /* !HAVE_PYTHON_SUPPORT */

/* copied from ceval.c where it is private */
/* added a workaround for PyClass_New(), which normally uses
   PyEval_GetGlobals() to get the globals. */
static PyObject*
cimpl_build_class(PyObject* g,  /* globals */
		  PyObject *methods, PyObject *bases, PyObject *name)
{
#if NEW_STYLE_TYPES
	/* Python 2.2 version */
	PyObject *metaclass = NULL, *result, *base;

	if (PyDict_Check(methods))
		metaclass = PyDict_GetItemString(methods, "__metaclass__");
	if (metaclass != NULL)
		Py_INCREF(metaclass);
	else if (PyTuple_Check(bases) && PyTuple_GET_SIZE(bases) > 0) {
		base = PyTuple_GET_ITEM(bases, 0);
		metaclass = PyObject_GetAttrString(base, "__class__");
		if (metaclass == NULL) {
			PyErr_Clear();
			metaclass = (PyObject *)base->ob_type;
			Py_INCREF(metaclass);
		}
	}
	else {
		/*if (g != NULL && PyDict_Check(g))*/
			metaclass = PyDict_GetItemString(g, "__metaclass__");
		if (metaclass == NULL)
			metaclass = (PyObject *) &PyClass_Type;
		Py_INCREF(metaclass);
	}
	if (metaclass == (PyObject *) &PyClass_Type) {
		/* special case, workaround for PyClass_New() */
		result = PyClass_NewInGlobals(g, bases, methods, name);
	}
	else
		result = PyObject_CallFunction(metaclass, "OOO",
					       name, bases, methods);
	Py_DECREF(metaclass);
	return result;
	
#else
	/* Python 2.1 version */
	int i, n;
	n = PyTuple_Size(bases);
	for (i = 0; i < n; i++) {
		PyObject *base = PyTuple_GET_ITEM(bases, i);
		if (!PyClass_Check(base)) {
			/* Call the base's *type*, if it is callable.
			   This code is a hook for Donald Beaudry's
			   and Jim Fulton's type extensions.  In
			   unextended Python it will never be triggered
			   since its types are not callable.
			   Ditto: call the bases's *class*, if it has
			   one.  This makes the same thing possible
			   without writing C code.  A true meta-object
			   protocol! */
			PyObject *basetype = (PyObject *)base->ob_type;
			PyObject *callable = NULL;
			if (PyCallable_Check(basetype))
				callable = basetype;
			else
				callable = PyObject_GetAttrString(
					base, "__class__");
			if (callable) {
				PyObject *args;
				PyObject *newclass = NULL;
				args = Py_BuildValue(
					"(OOO)", name, bases, methods);
				if (args != NULL) {
					newclass = PyEval_CallObject(
						callable, args);
					Py_DECREF(args);
				}
				if (callable != basetype) {
					Py_DECREF(callable);
				}
				return newclass;
			}
			PyErr_SetString(PyExc_TypeError,
				"base is not a class object");
			return NULL;
		}
	}
	return PyClass_NewInGlobals(g, bases, methods, name);
#endif
}

static PyObject* cimpl_import_name(PyObject* globals, PyObject* name,
				   PyObject* fromlist)
{
	PyObject* w;
	PyObject* x = PyDict_GetItemString(psy_get_builtins(globals),
					   "__import__");
	if (x == NULL) {
		PyErr_SetString(PyExc_ImportError,
				"__import__ not found");
		return NULL;
	}
	w = Py_BuildValue("(OOOO)",
			  name,
			  globals,
			  Py_None,
			  fromlist);
	if (w == NULL)
		return NULL;
	x = PyEval_CallObject(x, w);
	Py_DECREF(w);
	return x;
}


 /***************************************************************/
/***                         Main loop                         ***/
 /***************************************************************/

static code_t* exit_function(PsycoObject* po)
{
	vinfo_t** locals_plus;
        vinfo_t** pp;
	Source retsource;
	
	/* clear the stack and the locals */
	locals_plus = LOC_LOCALS_PLUS;
	for (pp = po->vlocals.items + po->vlocals.count; --pp >= locals_plus; )
		if (*pp != NULL) {
			vinfo_decref(*pp, po);
			*pp = NULL;
		}
	
	if (PycException_Is(po, &EReturn)) {
		/* load the return value */
		vinfo_t* retval = po->pr.val;
		extra_assert(retval != NULL);
		if (!compute_vinfo(retval, po)) return NULL;
		/* return a new reference */
		consume_reference(po, retval);
		if (retval->array->count > 0) {
			array_delete(retval->array, po);
			retval->array = NullArray;
		}
		retsource = retval->source;
	}
	else {
		/* &ERtPython is the case where the code that raised
		   the Python exception is already written, e.g. when
		   we called a function in the Python interpreter which
		   raised an exception. */
		if (!PycException_Is(po, &ERtPython)) {
			/* In the other cases (virtual exception),
			   compute and raise the exception now. */
			char args[4];
			args[3] = 0;
			if (po->pr.tb == NULL)
				args[2] = 'l';
			else {
				args[2] = 'v';
				consume_reference(po, po->pr.tb);
			}
			if (po->pr.val == NULL)
				args[1] = 'l';
			else {
				args[1] = 'v';
				consume_reference(po, po->pr.val);
			}
			args[0] = 'v';
			consume_reference(po, po->pr.exc);
			if (psyco_generic_call(po, PyErr_Restore,
					       CfNoReturnValue, args,
					       po->pr.exc, po->pr.val,
					       po->pr.tb) == NULL)
				return NULL;
		}
		clear_pseudo_exception(po);
		retsource = CompileTime_NewSk(&psyco_skZero); /*to return NULL */
	}
	
	return psyco_finish_return(po, retsource);
}


/***************************************************************/
 /***   the main loop of the interpreter/compiler.            ***/

DEFINEFN
code_t* psyco_pycompiler_mainloop(PsycoObject* po)
{
  /* 'stack_a' is the Python stack base pointer */
  vinfo_t** stack_a = po->vlocals.items + po->pr.stack_base;
  int opcode=0;	/* Current opcode */
  int oparg=0;	/* Current opcode argument, if any */
  code_t* code1;
  
  /* save and restore the current Python exception throughout compilation */
  PyObject *old_py_exc, *old_py_val, *old_py_tb;
  PyErr_Fetch(&old_py_exc, &old_py_val, &old_py_tb);

  if ((psyco_mp_flags(po->pr.merge_points) & MP_FLAGS_HAS_EXCEPT) != 0 &&
      (LOC_CONTINUATION->array == NullArray)) {
    /* for functions that have "try: except:" blocks, we reserve some room
       in the stack to store the previous tstate->exc_xxx if an exception
       is raised and catched. This space plays the role of Python's
       PyFrameObject::f_exc_xxx fields. */
    psyco_emit_header(po, 3);
  }
  
  while (po->pr.next_instr != -1)
    {
      /* 'co' is the code object we are interpreting/compiling */
      PyCodeObject* co = po->pr.co;
      unsigned char* bytecode = (unsigned char*) PyString_AS_STRING(co->co_code);
      vinfo_t *u, *v,	/* temporary objects    */
	      *w, *x;	/* popped off the stack */
      condition_code_t cc;
      /* 'next_instr' is the position in the byte-code of the next instr */
      int next_instr = po->pr.next_instr;
      mergepoint_t* mp = psyco_next_merge_point(po->pr.merge_points,
                                                next_instr+1);

      /* trace each code block entry point */
      TRACE_EXECUTION_NOERR("ENTER_MAINLOOP");
      
      /* main loop */
      while (1) {

	extra_assert(!PycException_Occurred(po));
	psyco_assert_coherent(po);  /* this test is expensive */

	/* save 'next_instr' */
        SAVE_NEXT_INSTR(next_instr);  /* could be optimized, not needed in the
                                         case of an opcode that cannot set
                                         run-time conditions */
        
	opcode = NEXTOP();
	if (HAS_ARG(opcode))
		oparg = NEXTARG();
  dispatch_opcode:

	/* Main switch on opcode */
	
	/* !!IMPORTANT!!
	   No operation with side-effects must be performed before we are
	   sure the compilation of the current instruction succeeded!
	   Indeed, if the compilation is interrupted by a C++ exception,
	   it will be restarted by re-running psyco_pycompiler_mainloop()
	   and this will restart the compilation of the instruction from
	   the beginning. In particular, use POP() with care. Better use
	   TOP() to get the arguments off the stack and call POP() at the
	   end when you are sure everything when fine. */

	/* All opcodes found here must also be listed in mergepoints.c.
	   Python code objects using a bytecode instruction not listed
	   in mergepoints.c are never Psyco-ified. */

	switch (opcode) {

	/* case STOP_CODE: this is an error! */

	case POP_TOP:
		POP_DECREF();
		goto fine;

	case ROT_TWO:
		POP(v);
		POP(w);
		PUSH(v);
		PUSH(w);
		goto fine;

	case ROT_THREE:
		POP(v);
		POP(w);
		POP(x);
		PUSH(v);
		PUSH(x);
		PUSH(w);
		goto fine;

	case ROT_FOUR:
		POP(u);
		POP(v);
		POP(w);
		POP(x);
		PUSH(u);
		PUSH(x);
		PUSH(w);
		PUSH(v);
		goto fine;

	case DUP_TOP:
		v = TOP();
		vinfo_incref(v);
		PUSH(v);
		goto fine;

	case DUP_TOPX:
	{
		int i;
		for (i=0; i<oparg; i++) {
			x = NTOP(oparg);
			vinfo_incref(x);
			PUSH(x);
		}
		goto fine;
	}

        case UNARY_POSITIVE:
		x = PsycoNumber_Positive(po, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;
		
        case UNARY_NEGATIVE:
		x = PsycoNumber_Negative(po, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;

	case UNARY_NOT:
		v = PsycoObject_IsTrue(po, TOP());
		cc = integer_NON_NULL(po, v);
		if (cc == CC_ERROR)
			break;
		
                cc = INVERT_CC(cc);
		/* turns 'cc' into a Python integer object, 0 or 1 */
		x = PsycoInt_FROM_LONG(psyco_vinfo_condition(po, cc));
		POP_DECREF();
		PUSH(x);  /* consumes ref on 'x' */
		goto fine;

	case UNARY_CONVERT:
		x = PsycoObject_Repr(po, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;

	case UNARY_INVERT:
		x = PsycoNumber_Invert(po, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;

	case BINARY_POWER:
		u = psyco_vi_None();
		x = PsycoNumber_Power(po, NTOP(2), NTOP(1), u);
		vinfo_decref(u, po);
		if (x == NULL)
			break;
		POP_DECREF();
		POP_DECREF();
		PUSH(x);
		goto fine;

#define BINARY_OPCODE(opcode, psycofn)			\
	case opcode:					\
		x = psycofn (po, NTOP(2), NTOP(1));	\
		if (x == NULL)				\
			break;				\
		POP_DECREF();				\
		POP_DECREF();				\
		PUSH(x);				\
		goto fine
		
	BINARY_OPCODE(BINARY_MULTIPLY, PsycoNumber_Multiply);
	BINARY_OPCODE(BINARY_DIVIDE, PsycoNumber_Divide);
	BINARY_OPCODE(BINARY_MODULO, PsycoNumber_Remainder);
	BINARY_OPCODE(BINARY_ADD, PsycoNumber_Add);
	BINARY_OPCODE(BINARY_SUBTRACT, PsycoNumber_Subtract);
	BINARY_OPCODE(BINARY_SUBSCR, PsycoObject_GetItem);
	BINARY_OPCODE(BINARY_LSHIFT, PsycoNumber_Lshift);
	BINARY_OPCODE(BINARY_RSHIFT, PsycoNumber_Rshift);
	BINARY_OPCODE(BINARY_AND, PsycoNumber_And);
	BINARY_OPCODE(BINARY_XOR, PsycoNumber_Xor);
	BINARY_OPCODE(BINARY_OR, PsycoNumber_Or);

#ifdef BINARY_FLOOR_DIVIDE
        BINARY_OPCODE(BINARY_FLOOR_DIVIDE, PsycoNumber_FloorDivide);
        BINARY_OPCODE(BINARY_TRUE_DIVIDE, PsycoNumber_TrueDivide);
        BINARY_OPCODE(INPLACE_FLOOR_DIVIDE, PsycoNumber_InPlaceFloorDivide);
        BINARY_OPCODE(INPLACE_TRUE_DIVIDE, PsycoNumber_InPlaceTrueDivide);
#endif

	case INPLACE_POWER:
		u = psyco_vi_None();
		x = PsycoNumber_InPlacePower(po, NTOP(2), NTOP(1), u);
		vinfo_decref(u, po);
		if (x == NULL)
			break;
		POP_DECREF();
		POP_DECREF();
		PUSH(x);
		goto fine;
	
	BINARY_OPCODE(INPLACE_MULTIPLY, PsycoNumber_InPlaceMultiply);
	BINARY_OPCODE(INPLACE_DIVIDE, PsycoNumber_InPlaceDivide);
	BINARY_OPCODE(INPLACE_MODULO, PsycoNumber_InPlaceRemainder);
	BINARY_OPCODE(INPLACE_ADD, PsycoNumber_InPlaceAdd);
	BINARY_OPCODE(INPLACE_SUBTRACT, PsycoNumber_InPlaceSubtract);
	BINARY_OPCODE(INPLACE_LSHIFT, PsycoNumber_InPlaceLshift);
	BINARY_OPCODE(INPLACE_RSHIFT, PsycoNumber_InPlaceRshift);
	BINARY_OPCODE(INPLACE_AND, PsycoNumber_InPlaceAnd);
	BINARY_OPCODE(INPLACE_XOR, PsycoNumber_InPlaceXor);
	BINARY_OPCODE(INPLACE_OR, PsycoNumber_InPlaceOr);

	case SLICE+0:
	case SLICE+1:
	case SLICE+2:
	case SLICE+3:
	{
		int from_top = 1;
		if ((opcode-SLICE) & 2) {
			w = NTOP(from_top);
			from_top++;
		}
		else
			w = NULL;
		if ((opcode-SLICE) & 1) {
			v = NTOP(from_top);
			from_top++;
		}
		else
			v = NULL;
		u = NTOP(from_top);
		x = psyco_apply_slice(po, u, v, w);
		if (x == NULL)
			break;
		POP_DECREF();
		if (v != NULL) POP_DECREF();
		if (w != NULL) POP_DECREF();
		PUSH(x);
		goto fine;
	}

	case STORE_SLICE+0:
	case STORE_SLICE+1:
	case STORE_SLICE+2:
	case STORE_SLICE+3:
	{
		int from_top = 1;
		if ((opcode-STORE_SLICE) & 2) {
			w = NTOP(from_top);
			from_top++;
		}
		else
			w = NULL;
		if ((opcode-STORE_SLICE) & 1) {
			v = NTOP(from_top);
			from_top++;
		}
		else
			v = NULL;
		u = NTOP(from_top);
		if (!psyco_assign_slice(po, u, v, w, NTOP(from_top+1)))
			break;
		POP_DECREF();
		POP_DECREF();
		if (v != NULL) POP_DECREF();
		if (w != NULL) POP_DECREF();
		goto fine;
	}
	
	case DELETE_SLICE+0:
	case DELETE_SLICE+1:
	case DELETE_SLICE+2:
	case DELETE_SLICE+3:
	{
		int from_top = 1;
		if ((opcode-DELETE_SLICE) & 2) {
			w = NTOP(from_top);
			from_top++;
		}
		else
			w = NULL;
		if ((opcode-DELETE_SLICE) & 1) {
			v = NTOP(from_top);
			from_top++;
		}
		else
			v = NULL;
		u = NTOP(from_top);
		if (!psyco_assign_slice(po, u, v, w, NULL))
			break;
		POP_DECREF();
		if (v != NULL) POP_DECREF();
		if (w != NULL) POP_DECREF();
		goto fine;
	}

	case STORE_SUBSCR:
		w = NTOP(1);
		v = NTOP(2);
		u = NTOP(3);
		/* v[w] = u */
		if (!PsycoObject_SetItem(po, v, w, u))
			break;
		POP_DECREF();
		POP_DECREF();
		POP_DECREF();
		goto fine;

	case DELETE_SUBSCR:
		w = NTOP(1);
		v = NTOP(2);
		/* del v[w] */
		if (!PsycoObject_SetItem(po, v, w, NULL))
			break;
		POP_DECREF();
		POP_DECREF();
		goto fine;

	case PRINT_EXPR:
		if (!psyco_generic_call(po, cimpl_print_expr,
					CfNoReturnValue|CfPyErrIfNonNull,
					"v", TOP()))
			break;
		POP_DECREF();
		goto fine;

	case PRINT_ITEM:
		if (!psyco_generic_call(po, cimpl_print_item_to,
					CfNoReturnValue|CfPyErrIfNonNull,
					"vl", TOP(), 0))
			break;
		POP_DECREF();
		goto fine;
		
	case PRINT_ITEM_TO:
		if (!psyco_generic_call(po, cimpl_print_item_to,
					CfNoReturnValue|CfPyErrIfNonNull,
					"vv", NTOP(2), TOP()))
			break;
		POP_DECREF();
		POP_DECREF();
		goto fine;
		
	case PRINT_NEWLINE:
		if (!psyco_generic_call(po, cimpl_print_newline_to,
					CfNoReturnValue|CfPyErrIfNonNull,
					"l", 0))
			break;
		goto fine;
		
	case PRINT_NEWLINE_TO:
		if (!psyco_generic_call(po, cimpl_print_newline_to,
					CfNoReturnValue|CfPyErrIfNonNull,
					"v", TOP()))
			break;
		POP_DECREF();
		goto fine;

	case BREAK_LOOP:
		PycException_Raise(po, vinfo_new(VirtualTime_New(&EBreak)),
				   NULL);
		break;

	case CONTINUE_LOOP:
		PycException_Raise(po, vinfo_new(VirtualTime_New(&EContinue)),
				   vinfo_new(CompileTime_New(oparg)));
		break;

	case RAISE_VARARGS:
		u = v = w = x = psyco_vi_Zero();
		switch (oparg) {
		case 3:
			u = NTOP(oparg-2); /* traceback */
			/* Fallthrough */
		case 2:
			v = NTOP(oparg-1); /* value */
			/* Fallthrough */
		case 1:
			w = NTOP(oparg-0); /* exc */
		case 0: /* Fallthrough */
			psyco_generic_call(po, cimpl_do_raise, CfPyErrAlways,
					   "vvv", w, v, u);
                        break;
		default:
			PycException_SetString(po, PyExc_SystemError,
					       "bad RAISE_VARARGS oparg");
		}
		vinfo_decref(x, po);
		break;

        /*MISSING_OPCODE(LOAD_LOCALS);*/

	case RETURN_VALUE:
		POP(v);
		PycException_Raise(po, vinfo_new(VirtualTime_New(&EReturn)), v);
		break;

#ifdef RETURN_NONE
                /* This opcode was temporarily added to the Python 2.3 CVS.
                   Now it is gone again, so the following will probably
                   never be compiled into Psyco anyway. */
	case RETURN_NONE:
		v = psyco_vi_None();
		PycException_Raise(po, vinfo_new(VirtualTime_New(&EReturn)), v);
		break;
#endif

	/*MISSING_OPCODE(YIELD_VALUE);*/
	/*MISSING_OPCODE(EXEC_STMT);*/

	case POP_BLOCK:
	{
		PyTryBlock *b = block_pop(po);
		while (STACK_LEVEL() > b->b_level) {
			POP(v);
			vinfo_decref(v, po);
		}
		goto fine;
	}

	case END_FINALLY:
		/* First make extra sure no exception is pending */
		if (PycException_Occurred(po))
			Py_FatalError("psyco: undetected exception "
				      "in END_FINALLY");
		POP(v);
		if (psyco_knowntobe(v, (long) Py_None)) {
			/* 'None' on the stack, it is the end of a finally
			   block with no exception raised */
			vinfo_decref(v, po);
			goto fine;
		}
		if (Psyco_KnownType(v) == &PyTuple_Type &&
		    PsycoTuple_Load(v) == 3) {
			/* 'v' is a 3- tuple. As no real Python exception is
			   a tuple object we are sure it comes from a
			   finally block (see the SETUP_FINALLY stack unwind
			   code at the end of pycompiler.c) */
			/* Re-raise the pseudo exception. This will re-raise
			   exceptions like EReturn as well. */
			po->pr.exc = PsycoTuple_GET_ITEM(v, 0);
			PsycoTuple_GET_ITEM(v, 0) = NULL;
			po->pr.val = PsycoTuple_GET_ITEM(v, 1);
			PsycoTuple_GET_ITEM(v, 1) = NULL;
			po->pr.tb = PsycoTuple_GET_ITEM(v, 2);
			PsycoTuple_GET_ITEM(v, 2) = NULL;
			vinfo_decref(v, po);
			break;
		}
		else {
			/* end of an EXCEPT block, re-raise the exception
			   stored in the stack. As in Python, no extra
			   traceback is recorded, but instead of the
			   WHY_RERAISE trick we simply record no traceback
			   if the opcode is END_FINALLY. */
			po->pr.exc = v;
			POP(po->pr.val);
			POP(po->pr.tb);
			break;
		}

	case BUILD_CLASS:
		x = psyco_generic_call(po, cimpl_build_class,
				       CfReturnRef|CfPyErrIfNull,
				       "vvvv", LOC_GLOBALS,
						NTOP(1), NTOP(2), NTOP(3));
		if (x == NULL)
			break;
		POP_DECREF();
		POP_DECREF();
		POP_DECREF();
		PUSH(x);
		goto fine;

	/*MISSING_OPCODE(STORE_NAME); -- only used in module's code objects?
	  MISSING_OPCODE(DELETE_NAME);*/

	case UNPACK_SEQUENCE:
	{
		int i;
		void* cimpl_unpack;
		PyTypeObject* vtp;
		
		v = TOP();
		vtp = Psyco_NeedType(po, v);
		if (vtp == NULL)
			break;

		if (PyType_TypeCheck(vtp, &PyTuple_Type)) {
			/* shortcut: is this a virtual tuple?
			             of the correct length? */
			if (PsycoTuple_Load(v) != oparg) {

				/* No, fall back to the default path:
				   load the size, compare it with oparg,
				   and if they match proceed by loading the
				   tuple item by item into the stack. */
				vinfo_t* vsize;
				vsize = psyco_get_const(po, v, FIX_size);
				if (vsize == NULL)
					break;

				/* check the size */
				cc = integer_cmp_i(po, vsize, oparg, Py_NE);
				if (cc == CC_ERROR)
					break;
				if (runtime_condition_f(po, cc)) {
					PycException_SetString(po,
						PyExc_ValueError,
						"unpack tuple of wrong size");
					break;
				}

				/* make sure the tuple data is loaded */
				for (i=oparg; i--; ) {
					w = psyco_get_nth_const(po, v,
								TUPLE_ob_item,
								i);
					if (w == NULL)
						break;
				}
				if (i >= 0)
					break;
			}
			/* copy the tuple items into the stack */
			POP(v);
			for (i=oparg; i--; ) {
				w = PsycoTuple_GET_ITEM(v, i);
                                vinfo_incref(w);
                                PUSH(w);
                                /* in case the tuple is freed while its items
                                   are still in use: */
                                need_reference(po, w);
                        }
			vinfo_decref(v, po);
                        goto fine;

		}

		if (PyType_TypeCheck(vtp, &PyList_Type))
			cimpl_unpack = cimpl_unpack_list;
		else
			cimpl_unpack = cimpl_unpack_iterable;
		
		{
			vinfo_array_t* array = array_new(oparg);
			if (!psyco_generic_call(po, cimpl_unpack,
					      CfNoReturnValue|CfPyErrIfNonNull,
						"vlA", v, oparg, array)) {
				array_release(array);
				break;
			}
			POP_DECREF();
			for (i=oparg; i--; )
				PUSH(array->items[i]);
			array_release(array);
			goto fine;
		}
	}

	case STORE_ATTR:
	{
		bool ok;
		w = vinfo_new(CompileTime_New(((long) GETNAMEV(oparg))));
		ok = PsycoObject_SetAttr(po, TOP(), w, NTOP(2));  /* v.w = u */
		vinfo_decref(w, po);
		if (!ok)
			break;
		POP_DECREF();
		POP_DECREF();
		goto fine;
	}

	case DELETE_ATTR:
	{
		bool ok;
		w = vinfo_new(CompileTime_New(((long) GETNAMEV(oparg))));
		ok = PsycoObject_SetAttr(po, TOP(), w, NULL);   /* del v.w */
		vinfo_decref(w, po);
		if (!ok)
			break;
		POP_DECREF();
		goto fine;
	}

	case STORE_GLOBAL:
	{
		PyObject* w = GETNAMEV(oparg);
		mark_varying(po, w);
		if (!psyco_generic_call(po, PyDict_SetItem,
					CfNoReturnValue|CfPyErrIfNonNull,
					"vlv", LOC_GLOBALS, w, TOP()))
			break;
		POP_DECREF();
		goto fine;
	}

	case DELETE_GLOBAL:
	{
		PyObject* w = GETNAMEV(oparg);
		mark_varying(po, w);
		if (runtime_NON_NULL_f(po, psyco_generic_call(po, PyDict_DelItem,
					CfReturnNormal, "vl", LOC_GLOBALS, w))) {
			PycException_SetFormat(po, PyExc_NameError,
					       GLOBAL_NAME_ERROR_MSG,
					       PyString_AsString(w));
			break;
		}
		goto fine;
	}
	
	case LOAD_CONST:
		/* reference borrowed from the code object */
		v = vinfo_new(CompileTime_New((long) GETCONST(oparg)));
		PUSH(v);
		goto fine;

	/*MISSING_OPCODE(LOAD_NAME);*/

	case LOAD_GLOBAL:
	{
		PyObject* namev = GETNAMEV(oparg);
                PyObject* value;
		if (is_compiletime(LOC_GLOBALS->source) &&
		    (po->pr.changing_globals == NULL ||
		     PyDict_GetItem(po->pr.changing_globals, namev) == NULL)) {
			/* Common case: fast global loading */
			value = load_global(po, namev, next_instr);
			if (value != NULL) {
				/* success */
				v = vinfo_new(CompileTime_NewSk(
					sk_new((long) value, SkFlagPyObj)));
				PUSH(v);
				goto fine;
			}
			/* else { variable not found at all } */
		}
		/* else { Globals dict is unknown at compile-time,
		   	  or the global has been marked as varying } */
		v = psyco_generic_call(po, cimpl_load_global,
				       CfReturnRef|CfPyErrIfNull,
				       "vl", LOC_GLOBALS, namev);
		if (v == NULL)
			break;
		PUSH(v);
		goto fine;
	}

	case LOAD_FAST:
		x = GETLOCAL(oparg);
		/* a local variable can only be unbound if its
		   value is known to be (PyObject*)NULL. A run-time
		   or virtual value is always non-NULL. */
		if (psyco_knowntobe(x, 0)) {
			PyObject* namev;
			namev = PyTuple_GetItem(co->co_varnames, oparg);
			PycException_SetFormat(po, PyExc_UnboundLocalError,
					       UNBOUNDLOCAL_ERROR_MSG,
					       PyString_AsString(namev));
			break;
		}
		vinfo_incref(x);
		PUSH(x);
		goto fine;

	case STORE_FAST:
		POP(v);
		SETLOCAL(oparg, v);
		goto fine;

	case DELETE_FAST:
		x = GETLOCAL(oparg);
		/* a local variable can only be unbound if its
		   value is known to be (PyObject*)NULL. A run-time
		   or virtual value is always non-NULL. */
		if (psyco_knowntobe(x, 0)) {
			PyObject* namev;
			namev = PyTuple_GetItem(co->co_varnames, oparg);
			PycException_SetFormat(po, PyExc_UnboundLocalError,
					       UNBOUNDLOCAL_ERROR_MSG,
					       PyString_AsString(namev));
			break;
		}

		u = psyco_vi_Zero();
		SETLOCAL(oparg, u);
		goto fine;

	/*MISSING_OPCODE(LOAD_CLOSURE);
	  MISSING_OPCODE(LOAD_DEREF);
	  MISSING_OPCODE(STORE_DEREF);*/

        case BUILD_TUPLE:
		v = PsycoTuple_New(oparg, STACK_POINTER() - oparg);
		while (oparg--)
			POP_DECREF();
		PUSH(v);
		goto fine;

	case BUILD_LIST:
		v = PsycoList_New(po, oparg, STACK_POINTER() - oparg);
		if (v == NULL)
			break;
		while (oparg--)
			POP_DECREF();
		PUSH(v);
		goto fine;

	case BUILD_MAP:
		v = PsycoDict_New(po);
		if (v == NULL)
			break;
		PUSH(v);
		goto fine;

	case LOAD_ATTR:
		w = vinfo_new(CompileTime_New(((long) GETNAMEV(oparg))));
		x = PsycoObject_GetAttr(po, TOP(), w);  /* v.w */
		vinfo_decref(w, po);
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;

	case COMPARE_OP:
	{
		condition_code_t cc = CC_ERROR;
		w = TOP();
		v = NTOP(2);
		switch (oparg) {
			
		case PyCmp_IS:      /* pointer comparison */
			cc = integer_cmp(po, v, w, Py_EQ);
			break;
			
		case PyCmp_IS_NOT:  /* pointer comparison */
			cc = integer_cmp(po, v, w, Py_NE);
			break;

		case PyCmp_IN:
		case PyCmp_NOT_IN:
			x = PsycoSequence_Contains(po, w, v);
			cc = integer_NON_NULL(po, x);
			if (oparg == PyCmp_NOT_IN && cc != CC_ERROR)
				cc = INVERT_CC(cc);
			break;

		case PyCmp_EXC_MATCH:
			x = psyco_generic_call(po, PyErr_GivenExceptionMatches,
					       CfPure|CfReturnNormal,
					       "vv", v, w);
			cc = integer_NON_NULL(po, x);
			break;

		default:
			x = PsycoObject_RichCompare(po, v, w, oparg);
			goto compare_done;
		}

		if (cc == CC_ERROR)
			break;
		
		/* turns 'cc' into a virtual Python integer object, 0 or 1 */
		x = PsycoInt_FROM_LONG(psyco_vinfo_condition(po, cc));
		
	compare_done:
		if (x == NULL)
			break;
		POP_DECREF();
		POP_DECREF();
		PUSH(x);  /* consumes ref on x */
		goto fine;
	}

	case IMPORT_NAME:
	{
		PyObject* w = GETNAMEV(oparg);

		x = psyco_generic_call(po, cimpl_import_name,
				       CfReturnRef|CfPyErrIfNull,
				       "vlv", LOC_GLOBALS, w, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;
	}
	
	/*MISSING_OPCODE(IMPORT_STAR);*/

	case IMPORT_FROM:
	{
		PyObject* name = GETNAMEV(oparg);
		w = vinfo_new(CompileTime_New(((long) name)));
		v = TOP();
		x = PsycoObject_GetAttr(po, v, w);
		vinfo_decref(w, po);
		if (x == NULL) {
			extra_assert(PycException_Occurred(po));
			
			/* catch PyExc_AttributeError */
			v = PycException_Matches(po, PyExc_AttributeError);
			if (runtime_NON_NULL_t(po, v) == true) {
				PycException_SetFormat(po, PyExc_ImportError,
					"cannot import name %.230s",
					PyString_AsString(name));
			}
			break;
		}
		PUSH(x);
		goto fine;
	}

	case JUMP_FORWARD:
		JUMPBY(oparg);
		mp = psyco_next_merge_point(po->pr.merge_points, next_instr);
		goto fine;

	case JUMP_IF_TRUE:
	case JUMP_IF_FALSE:
		/* This code is very different from the original
		   interpreter's, because we generally do not know the
		   outcome of PyObject_IsTrue(). In the case of JUMP_IF_xxx
		   we must be prepared to have to compile the two possible
		   paths. */
		cc = integer_NON_NULL(po, PsycoObject_IsTrue(po, TOP()));
		if (cc == CC_ERROR)
			break;
		if (opcode == JUMP_IF_FALSE)
			cc = INVERT_CC(cc);
		if (cc < CC_TOTAL) {
			/* compile the beginning of the "if true" path */
			int current_instr = next_instr;
			JUMPBY(oparg);
			SAVE_NEXT_INSTR(next_instr);
			psyco_compile_cond(po,
				psyco_exact_merge_point(po->pr.merge_points,
							next_instr),
                                           cc);
			next_instr = current_instr;
		}
		else if (cc == CC_ALWAYS_TRUE) {
			JUMPBY(oparg);   /* always jump */
			mp = psyco_next_merge_point(po->pr.merge_points,
						    next_instr);
                }
		else
			;                  /* never jump */
		goto fine;

	case JUMP_ABSOLUTE:
		JUMPTO(oparg);
		mp = psyco_next_merge_point(po->pr.merge_points, next_instr);
		goto fine;

#ifdef GET_ITER
	case GET_ITER:
		x = PsycoObject_GetIter(po, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;
#endif

#ifdef FOR_ITER
	case FOR_ITER:
		v = PsycoIter_Next(po, TOP());
		if (v != NULL) {
			/* iterator not exhausted */
			PUSH(v);
		}
		else {
			extra_assert(PycException_Occurred(po));
			
			/* catch PyExc_StopIteration */
			v = PycException_Matches(po, PyExc_StopIteration);
			if (runtime_NON_NULL_t(po, v) == true) {
				/* iterator ended normally */
				PycException_Clear(po);
				POP_DECREF();
				JUMPBY(oparg);
				mp = psyco_next_merge_point(po->pr.merge_points,
							    next_instr);
			}
			else
				break;   /* any other exception, or error in
					    runtime_NON_NULL_t() */
		}
		goto fine;
#endif

#if OLD_STYLE_LOOP
	case FOR_LOOP:
		w = TOP();
		v = NTOP(2);
		u = psyco_loop_subscript(po, v, w);
		if (u != NULL) {
			x = PsycoInt_AS_LONG(po, w);
			x = integer_add_i(po, x, 1, true);
			if (x == NULL)
				break;
			POP_DECREF();
			PUSH(PsycoInt_FROM_LONG(x));
			PUSH(u);
		}
		else {
			extra_assert(PycException_Occurred(po));
			v = PycException_Matches(po, PyExc_IndexError);
			if (runtime_NON_NULL_t(po, v) == true) {
				PycException_Clear(po);
                                POP_DECREF();
                                POP_DECREF();
				JUMPBY(oparg);
				mp = psyco_next_merge_point(po->pr.merge_points,
							    next_instr);
			}
			else
				break;   /* any other exception, or error in
					    runtime_NON_NULL_t() */
		}
		goto fine;
#endif

	case SETUP_LOOP:
	case SETUP_EXCEPT:
	case SETUP_FINALLY:
		block_setup(po, opcode, INSTR_OFFSET() + oparg,
			    STACK_LEVEL());
		goto fine;

#ifdef SET_LINENO
	case SET_LINENO:
		/* trace machine code execution at each SET_LINENO opcode
		       XXX re-implement this feature for Python 2.3, which
		       no longer uses the SET_LINENO opcode */
		TRACE_EXECUTION_NOERR("SET_LINENO");
		goto fine;
#endif

	case CALL_FUNCTION:
	case CALL_FUNCTION_VAR:
	case CALL_FUNCTION_KW:
	case CALL_FUNCTION_VAR_KW:
	{
		int i, stack_to_pop;
		x = psyco_ext_do_calls(po, opcode, oparg, STACK_POINTER(),
				       &stack_to_pop);
		if (x == NULL)
			break;

		/* clean up the stack (remove args and func) */
		for (i=stack_to_pop; i--; )
			POP_DECREF();
		PUSH(x);
		goto fine;
	}

	/*MISSING_OPCODE(MAKE_CLOSURE);*/

	case MAKE_FUNCTION:
		if (oparg > 0)
			v = PsycoTuple_New(oparg, STACK_POINTER() - oparg - 1);
		else
			v = NULL;
		x = PsycoFunction_New(po, TOP(), LOC_GLOBALS, v);
		vinfo_xdecref(v, po);
		if (x == NULL)
			break;
		
		/* clean up the stack (remove args and func) */
		while (oparg-- >= 0)
			POP_DECREF();
		PUSH(x);
		goto fine;

	case BUILD_SLICE:
		if (oparg == 3) {
			w = NTOP(1);
			v = NTOP(2);
			u = NTOP(3);
		}
		else {
			w = psyco_vi_Zero();
			v = NTOP(1);
			u = NTOP(2);
		}
		x = psyco_generic_call(po, PySlice_New,
				       CfReturnRef|CfPyErrIfNull,
				       "vvv", u, v, w);
		if (oparg != 3)
			vinfo_decref(w, po);
		if (x == NULL)
			break;
		if (oparg == 3)
			POP_DECREF();	/* w */
		POP_DECREF();		/* v */
		POP_DECREF();		/* u */
		PUSH(x);
		goto fine;

	case EXTENDED_ARG:
		opcode = NEXTOP();
		oparg = oparg<<16 | NEXTARG();
		goto dispatch_opcode;

	default:
		fprintf(stderr,
			"XXX opcode: %d\n",
			opcode);
		Py_FatalError("unknown opcode");

	}  /* switch (opcode) */

	/* Exit if an error occurred */
	if (PycException_Occurred(po))
		break;

  fine:
	extra_assert(!PycException_Occurred(po));
	extra_assert(mp == psyco_next_merge_point(po->pr.merge_points,
                                                  next_instr));
	
	/* are we running out of space in the current code buffer? */
	if ((po->codelimit - po->code) < BUFFER_MARGIN) {
		if (is_respawning(po)) {
			/* when respawning, just forget everything we
			   wrote so far and come back to the beginning
			   again */
			PsycoObject_EmergencyCodeRoom(po);
		}
		else {
			/* normal case: save the current position, and
			   stop compilation. When this point is reached
			   at run-time, compilation will go on in a
			   new buffer. */
			SAVE_NEXT_INSTR(next_instr);
			if (mp->bytecode_position != next_instr)
				mp = NULL;
			code1 = psyco_compile(po, mp, false);
			goto finished;
		}
	}
	
	/* mark merge points via a call to psyco_compile() */
	if (mp->bytecode_position == next_instr) {
		extra_assert(!is_respawning(po));
#if OLD_STYLE_LOOP
		/* hack to un-promote the loop index from compile-time
		   (loaded by "LOAD_CONST 0") to run-time. This is not
		   theoretically needed but it is a huge win. */
		if (bytecode[next_instr] == FOR_LOOP &&
		    is_compiletime(TOP()->source)) {
			psyco_unfix(po, TOP());
		}
#endif
		SAVE_NEXT_INSTR(next_instr);
                /* simplify the po->vlocals array */
		if (!psyco_limit_nested_weight(po, &po->vlocals, NWI_NORMAL,
					       NESTED_WEIGHT_END))
			break;
		psyco_delete_unused_vars(po, &mp->entries);
		code1 = psyco_compile(po, mp, true);
		if (code1 != NULL)
			goto finished;
                mp++;
		/* trace execution at each of the <snapshot>s the execution
		   goes through */
		TRACE_EXECUTION_NOERR("SNAPSHOT");
	}
      }  /* end of the main loop, exit if exception */

      psyco_assert_coherent(po);

      /* check for the 'promotion' pseudo-exception.
	 This is the only case in which the instruction
	 causing the exception is restarted. */
      if (is_virtualtime(po->pr.exc->source) &&
	  psyco_vsource_is_promotion(po->pr.exc->source)) {
	      c_promotion_t* promotion = (c_promotion_t*) \
		      VirtualTime_Get(po->pr.exc->source);
	      vinfo_t* promote_me = po->pr.val;
	      /* NOTE: we assume that 'promote_me' is a member of the
		 'po->vlocals' arrays, so that we can safely DECREF it
		 now without actually releasing it. If this assumption
		 is false, the psyco_finish_xxx() calls below will give
		 unexpected results. */
	      extra_assert(array_contains(&po->vlocals, promote_me));
	      clear_pseudo_exception(po);
#if USE_RUNTIME_SWITCHES
	      if (promotion->fs == NULL)
#endif
		      code1 = psyco_finish_promotion(po,
						     promote_me,
						     promotion->kflags);
#if USE_RUNTIME_SWITCHES
	      else
		      code1 = psyco_finish_fixed_switch(po,
							promote_me,
							promotion->kflags,
							promotion->fs);
#endif
	      goto finished;
      }
      
      /* At this point, we got a real pseudo-exception. */

      if (PycException_IsPython(po) && opcode != END_FINALLY)
        {
          /* log traceback info for real Python exceptions,
             unless re-raised by END_FINALLY */
          int lasti = po->pr.next_instr - 1;
          if (HAS_ARG(opcode))
            lasti -= 2;
          PsycoTraceBack_Here(po, lasti);
        }
      
      /* Unwind the Python stack until we find a handler for
	 the (pseudo) exception. You will recognize here
	 ceval.c's stack unwinding code. */
      
      while (po->pr.iblock > 0) {
	PyTryBlock *b = block_pop(po);
	      
	if (b->b_type == SETUP_LOOP && PycException_Is(po, &EContinue)) {
		/* For a continue inside a try block,
		   don't pop the block for the loop. */
		int next_instr;
		block_setup(po, b->b_type, b->b_handler, b->b_level);
		JUMPTO(CompileTime_Get(po->pr.val->source)->value);
		clear_pseudo_exception(po);
		SAVE_NEXT_INSTR(next_instr);
		break;
	}
	
	/* clear the stack up to b->b_level */
	while (STACK_LEVEL() > b->b_level) {
		POP_DECREF();   /* no NULLs here */
	}
	
	if (b->b_type == SETUP_LOOP && PycException_Is(po, &EBreak)) {
		int next_instr;
		clear_pseudo_exception(po);
		JUMPTO(b->b_handler);
		SAVE_NEXT_INSTR(next_instr);
		break;
	}
	
	if (b->b_type == SETUP_FINALLY) {
		/* SETUP_FINALLY always pushes on the stack either a
		   single compile-time None object or three objects
		   (exc, value, traceback). Unlike Python, the latter
		   might represent a pseudo-exception like EReturn. */
		int next_instr;
		vinfo_t** stack_a = po->vlocals.items + po->pr.stack_base;
		vinfo_t* exc_info = PsycoTuple_New(3, NULL);
		PycException_Fetch(po);
		PsycoTuple_GET_ITEM(exc_info, 0) = po->pr.exc;
		PsycoTuple_GET_ITEM(exc_info, 1) = po->pr.val;
		PsycoTuple_GET_ITEM(exc_info, 2) = po->pr.tb;
		po->pr.exc = NULL;
		po->pr.val = NULL;
		po->pr.tb  = NULL;
		PUSH(exc_info);
		JUMPTO(b->b_handler);
		SAVE_NEXT_INSTR(next_instr);
		break;
	}
	
	if (b->b_type == SETUP_EXCEPT && PycException_IsPython(po)) {
		/* SETUP_EXCEPT pushes three objects individually as in
		   ceval.c, as needed for bytecode compatibility. See tricks
		   in END_FINALLY to distinguish between the end of a FINALLY
		   and the end of an EXCEPT block. */
		int next_instr;
		vinfo_t** stack_a = po->vlocals.items + po->pr.stack_base;
		while (!PycException_FetchNormalize(po)) {
			/* got an exception while initializing the EXCEPT
			   block... Consider this new exception as overriding
			   the previous one, so that we just re-enter the same
			   EXCEPT block. */
			/* XXX check that this empty loop cannot be endless */
		}
		extra_assert(po->pr.val != NULL);
		extra_assert(po->pr.tb != NULL);
		PUSH(po->pr.tb);     po->pr.tb  = NULL;
		PUSH(po->pr.val);    po->pr.val = NULL;
		PUSH(po->pr.exc);    po->pr.exc = NULL;
		JUMPTO(b->b_handler);
		SAVE_NEXT_INSTR(next_instr);
		break;
	}
      } /* end of unwind stack */
      
      /* End the function if we still have a (pseudo) exception */
      if (PycException_Occurred(po)) {
	      /* at the end of the function we set next_instr to -1
		 because the actual position has no longer any importance */
	      po->pr.next_instr = -1;
      }
    }

  	
  /* function return (either by RETURN or by exception). */
  while ((code1 = exit_function(po)) == NULL) {
	  /* If LOC_EXCEPTION is still set after exit_function(), it
	     means an exception was raised while handling the functn
	     return... In this case, we do a function return again,
	     this time with the newly raised exception. XXX make sure
	     this loop cannot be endless */
  }

 finished:
#if ALL_CHECKS
  if (PyErr_Occurred()) {
	  fprintf(stderr, "psyco: unexpected Python exception during compilation:\n");
	  PyErr_WriteUnraisable(Py_None);
  }
#endif
  PyErr_Restore(old_py_exc, old_py_val, old_py_tb);
  return code1;
}
