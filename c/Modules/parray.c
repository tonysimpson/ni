#include "../psyco.h"
#include "../Objects/pabstract.h"
#include "../Objects/pintobject.h"
#include "../Objects/pfloatobject.h"
#include "../Objects/pstringobject.h"


/***************************************************************/

/* the following two variables are set by the initialization function
   to point to private things in arraymodule.c */
static PyCFunction cimpl_array = NULL;   /* ptr to C impl of array.array() */
static PyTypeObject* arraytype;          /* array.ArrayType */


/* declarations copied from arraymodule.c */
struct arrayobject; /* Forward */
struct arraydescr {
	int typecode;
	int itemsize;
	PyObject * (*getitem)(struct arrayobject *, int);
	int (*setitem)(struct arrayobject *, int, PyObject *);
};
typedef struct arrayobject {
	PyObject_VAR_HEAD
	char *ob_item;
	struct arraydescr *ob_descr;
} arrayobject;


#define ARRAY_OB_ITEM        QUARTER(offsetof(arrayobject, ob_item))
#define ARRAY_OB_DESCR       QUARTER(offsetof(arrayobject, ob_descr))


/* meta-implementation of some getitem() and setitem() functions.
   Our meta-implementations of setitem() are never called with a run-time
   index equal to -1; to do the equivalent of the array module's object
   type checking, you must call setitem() with a *compile-time* value of -1. */

static vinfo_t* p_c_getitem(PsycoObject* po, vinfo_t* ap, vinfo_t* vi)
{
	vinfo_t* result;
	vinfo_t* ob_item;
	vinfo_t* chr;
	
	ob_item = read_array_item(po, ap, ARRAY_OB_ITEM);
	if (ob_item == NULL)
		return NULL;

	chr = read_array_item_var(po, ob_item, 0, vi, true);
	vinfo_decref(ob_item, po);
	if (chr == NULL)
		return NULL;
	
	result = PsycoCharacter_New(chr);
	vinfo_decref(chr, po);
	return result;
}

static vinfo_t* p_BB_getitem(PsycoObject* po, vinfo_t* ap, vinfo_t* vi)
{
	vinfo_t* ob_item;
	vinfo_t* value;
	
	ob_item = read_array_item(po, ap, ARRAY_OB_ITEM);
	if (ob_item == NULL)
		return NULL;

	value = read_array_item_var(po, ob_item, 0, vi, true);
	vinfo_decref(ob_item, po);
	if (value == NULL)
		return NULL;
	
	return PsycoInt_FROM_LONG(value);
}

static vinfo_t* p_l_getitem(PsycoObject* po, vinfo_t* ap, vinfo_t* vi)
{
	vinfo_t* ob_item;
	vinfo_t* value;
	
	ob_item = read_array_item(po, ap, ARRAY_OB_ITEM);
	if (ob_item == NULL)
		return NULL;

	value = read_array_item_var(po, ob_item, 0, vi, false);
	vinfo_decref(ob_item, po);
	if (value == NULL)
		return NULL;
	
	return PsycoInt_FROM_LONG(value);
}

static bool p_l_setitem(PsycoObject* po, vinfo_t* ap, vinfo_t* vi, vinfo_t* v)
{
	bool result;
	vinfo_t* ob_item;
	vinfo_t* value;
	
	value = PsycoInt_AsLong(po, v);
	if (value == NULL) {
		/* XXX translate TypeError("an integer is required")
			    into TypeError("array item must be integer") */
		return false;
	}
	
	ob_item = read_array_item(po, ap, ARRAY_OB_ITEM);
	result = (ob_item != NULL) &&
		write_array_item_var(po, ob_item, 0, vi, value);
	vinfo_xdecref(ob_item, po);
	vinfo_decref(value, po);
	return result;
}

#if SIZEOF_INT==SIZEOF_LONG
# define p_i_getitem  p_l_getitem
# define p_i_setitem  p_l_setitem
#else
# define p_i_getitem  NULL
# define p_i_setitem  NULL
#endif

static vinfo_t* p_d_getitem(PsycoObject* po, vinfo_t* ap, vinfo_t* vi)
{
	vinfo_t* ob_item;
	vinfo_t* val1;
	vinfo_t* val2;
	vinfo_t* vi2;
	
	ob_item = read_array_item(po, ap, ARRAY_OB_ITEM);
	if (ob_item == NULL)
		return NULL;

	/* XXX use the i386's multiply-by-8 addressing mode */
	vi2 = integer_add(po, vi, vi, false);
	if (vi2 == NULL) {
		vinfo_decref(ob_item, po);
		return NULL;
	}
	val1 = read_array_item_var(po, ob_item, 0, vi2, false);
	val2 = read_array_item_var(po, ob_item, 1, vi2, false);
	vinfo_decref(vi2, po);
	vinfo_decref(ob_item, po);
	if (val1 == NULL || val2 == NULL) {
		vinfo_xdecref(val2, po);
		vinfo_xdecref(val1, po);
		return NULL;
	}
	
	return PsycoFloat_FROM_DOUBLE(val1, val2);
}


/* list of meta-implementations of descriptor-specific getitem() and setitem() */
struct metadescr_s {
	int typecode;
	vinfo_t* (*meta_getitem) (PsycoObject*, vinfo_t*, vinfo_t*);
	bool (*meta_setitem) (PsycoObject*, vinfo_t*, vinfo_t*, vinfo_t*);
};

static struct metadescr_s metadescriptors[] = {
	{'c', p_c_getitem, NULL},
	{'B', p_BB_getitem, NULL},
	{'i', p_i_getitem, p_i_setitem},
	{'l', p_l_getitem, p_l_setitem},
	{'d', p_d_getitem, NULL},
	{'\0', NULL, NULL} /* Sentinel */
};


/* meta-implementation of array_item */
static vinfo_t* parray_item(PsycoObject* po, vinfo_t* ap, vinfo_t* vi)
{
	condition_code_t cc;
	vinfo_t* vlen;
	struct arraydescr* d;
	long dlong;

	/* get the ob_descr field of 'ap' and promote it to compile-time */
	vinfo_t* vdescr = get_array_item(po, ap, ARRAY_OB_DESCR);
	if (vdescr == NULL)
		return NULL;
	dlong = psyco_atcompiletime(po, vdescr);
	if (dlong == -1) {
		/* a pointer cannot be -1, so we know it must be an exception */
		extra_assert(PycException_Occurred(po));
		return NULL;
	}
	d = (struct arraydescr*) dlong;

	/* check that the index is within range */
	vlen = read_array_item(po, ap, VAR_OB_SIZE);
	if (vlen == NULL)
		return NULL;
	
	cc = integer_cmp(po, vi, vlen, Py_GE|COMPARE_UNSIGNED);
        vinfo_decref(vlen, po);
	if (cc == CC_ERROR)
		return NULL;

	if (runtime_condition_f(po, cc)) {
		PycException_SetString(po, PyExc_IndexError,
				       "array index out of range");
		return NULL;
	}

	/* call the item getter or its meta-implementation */
	return Psyco_META2(po, d->getitem, CfReturnRef|CfPyErrIfNull,
			   "vv", ap, vi);
}

/*
static bool parray_ass_item(PsycoObject* po, vinfo_t* ap, vinfo_t* vi,vinfo_t* v)
{
	......
	}*/


/* array creation: we know the result is of type ArrayType.
   XXX we should also decode a constant-time description character. */
DEF_KNOWN_RET_TYPE_2(pa_array, cimpl_array, CfReturnRef|CfPyErrIfNull, arraytype)

#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
static vinfo_t* parray_new(PsycoObject* po, PyTypeObject* type,
			   vinfo_t* varg, vinfo_t* vkw)
{
	vinfo_t* result = psyco_generic_call(po, arraytype->tp_new,
					     CfReturnRef|CfPyErrIfNull,
					     "lvv", type, varg, vkw);
	if (result != NULL) {
		set_array_item(po, result, OB_TYPE,
			       vinfo_new(CompileTime_New((long) arraytype)));
	}
	return result;
}
#endif

/***************************************************************/


INITIALIZATIONFN
void psyco_initarray(void)
{
	PyObject* md = Psyco_DefineMetaModule("array");
	PyObject* arrayobj;
	struct metadescr_s* descr;
	PySequenceMethods* m;

	if (md == NULL)
		return;
	
	/* get array.array and array.ArrayType */
	arrayobj = Psyco_GetModuleObject(md, "array", NULL);
	arraytype = (PyTypeObject*)
		Psyco_GetModuleObject(md, "ArrayType", &PyType_Type);

	/* bail out if not found */
	if (arrayobj == NULL || arraytype == NULL) {
		Py_DECREF(md);
		return;
	}

	m = arraytype->tp_as_sequence;
	Psyco_DefineMeta(m->sq_length,   psyco_generic_mut_ob_size);
	Psyco_DefineMeta(m->sq_item,     parray_item);
	/*Psyco_DefineMeta(m->sq_ass_item, parray_ass_item);*/
		
	/* map array.array() to its meta-implementation pa_array() */
	cimpl_array = Psyco_DefineModuleC(md, "array", METH_VARARGS,
					  &pa_array, &parray_new);

	for (descr=metadescriptors; descr->typecode!=0; descr++) {
		/* There seem to be no better way to get Python's
		   original array descriptors than to create dummy
		   arrays */
		PyObject* array = PyObject_CallFunction(arrayobj, "c",
						(char) descr->typecode);
		if (!array) {
			PyErr_Clear();
			debug_printf(("psyco: note: cannot create an array of "
				      "typecode '%c'\n", (char)descr->typecode));
		}
		else {
			struct arraydescr* d = ((arrayobject*) array)->ob_descr;
			if (descr->meta_getitem)
				Psyco_DefineMeta(d->getitem,descr->meta_getitem);
			if (descr->meta_setitem)
				Psyco_DefineMeta(d->setitem,descr->meta_setitem);
			Py_DECREF(array);
		}
	}
	Py_DECREF(md);
}
