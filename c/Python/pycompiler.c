#include "pycompiler.h"
#include "../pycencoding.h"
#include "../processor.h"
#include "../dispatcher.h"
#include "../mergepoints.h"
#include "../codemanager.h"

#include "pbltinmodule.h"

#include "../Objects/pobject.h"
#include "../Objects/pabstract.h"
#include "../Objects/pclassobject.h"
#include "../Objects/pdescrobject.h"
#include "../Objects/pdictobject.h"
#include "../Objects/pfuncobject.h"
#include "../Objects/pintobject.h"
#include "../Objects/piterobject.h"
#include "../Objects/plistobject.h"
#include "../Objects/plongobject.h"
#include "../Objects/pmethodobject.h"
#include "../Objects/pstringobject.h"
#include "../Objects/pstructmember.h"
#include "../Objects/psycofuncobject.h"
#include "../Objects/ptupleobject.h"

#include <eval.h>
#include <opcode.h>


#define KNOWN_VAR(type, varname, loc)   \
    type varname = (type)(KNOWN_SOURCE(loc)->value)


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
void Psyco_DefineMetaModule(char* modulename, meta_impl_t* def)
{
	/* call Psyco_DefineMeta() for all functions of module 'modulename'
	   found in the NULL-terminated array 'def'. */
	PyObject* module;
	PyObject* fobj;
	char* name;
	void* f;

	/* XXX should not load the modules now. Not loaded modules should
	   be registered in some hook and Psyco_DefineMetaModule() should
	   be automatically called again if and when the module is called. */
	module = PyImport_ImportModule(modulename);
	if (module == NULL) {
		PyErr_Clear();
		debug_printf(("psyco: note: module %s not found\n",
			      modulename));
		return;
	}
	
	for (; def->mm_descr != NULL; def++) {
		name = def->mm_descr->cd_name;
		fobj = PyObject_GetAttrString(module, name);
		if (fobj == NULL) {
			PyErr_Clear();
			debug_printf(("psyco: note: %s.%s not found\n",
				      modulename, name));
			continue;
		}
		f = NULL;
		switch (def->mm_descr->cd_flags) {

#if NEW_STYLE_TYPES
		case CD_META_TP_NEW:
			if (PyType_Check(fobj))
				f = ((PyTypeObject*) fobj)->tp_new;
			break;
#endif
		default:
			if (PyCFunction_Check(fobj) &&
			    PyCFunction_GET_FLAGS(fobj)==def->mm_descr->cd_flags)
				f = PyCFunction_GET_FUNCTION(fobj);
		}

		def->mm_descr->cd_function = f;
		if (f != NULL) {
			Psyco_DefineMeta(f, def->mm_psyco_fn);
		}
		else {
			debug_printf(("psyco: note: %s.%s not implemented "
				      "as expected\n",
				      modulename, name));
		}
		Py_DECREF(fobj);
	}
	Py_DECREF(module);
}


#define BAD_FLAGS(fl)  if ((flags & CfReturnMask) == (fl))			\
			    Py_FatalError("generic_call_check(): CfPyErrXxx "	\
					  "flags incompatible with "		\
					  "CfReturnXxx flags")
#define FORGET_REF     if (has_rtref(vi->source))			\
			    vi->source = remove_rtref(vi->source)

DEFINEFN
vinfo_t* generic_call_check(PsycoObject* po, int flags, vinfo_t* vi)
{
	condition_code_t cc;
	
	switch (flags & CfPyErrMask) {

	case CfPyErrIfNull:   /* a return == 0 (or NULL) means an error */
		BAD_FLAGS(CfNoReturnValue);
		if ((flags & CfReturnMask) == CfReturnFlag)
			cc = INVERT_CC((condition_code_t) vi);
		else
			cc = integer_cmp_i(po, vi, 0, Py_EQ);
		break;

	case CfPyErrIfNonNull:   /* a return != 0 means an error */
		BAD_FLAGS(CfNoReturnValue);
		if ((flags & CfReturnMask) == CfReturnFlag)
			cc = (condition_code_t) vi;
		else
			cc = integer_cmp_i(po, vi, 0, Py_NE);
		break;

	case CfPyErrIfNeg:   /* a return < 0 means an error */
		BAD_FLAGS(CfReturnFlag);
		BAD_FLAGS(CfNoReturnValue);
		cc = integer_cmp_i(po, vi, 0, Py_LT);
		break;

	case CfPyErrIfMinus1:     /* only -1 means an error */
		BAD_FLAGS(CfReturnFlag);
		BAD_FLAGS(CfNoReturnValue);
		cc = integer_cmp_i(po, vi, -1, Py_EQ);
		break;

	case CfPyErrCheck:    /* always check with PyErr_Occurred() */
		cc = PsycoErr_Occurred(po);
		break;

	case CfPyErrCheckMinus1:   /* use PyErr_Occurred() if return is -1 */
		BAD_FLAGS(CfReturnFlag);
		BAD_FLAGS(CfNoReturnValue);
		cc = integer_cmp_i(po, vi, -1, Py_NE);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_t(po, cc))
			return vi;   /* result is not -1, ok */
		cc = PsycoErr_Occurred(po);
		break;

        case CfPyErrCheckNeg:   /* use PyErr_Occurred() if return is < 0 */
		BAD_FLAGS(CfReturnFlag);
		BAD_FLAGS(CfNoReturnValue);
		cc = integer_cmp_i(po, vi, 0, Py_GE);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_t(po, cc))
			return vi;   /* result is >= 0, ok */
		cc = PsycoErr_Occurred(po);
		break;

	case CfPyErrNotImplemented:   /* test for a Py_NotImplemented result */
		BAD_FLAGS(CfReturnFlag);
		BAD_FLAGS(CfNoReturnValue);
		cc = integer_cmp_i(po, vi, (long) Py_NotImplemented, Py_EQ);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_f(po, cc)) {
			/* result is Py_NotImplemented */
			vinfo_decref(vi, po);
			vinfo_incref(psyco_viNotImplemented);
			return psyco_viNotImplemented;
		}
		cc = integer_cmp_i(po, vi, 0, Py_EQ);
		break;

	case CfPyErrIterNext:    /* specially for tp_iternext slots */
		BAD_FLAGS(CfReturnFlag);
		BAD_FLAGS(CfNoReturnValue);
		cc = integer_cmp_i(po, vi, 0, Py_NE);
		if (cc == CC_ERROR)
			goto Error;
		if (runtime_condition_t(po, cc))
			return vi;   /* result is not 0, ok */
		
		FORGET_REF;  /* NULL result */
		vinfo_decref(vi, po);
		cc = PsycoErr_Occurred(po);
		if (cc == CC_ERROR || runtime_condition_f(po, cc))
			goto PythonError;  /* PyErr_Occurred() returns true */

		/* NULL result with no error set; it is the end of the
		   iteration. Raise a pseudo PyErr_StopIteration. */
		vinfo_incref(psyco_viNone);
		PycException_SetVInfo(po, PyExc_StopIteration, psyco_viNone);
		return NULL;

	default:
		return vi;
	}
	
	if (cc == CC_ERROR || runtime_condition_f(po, cc)) {

	   Error:
		switch (flags & CfReturnMask) {
		case CfReturnRef:   /* in case of error, 'vi' is not a real
				       reference, so forget it */
			FORGET_REF;
			/* fall through */
		case CfReturnNormal:
			vinfo_decref(vi, po);
			vi = NULL;
			break;
		case CfReturnFlag:
			vi = (vinfo_t*) CC_ERROR;
			break;
		default:
			vi = NULL;
		}
		/* We have detected that a Python exception must be set at
		   this point. */
	   PythonError:
		PycException_Raise(po, vinfo_new(VirtualTime_New(&ERtPython)),
				   NULL);
	}
	return vi;
}
#undef BAD_FLAGS
#undef FORGET_REF

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

DEFINEFN
vinfo_t* Psyco_Meta4x(PsycoObject* po, void* c_function, int flags,
			     const char* arguments,
			     long a1, long a2, long a3, long a4)
{
	void* psyco_fn = Psyco_Lookup(c_function);
	if (psyco_fn == NULL)
		return psyco_generic_call(po, c_function, flags, arguments,
					   a1, a2, a3, a4);
	else
		return ((vinfo_t*(*)(PsycoObject*, long, long,
				     long, long))(psyco_fn))
			(po, a1, a2, a3, a4);
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

DEFINEFN
condition_code_t PycException_Matches(PsycoObject* po, PyObject* e)
{
	condition_code_t cc;
	if (PycException_Is(po, &ERtPython)) {
		/* Exception raised by Python, emit a call to
		   PyErr_ExceptionMatches() */
		cc = psyco_flag_call(po, PyErr_ExceptionMatches,
				     CfReturnFlag, "l", (long) e);
	}
	else if (PycException_IsPython(po)) {
		/* Exception virtually set, that is, present in the PsycoObject
		   but not actually set at run-time by Python's PyErr_SetXxx */
		cc = psyco_flag_call(po, PyErr_GivenExceptionMatches,
				     CfPure | CfReturnFlag,
				     "vl", po->pr.exc, (long) e);
	}
	else {   /* pseudo exceptions don't match real Python ones */
		cc = CC_ALWAYS_FALSE;
	}
	return cc;
}

inline void clear_pseudo_exception(PsycoObject* po)
{
	extra_assert(PycException_Occurred(po));
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
	PyErr_Fetch(&exc, &val, &tb);
	extra_assert(exc != NULL);

	PycException_Raise(po,
			  vinfo_new(CompileTime_NewSk(sk_new((long) exc,
                                                             SkFlagPyObj))),
			  vinfo_new(CompileTime_NewSk(sk_new((long) val,
                                                             SkFlagPyObj))));
	Py_XDECREF(tb);  /* XXX implement tracebacks */
}

static void cimpl_pyerr_fetch(PyObject* target[])
{
	PyObject* tb;
	PyErr_Fetch(target+0, target+1, &tb);
	Py_XDECREF(tb);  /* XXX implement tracebacks */
	/* XXX call set_exc_info() */
	if (target[0] == NULL) {
		target[0] = Py_None;
		Py_INCREF(Py_None);
	}
	if (target[1] == NULL) {
		target[1] = Py_None;
		Py_INCREF(Py_None);
	}
}

static void cimpl_pyerr_fetch_and_normalize(PyObject* target[])
{
	PyObject* tb;
	PyErr_Fetch(target+0, target+1, &tb);
	PyErr_NormalizeException(target+0, target+1, &tb);
	Py_XDECREF(tb);  /* XXX implement tracebacks */
	/* XXX call set_exc_info() */
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
		vinfo_array_t* array = array_new(2);
		psyco_generic_call(po, cimpl_pyerr_fetch,
				   CfNoReturnValue, "A", array);
		clear_pseudo_exception(po);
		PycException_Raise(po, array->items[0], array->items[1]);
		array_release(array);
	}
}

inline bool PycException_FetchNormalize(PsycoObject* po)
{
	if (PycException_Is(po, &ERtPython)) {
		vinfo_array_t* array = array_new(2);
		if (psyco_generic_call(po, cimpl_pyerr_fetch_and_normalize,
				       CfNoReturnValue, "A", array) == NULL)
			return false;
		clear_pseudo_exception(po);
		PycException_Raise(po, array->items[0], array->items[1]);
		array_release(array);
	}
	else {
		/* normalize the exception */
		vinfo_t* exc;
		vinfo_t* val;
		vinfo_t* tb;
		exc = make_runtime_copy(po, po->pr.exc);
		if (exc == NULL) return false;
		val = make_runtime_copy(po, po->pr.val);
		if (val == NULL) return false;
		tb = make_runtime_copy(po, psyco_viZero);
		if (tb == NULL) return false;
		if (psyco_generic_call(po, PyErr_NormalizeException,
				CfNoReturnValue, "rrr", exc, val, tb) == NULL)
			return false;
		vinfo_decref(tb, po);
		PycException_Raise(po, exc, val);
	}
	return true;
}



 /***************************************************************/
/***                      Initialization                       ***/
 /***************************************************************/


DEFINEVAR vinfo_t* psyco_viNone;    /* known value 'Py_None' */
DEFINEVAR vinfo_t* psyco_viZero;    /* known value 0 */
DEFINEVAR vinfo_t* psyco_viOne;     /* known value 1 */
DEFINEVAR vinfo_t* psyco_viNotImplemented;

static PyObject* s_builtin_object;   /* intern string '__builtins__' */

DEFINEFN
void psyco_pycompiler_init()
{
	s_builtin_object = PyString_InternFromString("__builtins__");
	
	psyco_viNone = vinfo_new(CompileTime_NewSk(sk_new((long) Py_None,
							  SkFlagFixed)));
	psyco_viZero = vinfo_new(CompileTime_NewSk(sk_new(0, SkFlagFixed)));
	psyco_viOne  = vinfo_new(CompileTime_NewSk(sk_new(1, SkFlagFixed)));
	psyco_viNotImplemented = vinfo_new(CompileTime_NewSk(sk_new
				((long) Py_NotImplemented, SkFlagFixed)));

	ERtPython = psyco_vsource_not_important;
	EReturn   = psyco_vsource_not_important;
	EContinue = psyco_vsource_not_important;
	EBreak    = psyco_vsource_not_important;

        psy_object_init();
        psy_abstract_init();
        psy_classobject_init();
        psy_descrobject_init();
        psy_dictobject_init();
        psy_funcobject_init();
        psy_intobject_init();
        psy_iterobject_init();
        psy_listobject_init();
        psy_longobject_init();
        psy_methodobject_init();
        psy_stringobject_init();
        psy_structmember_init();
        psy_psycofuncobject_init();
        psy_tupleobject_init();

	psy_bltinmodule_init();
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

/*  Definition hacks for Python version <2.2b1,
 *  as detected by the missing macro PyString_CheckExact.
 *   (this assumes that the structure of the dictionaries is
 *    exported if and only if NEW_STYLE_TYPES)
 */

#if !NEW_STYLE_TYPES
typedef struct {
	long me_hash;      /* cached hash code of me_key */
	PyObject *me_key;
	PyObject *me_value;
#ifdef USE_CACHE_ALIGNED
	long	aligner;
#endif
} PyDictEntry;
typedef struct _dictobject PyDictObject;
struct _dictobject {
	PyObject_HEAD
	int ma_fill;  /* # Active + # Dummy */
	int ma_used;  /* # Active */
	int ma_mask;
	PyDictEntry *ma_table;
	PyDictEntry *(*ma_lookup)(PyDictObject *mp, PyObject *key, long hash);
	/* not needed: PyDictEntry ma_smalltable[PyDict_MINSIZE]; */
};
#endif  /* !NEW_STYLE_TYPES */


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
	vinfo_t* v;
	vinfo_t** stack_a = po->vlocals.items + po->pr.stack_base;
        mergepoint_t* mp;
	KNOWN_VAR(PyDictObject*, globals, LOC_GLOBALS);
	PyDictEntry* ep;
	void* dict_subscript;
	code_t* target;

	/* first check that the value really changed; it could merely
	   have moved in the dictionary table (reallocations etc.) */
	ep = (globals->ma_lookup)(globals, key,
				  ((PyStringObject*) key)->ob_shash);
	
	if (ep->me_value == cg->previousvalue) {
		int index = ep - globals->ma_table;
		/* no real change; update the original macro code
		   and that's it */
		DICT_ITEM_IFCHANGED(code, globals, index, key, ep->me_value,
				    cg->self->codeptr);
		return code;  /* execution continues after the macro code */
	}

	/* un-promote the global variable to run-time and write code that
	   calls dict_subscript(). Warning: we assume this will not overflow
	   the (relatively large) code previously written by the macro
	   DICT_ITEM_IFCHANGED at the same place.

	   We call dict_subscript() instead of PyDict_GetItem() for the extra
	   reference; indeed, we cannot be sure how long we will need the
	   object so we must own a reference. dict_subscript() could be
	   inlined too (but be careful about the overflow problem above!)

           XXX what occurs if the global has been deleted ?
	*/
	SAVE_REGS_FN_CALLS;
	CALL_SET_ARG_IMMED((long) key,         1, 2);
	CALL_SET_ARG_IMMED((long) globals,     0, 2);

	v = new_rtvinfo(po, REG_FUNCTIONS_RETURN, true);
	PUSH(v);
	/* 'v' is now run-time, recompile */
        mp = psyco_exact_merge_point(po->pr.merge_points, po->pr.next_instr);
	target = psyco_compile_code(po, mp)->codeptr;
	/* XXX don't know what to do with the reference returned by
	   XXX psyco_compile_code() */

	dict_subscript = PyDict_Type.tp_as_mapping->mp_subscript;
	CALL_C_FUNCTION_AND_JUMP(dict_subscript,  2,  target);
  
  /* cannot Py_DECREF(cg->self) because the current function is returning into
     that code now, but any time later is fine: use the trash of codemanager.h */
	psyco_trash_object((PyObject*) cg->self);
	return cg->originalmacrocode;
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
	extra_assert(((PyStringObject*) key)->ob_sinterned != NULL);
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
		
		result = ep->me_value;
		
		/* if the object is changed later we will jump to
		   a proxy which we prepare now */
		onchangebuf = psyco_new_code_buffer(NULL, NULL);
                if (onchangebuf == NULL)
			OUT_OF_MEMORY();
		po = PsycoObject_Duplicate(po);

                /* The global object has not been PUSHed on the Python stack
                   yet. However, if we enter do_changed_global() later and
                   figure out the value changed, do_changed_global() will
                   emit code that loads the run-time value of the global
                   and emulate the LOAD_GLOBAL opcode by PUSHing a run-time
                   vinfo_t. This means the recompilation triggered by
                   do_changed_global() must restart *after* the LOAD_GLOBAL
                   instruction and not *at* it, so we store the next
                   instruction position in the new 'po': */
                SAVE_NEXT_INSTR(next_instr);

		code = onchangebuf->codeptr;
		TEMP_SAVE_REGS_FN_CALLS;
		po->code = code;
		cg = (changed_global_t*) psyco_jump_proxy(po,
						   &do_changed_global, 1, 1);
		cg->self = onchangebuf;
		cg->po = po;
		cg->varname = key;
                cg->previousvalue = result;    Py_INCREF(result);
		cg->originalmacrocode = po1->code;
		SHRINK_CODE_BUFFER(onchangebuf,
                                   (code_t*)(cg+1) - onchangebuf->codeptr,
                                   "load_global");

		/* go on in the main code sequence */
		po = po1;
		/* write code that quickly checks that the same
		   object is still in place in the dictionary */
		code = po->code;
		DICT_ITEM_IFCHANGED(code, globals, index, key, result,
				    onchangebuf->codeptr);
		po->code = code;
                psyco_dump_code_buffers();

		Py_INCREF(result);
		return result;
	}
	else {
		/* no such global variable, get the builtins */
		if (po->pr.f_builtins == NULL) {
			PyObject* builtins;
				/* code copied from frameobject.c */
				/* XXX we currently consider the absence
				   of builtins to be a fatal error */
			builtins = PyDict_GetItem((PyObject*) globals,
						  s_builtin_object);
			assert(builtins != NULL);
			if (PyModule_Check(builtins)) {
				builtins = PyModule_GetDict(builtins);
				assert(builtins != NULL);
			}
			assert(PyDict_Check(builtins));
			po->pr.f_builtins = builtins;
		}
		result = PyDict_GetItem(po->pr.f_builtins, key);
		
		/* found at all? */
		if (result != NULL)
			Py_INCREF(result);
		else
			PycException_SetFormat(po, PyExc_NameError,
					       GLOBAL_NAME_ERROR_MSG,
					       PyString_AS_STRING(key));
	}
	return result;
}


/***************************************************************/
 /***   Slicing                                               ***/

static vinfo_t* _PsycoEval_SliceIndex(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* result;
	switch (Psyco_TypeSwitch(po, v, &psyfs_int_long)) {

	case 0:   /* PyInt_Type */
		result = PsycoInt_AS_LONG(po, v);
		vinfo_incref(result);
		break;

	case 1:   /* PyLong_Type */
		result = PsycoLong_AsLong(po, v);
		if (result == NULL) {
			vinfo_t* vi_zero;
			PyObject* long_zero;
			condition_code_t cc;
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
			cc = PsycoObject_RichCompareBool(po, v, vi_zero, Py_GT);
			vinfo_decref(vi_zero, po);
			if (cc == CC_ERROR)
				return NULL;
			if (runtime_condition_t(po, cc))
				x = INT_MAX;
			else
				x = 0;
			result = vinfo_new(CompileTime_New(x));
		}
		break;

	default:
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
			ilow = psyco_viZero;
			vinfo_incref(ilow);
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
			ilow = psyco_viZero;
			vinfo_incref(ilow);
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


/***************************************************************/
 /***   Run-time implementation of various opcodes            ***/

/* the code of the following functions is "copy-pasted" from ceval.c */

static int cimpl_print_item_to(PyObject* v, PyObject* stream)
{
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
	int i = 0;
	PyObject *it;  /* iter(v) */
	PyObject *w;

	assert(v != NULL);

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


 /***************************************************************/
/***                         Main loop                         ***/
 /***************************************************************/

static code_t* exit_function(PsycoObject* po)
{
	vinfo_t** locals_plus;
        vinfo_t** pp;
	NonVirtualSource retsource;
	
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
		retsource = vinfo_compute(retval, po);
		if (retsource == SOURCE_ERROR) return NULL;
		if (!eat_reference(retval)) {
			/* return a new reference */
			psyco_incref_v(po, retval);
		}
	}
	else {
		/* &ERtPython is the case where the code that raised
		   the Python exception is already written, e.g. when
		   we called a function in the Python interpreter which
		   raised an exception. */
		if (!PycException_Is(po, &ERtPython)) {
			/* In the other cases (virtual exception),
			   compute and raise the exception now. */
			if (psyco_generic_call(po, PyErr_SetObject,
					       CfNoReturnValue, "vv",
					       po->pr.exc, po->pr.val) == NULL)
				return NULL;
			clear_pseudo_exception(po);
		}
		retsource = psyco_viZero->source;  /* to return NULL */
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
  code_t* code1;
  
  /* save and restore the current Python exception throughout compilation */
  PyObject *old_py_exc, *old_py_val, *old_py_tb;
  PyErr_Fetch(&old_py_exc, &old_py_val, &old_py_tb);

  while (po->pr.next_instr != -1)
    {
      /* 'co' is the code object we are interpreting/compiling */
      PyCodeObject* co = po->pr.co;
      unsigned char* bytecode = (unsigned char*) PyString_AS_STRING(co->co_code);
      int opcode=0;	/* Current opcode */
      int oparg=0;	/* Current opcode argument, if any */
      vinfo_t *u, *v,	/* temporary objects    */
	      *w, *x;	/* popped off the stack */
      condition_code_t cc;
      /* 'next_instr' is the position in the byte-code of the next instr */
      int next_instr = po->pr.next_instr;
      mergepoint_t* mp = psyco_next_merge_point(po->pr.merge_points,
                                                next_instr+1);

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
		cc = PsycoObject_IsTrue(po, TOP());
		if (cc == CC_ERROR)
			break;
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
		x = PsycoNumber_Power(po, NTOP(2), NTOP(1), psyco_viNone);
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

	/*#ifdef BINARY_FLOOR_DIVIDE
	 MISSING_OPCODE(BINARY_FLOOR_DIVIDE);
	 MISSING_OPCODE(BINARY_TRUE_DIVIDE);
	 MISSING_OPCODE(INPLACE_FLOOR_DIVIDE);
	 MISSING_OPCODE(INPLACE_TRUE_DIVIDE);
	#endif*/

	BINARY_OPCODE(BINARY_MODULO, PsycoNumber_Remainder);
	BINARY_OPCODE(BINARY_ADD, PsycoNumber_Add);
	BINARY_OPCODE(BINARY_SUBTRACT, PsycoNumber_Subtract);
	BINARY_OPCODE(BINARY_SUBSCR, PsycoObject_GetItem);
	BINARY_OPCODE(BINARY_LSHIFT, PsycoNumber_Lshift);
	BINARY_OPCODE(BINARY_RSHIFT, PsycoNumber_Rshift);
	BINARY_OPCODE(BINARY_AND, PsycoNumber_And);
	BINARY_OPCODE(BINARY_XOR, PsycoNumber_Xor);
	BINARY_OPCODE(BINARY_OR, PsycoNumber_Or);
	
	case INPLACE_POWER:
		x = PsycoNumber_InPlacePower(po, NTOP(2), NTOP(1), psyco_viNone);
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
	
		/*MISSING_OPCODE(PRINT_EXPR);*/

	case PRINT_ITEM:
		if (psyco_flag_call(po, cimpl_print_item_to,
				    CfReturnFlag|CfPyErrIfNonNull,
				    "vl", TOP(), 0) == CC_ERROR)
			break;
		POP_DECREF();
		goto fine;
		
	case PRINT_ITEM_TO:
		if (psyco_flag_call(po, cimpl_print_item_to,
				    CfReturnFlag|CfPyErrIfNonNull,
				    "vv", NTOP(2), TOP()) == CC_ERROR)
			break;
		POP_DECREF();
		POP_DECREF();
		goto fine;
		
	case PRINT_NEWLINE:
		psyco_flag_call(po, cimpl_print_newline_to,
				CfReturnFlag|CfPyErrIfNonNull,
				"l", 0);
		break;
		
	case PRINT_NEWLINE_TO:
		if (psyco_flag_call(po, cimpl_print_newline_to,
				    CfReturnFlag|CfPyErrIfNonNull,
				    "v", TOP()) == CC_ERROR)
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

	/*MISSING_OPCODE(RAISE_VARARGS);
	  MISSING_OPCODE(LOAD_LOCALS);*/

	case RETURN_VALUE:
		POP(v);
		PycException_Raise(po, vinfo_new(VirtualTime_New(&EReturn)), v);
		break;

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
		POP(v);
		if (is_compiletime(v->source) &&
		    CompileTime_Get(v->source)->value == (long) Py_None) {
			/* 'None' on the stack, it is the end of a finally
			   block with no exception raised */
			vinfo_decref(v, po);
			goto fine;
		}
		if (v->array->count > OB_TYPE &&
		    is_compiletime(v->array->items[OB_TYPE]->source) &&
		    CompileTime_Get(v->array->items[OB_TYPE]->source)->value ==
		     (long)(&PyTuple_Type) &&
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
			/* XXX no traceback support */
			/* tb will be decref'ed by vinfo_decref(v) below */
			vinfo_decref(v, po);
			break;
		}
		else {
			/* end of an EXCEPT block, re-raise the exception
			   stored in the stack.
			   XXX when implementing tracebacks find a trick to
			       mark this as a re-raising */
			po->pr.exc = v;
			POP(po->pr.val);
			POP(v);
			vinfo_decref(v, po);   /* XXX traceback */
			break;
		}

	/*MISSING_OPCODE(BUILD_CLASS);
	  MISSING_OPCODE(STORE_NAME);
	  MISSING_OPCODE(DELETE_NAME);*/

	case UNPACK_SEQUENCE:
	{
		int i;
		vinfo_array_t* array;
		void* cimpl_unpack = cimpl_unpack_iterable;
		
		v = TOP();
		u = get_array_item(po, TOP(), OB_TYPE);
		if (u == NULL)
			break;
		switch (psyco_switch_index(po, u, &psyfs_tuple_list)) {
			
		case 0:   /* PyTuple_Type */

			/* shortcut: is this a virtual tuple?
			             of the correct length? */
			if (PsycoTuple_Load(v) != oparg) {

				/* No, fall back to the default path:
				   load the size, compare it with oparg,
				   and if they match proceed by loading the
				   tuple item by item into the stack. */
				vinfo_t* vsize;
				vsize = get_array_item(po, v, VAR_OB_SIZE);
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
					w = get_array_item(po, v,
							   TUPLE_OB_ITEM + i);
					if (w == NULL)
						break;
				}
			}
			/* copy the tuple items into the stack */
			for (i=oparg; i--; ) {
				w = v->array->items[TUPLE_OB_ITEM + i];
                                vinfo_incref(w);
                                PUSH(w);
                                /* in case the tuple is freed while its items
                                   are still in use: */
                                need_reference(po, w);
                        }
                        break;

		case 1:   /* PyList_Type */
			cimpl_unpack = cimpl_unpack_list;
			/* fall through */

		default:
			array = array_new(oparg);
			if (psyco_flag_call(po, cimpl_unpack,
					    CfReturnFlag|CfPyErrIfNonNull,
					    "vlA", v, oparg, array) == CC_ERROR){
				array_delete(array, po);
				break;
			}
			POP_DECREF();
			for (i=oparg; i--; )
				PUSH(array->items[i]);
			array_release(array);
			goto fine;
		}
		break;
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
		KNOWN_VAR(PyObject*, globals, LOC_GLOBALS);
		PyObject* w = GETNAMEV(oparg);

		if (psyco_flag_call(po, PyDict_SetItem,
				    CfReturnFlag|CfPyErrIfNonNull,
				    "llv", globals, w, TOP()) == CC_ERROR)
			break;
		POP_DECREF();
		goto fine;
	}

	case DELETE_GLOBAL:
	{
		KNOWN_VAR(PyObject*, globals, LOC_GLOBALS);
		PyObject* w = GETNAMEV(oparg);

		if (psyco_flag_call(po, PyDict_DelItem,
				    CfReturnFlag|CfPyErrIfNonNull,
				    "ll", globals, w) == CC_ERROR) {
			/*if (!Py_Occurred() at run-time)*/
			PycException_SetFormat(po, PyExc_NameError,
					       GLOBAL_NAME_ERROR_MSG,
					       PyString_AsString(w));
			break;
		}
		POP_DECREF();
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
                PyObject* value = load_global(po, namev, next_instr);
		if (value == NULL)
			break;
		v = vinfo_new(CompileTime_NewSk(sk_new((long) value,
						       SkFlagPyObj)));
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

		SETLOCAL(oparg, psyco_viZero);
		vinfo_incref(psyco_viZero);
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
		v = PsycoList_New(po, oparg);
		if (v == NULL)
			break;
		
                if (oparg > 0) {
			int i;
			
			/* load 'list->ob_item' into 'w' */
			w = read_array_item(po, v, LIST_OB_ITEM);
			if (w == NULL) {
				vinfo_decref(v, po);
				break;
			}
			
			/* write the list items from 'w' */
			for (i=0; i<oparg; i++) {
				x = NTOP(oparg-i);
				if (!write_array_item_ref(po, w, i, x, true))
					break;
			}
			vinfo_decref(w, po);
			if (PycException_Occurred(po)) {
				vinfo_decref(v, po);
				break;
			}
			while (oparg--)
				POP_DECREF();
		}
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
			
		case IS:      /* pointer comparison */
			cc = integer_cmp(po, v, w, Py_EQ);
			break;
			
		case IS_NOT:  /* pointer comparison */
			cc = integer_cmp(po, v, w, Py_NE);
			break;

		/*MISSING_OPCODE(IN);
		  MISSING_OPCODE(NOT_IN);*/

		case EXC_MATCH:
			cc = psyco_flag_call(po, PyErr_GivenExceptionMatches,
					     CfPure | CfReturnFlag, "vv", v, w);
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

	/*MISSING_OPCODE(IMPORT_NAME);
	  MISSING_OPCODE(IMPORT_STAR);
	  MISSING_OPCODE(IMPORT_FROM);*/

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
		cc = PsycoObject_IsTrue(po, TOP());
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

	case GET_ITER:
		x = PsycoObject_GetIter(po, TOP());
		if (x == NULL)
			break;
		POP_DECREF();
		PUSH(x);
		goto fine;

	case FOR_ITER:
		v = PsycoIter_Next(po, TOP());
		if (v != NULL) {
			/* iterator not exhausted */
			PUSH(v);
		}
		else {
			/* catch PyExc_StopIteration */
			cc = PycException_Matches(po, PyExc_StopIteration);
			if (cc == CC_ERROR)
				break;
			if (runtime_condition_t(po, cc)) {
				/* iterator ended normally */
				PycException_Clear(po);
				POP_DECREF();
				JUMPBY(oparg);
				mp = psyco_next_merge_point(po->pr.merge_points,
							    next_instr);
			}
			else
				break;   /* any other exception */
		}
		goto fine;

	/*MISSING_OPCODE(FOR_LOOP);*/

	case SETUP_LOOP:
	case SETUP_EXCEPT:
	case SETUP_FINALLY:
		block_setup(po, opcode, INSTR_OFFSET() + oparg,
			    STACK_LEVEL());
		goto fine;

	case SET_LINENO:
		/* nothing */
		goto fine;

	case CALL_FUNCTION:
	{
		int na = oparg & 0xff;
		int nk = (oparg>>8) & 0xff;
		int n = na + 2 * nk;
		vinfo_t** args = STACK_POINTER() - n;
		vinfo_t* func = args[-1];
                
		/* build a virtual tuple for apply() */
		v = PsycoTuple_New(na, args);
		if (nk != 0) {
			/* XXX to do: PsycoDict_Virtual() */
			int i;
                        w = PsycoDict_New(po);
			if (w != NULL) {
				/* XXX do something closer to
				   update_keyword_args() in ceval.c, e.g.
				   check for duplicate keywords */
				for (i=na; i<n; i+=2) {
					if (psyco_flag_call(po, PyDict_SetItem,
						CfReturnFlag|CfPyErrIfNonNull,
						"vvv", w, args[i], args[i+1])
					    == CC_ERROR)
						break;
				}
			}
		}
		else
			w = NULL;
		if (PycException_Occurred(po))
			x = NULL;
		else
			x = PsycoObject_Call(po, func, v, w);
		vinfo_xdecref(w, po);
		vinfo_decref(v, po);
		if (x == NULL)
			break;

		/* clean up the stack (remove args and func) */
		while (n-- >= 0)
			POP_DECREF();
		PUSH(x);
		goto fine;
	}
	
	/*MISSING_OPCODE(CALL_FUNCTION_VAR);
	  MISSING_OPCODE(CALL_FUNCTION_KW);
	  MISSING_OPCODE(CALL_FUNCTION_VAR_KW);
	  MISSING_OPCODE(MAKE_FUNCTION);
	  MISSING_OPCODE(MAKE_CLOSURE);
	  MISSING_OPCODE(BUILD_SLICE);*/

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
		extra_assert(!is_respawning(po));
		SAVE_NEXT_INSTR(next_instr);
		if (mp->bytecode_position != next_instr)
			mp = NULL;
		code1 = psyco_compile(po, mp, false);
		goto finished;
	}
	
	/* mark merge points via a call to psyco_compile() */
	if (mp->bytecode_position == next_instr) {
		extra_assert(!is_respawning(po));
		SAVE_NEXT_INSTR(next_instr);
		code1 = psyco_compile(po, mp, true);
		if (code1 != NULL)
			goto finished;
                mp++;
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
	      if (promotion->fs == NULL)
		      code1 = psyco_finish_promotion(po,
						     promote_me,
						     promotion->kflags);
	      else
		      code1 = psyco_finish_fixed_switch(po,
							promote_me,
							promotion->kflags,
							promotion->fs);
	      goto finished;
      }
      
      /* At this point, we got a real pseudo-exception. */
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
		/* unlike ceval.c, SETUP_FINALLY always pushes a single
		   object on the stack. See comments in pycompiler.h.
		   This object is a 3-tuple (exc, value, traceback) which
		   might represent a pseudo-exception like EReturn. */
		int next_instr;
		vinfo_t** stack_a = po->vlocals.items + po->pr.stack_base;
		vinfo_t* exc_info = PsycoTuple_New(3, NULL);
		PycException_Fetch(po);
		exc_info->array->items[TUPLE_OB_ITEM + 0] = po->pr.exc;
		exc_info->array->items[TUPLE_OB_ITEM + 1] = po->pr.val;
		po->pr.exc = NULL;
		po->pr.val = NULL;
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
		PUSH(psyco_viNone);  vinfo_incref(psyco_viNone);
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
#ifdef ALL_CHECKS
  if (PyErr_Occurred()) {
	  fprintf(stderr, "psyco: unexpected Python exception during compilation:\n");
	  PyErr_WriteUnraisable(Py_None);
  }
#endif
  PyErr_Restore(old_py_exc, old_py_val, old_py_tb);
  return code1;
}
