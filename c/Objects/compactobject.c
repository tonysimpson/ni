#include "compactobject.h"
#if HAVE_COMPACT_OBJECT
#include "../blockalloc.h"
#include "../dispatcher.h"
#include "../Python/pycompiler.h"
#include "pintobject.h"


#define DEBUG_K_IMPL   1


BLOCKALLOC_STATIC(kimpl, compact_impl_t, 4096)


static compact_impl_t k_empty_impl = {
	NULL,          /* attrname */
	NULL,          /* vattr */
	0,             /* datasize */
	NULL,          /* extensions */
	NULL,          /* next */
	NULL,          /* parent */
};

/*****************************************************************/

#if DEBUG_K_IMPL

static void debug_k_impl(compact_impl_t* p)
{
	int smin, smax;
	compact_impl_t *q, *lim;
	fprintf(stderr, "\t$ ");
	lim = &k_empty_impl;
	while (p != lim) {
		for (q=p; q->parent != lim; q = q->parent)
			;
		fprintf(stderr, "  %s", PyString_AsString(q->attrname));
		smin = p->datasize;
		smax = 0;
		k_attribute_range(q->vattr, &smin, &smax);
		if (smin < smax)
			fprintf(stderr, "(%d-%d)", smin, smax);
		else
			fprintf(stderr, "(void)");
		lim = q;
	}
	fprintf(stderr, ".\n");
}

#else  /* !DEBUG_K_IMPL */
# define debug_k_impl(p)   /* nothing */
#endif

/*****************************************************************/

static PyObject *
compact_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyCompactObject* ko;
	char* keywords[] = {NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "", keywords))
		return NULL;

	ko = (PyCompactObject*) type->tp_alloc(type, 0);
	if (ko != NULL) {
		ko->k_impl = &k_empty_impl;
	}
	return (PyObject*) ko;
}

DEFINEFN
PyObject* PyCompact_New(void)
{
	PyObject* o = _PyObject_GC_Malloc(sizeof(PyCompactObject));
	if (o != NULL) {
		o = PyObject_INIT(o, &PyCompact_Type);
		((PyCompactObject*) o)->k_impl = &k_empty_impl;
		((PyCompactObject*) o)->k_data = NULL;
		PyObject_GC_Track(o);
	}
	return o;
}

static bool k_same_vinfo(vinfo_t* a, vinfo_t* b)
{
	if (a->source != b->source) {
		if (is_compiletime(a->source) && is_compiletime(b->source))
			return (CompileTime_Get(a->source)->value ==
				CompileTime_Get(b->source)->value);
		else
			return false;
	}
	if (a->array != b->array) {
		int i = a->array->count;
		if (i != b->array->count)
			return false;
		while (--i >= 0) {
			if (a->array->items[i] == b->array->items[i])
				continue;
			if (a->array->items[i] == NULL ||
			    b->array->items[i] == NULL)
				return false;
			if (!k_same_vinfo(a->array->items[i],
					  b->array->items[i]))
				return false;
		}
	}
	return true;
}

static int k_fix_run_time_vars(vinfo_t* a, int datasize)
{
	if (is_runtime(a->source)) {
		bool ref = has_rtref(a->source);
		int sindex = datasize;
		datasize += sizeof(long);
		extra_assert(sizeof(long) == sizeof(PyObject*));
		a->source = RunTime_NewStack(sindex, ref, false);
	}
	if (a->array != NullArray) {
		int i, n = a->array->count;
		for (i=0; i<n; i++) {
			if (a->array->items[i] != NULL)
				datasize = k_fix_run_time_vars(
						a->array->items[i], datasize);
		}
	}
	return datasize;
}

DEFINEFN
compact_impl_t* k_extend_impl(compact_impl_t* oldimpl, PyObject* attr,
			      vinfo_t* v)
{
	int datasize;
	compact_impl_t* p;
	extra_assert(PyString_CheckExact(attr) && PyString_CHECK_INTERNED(attr));

	/* enumerate the run-time entries */
	datasize = k_fix_run_time_vars(v, oldimpl->datasize);

	/* look for a matching existing extension of oldimpl */
	for (p = oldimpl->extensions; p != NULL; p = p->next) {
		if (p->attrname == attr && p->datasize == datasize &&
		    k_same_vinfo(v, p->vattr))
			return p;
	}

	/* build a new impl */
	p = psyco_llalloc_kimpl();
	p->attrname = attr;
	Py_INCREF(attr);
	p->vattr = v;
	vinfo_incref(v);
	p->datasize = datasize;
	p->extensions = NULL;
	p->next = oldimpl->extensions;
	oldimpl->extensions = p;
	p->parent = oldimpl;
        debug_k_impl(p);
        return p;
}

#if PSYCO_DEBUG
static int k_check_extension(compact_impl_t* impl1, compact_impl_t* impl2)
{
	while (impl1 != impl2) {
		impl1 = impl1->parent;
		if (impl1 == NULL)
			return false;
	}
	return true;
}
#endif

static int k_extend(PyCompactObject* ko, compact_impl_t* nimpl)
{
	char* ndata;
	extra_assert(k_check_extension(nimpl, ko->k_impl));
	if (nimpl->datasize > K_ROUNDUP(ko->k_impl->datasize)) {
		ndata = ko->k_data;
		ndata = (char*) PyMem_Realloc(ndata,
					      K_ROUNDUP(nimpl->datasize));
		if (ndata == NULL) {
			PyErr_NoMemory();
			return -1;
		}
		ko->k_data = ndata;
	}
	ko->k_impl = nimpl;
	return 0;
}

DEFINEFN
vinfo_t* vinfo_copy_no_share(vinfo_t* vi)
{
	vinfo_t* result = vinfo_new_skref(vi->source);
	if (vi->array != NullArray) {
		int i = vi->array->count;
		vinfo_array_grow(result, i);
		while (--i >= 0) {
			if (vi->array->items[i] != NULL)
				result->array->items[i] =
				    vinfo_copy_no_share(vi->array->items[i]);
		}
	}
	return result;
}


/*****************************************************************/

/* decref all PyObjects found in the CompactObject */
static void k_decref_objects(vinfo_t* a, char* data)
{
	if (has_rtref(a->source)) {
		int sindex = getstack(a->source);
		PyObject* o = *(PyObject**)(data+sindex);
		Py_DECREF(o);
	}
	if (a->array != NullArray) {
		int i = a->array->count;
		while (--i >= 0) {
			if (a->array->items[i] != NULL)
				k_decref_objects(a->array->items[i], data);
		}
	}
}

static void compact_dealloc(PyCompactObject* ko)
{
	compact_impl_t* impl = ko->k_impl;
	while (impl->vattr != NULL) {
		k_decref_objects(impl->vattr, ko->k_data);
		impl = impl->parent;
	}
	PyMem_Free(ko->k_data);
	ko->ob_type->tp_free((PyObject*) ko);
}

static int k_traverse_objects(vinfo_t* a, char* data,
			      visitproc visit, void* arg)
{
	int err;
	if (has_rtref(a->source)) {  /* run-time && with reference */
		int sindex = getstack(a->source);
		PyObject* o = *(PyObject**)(data+sindex);
		err = visit(o, arg);
		if (err)
			return err;
	}
	if (a->array != NullArray) {
		int i = a->array->count;
		while (--i >= 0) {
			if (a->array->items[i] != NULL) {
				err = k_traverse_objects(a->array->items[i],
							 data, visit, arg);
				if (err)
					return err;
			}
		}
	}
	return 0;
}

static int compact_traverse(PyCompactObject* ko, visitproc visit, void* arg)
{
	int err;
	compact_impl_t* impl = ko->k_impl;
	while (impl->vattr != NULL) {
		err = k_traverse_objects(impl->vattr, ko->k_data, visit, arg);
		if (err)
			return err;
		impl = impl->parent;
	}
	return 0;
}

static int compact_clear(PyCompactObject* ko)
{
	compact_impl_t* impl = ko->k_impl;
	char*           data = ko->k_data;
	ko->k_impl = &k_empty_impl;
	ko->k_data = NULL;
	while (impl->vattr != NULL) {
		k_decref_objects(impl->vattr, data);
		impl = impl->parent;
	}
	PyMem_Free(data);
	return 0;
}


/*****************************************************************/

static PyObject* compact_getattro(PyCompactObject* ko, PyObject* attr)
{
	compact_impl_t* impl = ko->k_impl;
	PyObject* o;
	
	Py_INCREF(attr);
	K_INTERN(attr);
	while (impl->attrname != NULL) {
		if (impl->attrname == attr) {
			o = direct_xobj_vinfo(impl->vattr, ko->k_data);
			if (o != NULL || PyErr_Occurred())
				goto finally;
		}
		impl = impl->parent;
	}
	o = PyObject_GenericGetAttr((PyObject*) ko, attr);
 finally:
	Py_DECREF(attr);
	return o;
}

DEFINEFN
bool k_match_vinfo(vinfo_t* vnew, vinfo_t* vexisting)
{
	if (vnew == NULL)
		return vexisting == NULL;
	if (vexisting == NULL)
		return false;
	switch (gettime(vnew->source)) {

	case RunTime:
		if (!is_runtime(vexisting->source))
			return false;
		break;

	case CompileTime:
		if (!is_compiletime(vexisting->source))
			return false;
		return CompileTime_Get(vnew->source)->value ==
			CompileTime_Get(vexisting->source)->value;

	case VirtualTime:
		if (vexisting->source != vnew->source)
			return false;
		break;
	}
	if (vnew->array != vexisting->array) {
		int i, n = vexisting->array->count;
		if (n != vnew->array->count)
			return false;
		for (i=0; i<n; i++) {
			if (!k_match_vinfo(vnew->array->items[i],
				      vexisting->array->items[i]))
				return false;
		}
	}
	return true;
}

static char* k_store_vinfo(vinfo_t* v, char* target, char* source)
{
	if (is_runtime(v->source)) {
		int sindex = getstack(v->source);
		if (has_rtref(v->source)) {
			PyObject* o = *(PyObject**) source;
			source += sizeof(PyObject*);
			*(PyObject**)(target+sindex) = o;
			Py_INCREF(o);
		}
		else {
			long l = *(long*) source;
			source += sizeof(long);
			*(long*)(target+sindex) = l;
		}
	}
	if (v->array != NullArray) {
		int i, n = v->array->count;
		for (i=0; i<n; i++) {
			if (v->array->items[i] != NULL)
				source = k_store_vinfo(v->array->items[i],
						       target, source);
		}
	}
	return source;
}

DEFINEFN
void k_attribute_range(vinfo_t* v, int* smin, int* smax)
{
	/* XXX Assumes that the data for an attr is stored consecutively */
	if (is_runtime(v->source)) {
		int sindex = getstack(v->source);
		if (*smin > sindex)
			*smin = sindex;
		sindex += sizeof(PyObject*);
		extra_assert(sizeof(PyObject*) == sizeof(long));
		if (*smax < sindex)
			*smax = sindex;
	}
	if (v->array != NullArray) {
		int i = v->array->count;
		while (--i >= 0) {
			if (v->array->items[i] != NULL)
				k_attribute_range(v->array->items[i],
						  smin, smax);
		}
	}
}

static void k_shift_rt_pos(vinfo_t* v, int delta)
{
	if (is_runtime(v->source)) {
		v->source += delta;
	}
	if (v->array != NullArray) {
		int i = v->array->count;
		while (--i >= 0) {
			if (v->array->items[i] != NULL)
				k_shift_rt_pos(v->array->items[i], delta);
		}
	}
}

DEFINEFN
compact_impl_t* k_duplicate_impl(compact_impl_t* base,
				 compact_impl_t* first_excluded,
				 compact_impl_t* last,
				 int shift_delta)
{
	vinfo_t* v;
	if (first_excluded == last)
		return base;
	base = k_duplicate_impl(base, first_excluded, last->parent,
				shift_delta);
	v = vinfo_copy_no_share(last->vattr);
	k_shift_rt_pos(v, shift_delta);
	return k_extend_impl(base, last->attrname, v);
}

static
int compact_setattro(PyCompactObject* ko, PyObject* attr, PyObject* value)
{
	int err, smin, smax;
	long immed_value;
	vinfo_t* source_vi;
	char* source_data;
	compact_impl_t* impl;
	compact_impl_t* p;

        /* NB. this assumes that 'attr' is an already-interned string.
           PyObject_SetAttr() should have interned it. */

	/* recognize a few obvious object types and optimize accordingly
	   Note that this is not related to Psyco's ability to store
	   attributes with arbitrary flexibility, which is implemented in
	   pcompactobject.c. */
	if (value == NULL) {
		source_vi = NULL;
		source_data = NULL;
	}
	else if (PyInt_CheckExact(value)) {
		immed_value = PyInt_AS_LONG(value);
		source_vi = PsycoInt_FROM_LONG(vinfo_new(SOURCE_DUMMY));
		source_data = (char*) &immed_value;
	}
	else if (value == Py_None) {
		source_vi = psyco_vi_None();
		source_data = NULL;
	}
	else {
		source_vi = vinfo_new(SOURCE_DUMMY_WITH_REF);
		source_data = (char*) &value;
	}
	impl = ko->k_impl;
	while (impl->attrname != NULL) {
		if (impl->attrname == attr) {
			k_decref_objects(impl->vattr, ko->k_data);
			
			if (k_match_vinfo(source_vi, impl->vattr)) {
				/* the attr already has the correct format */
				k_store_vinfo(impl->vattr, ko->k_data,
					      source_data);
				err = 0;
				goto finally;
			}

			/* a format change is needed: first delete the
			 * existing attribute.
			 * XXX Not too efficient right now.
			 * XXX Also assumes that attribute order matches
			 * XXX data storage order.
			 */
			smin = ko->k_impl->datasize;
			smax = 0;
			k_attribute_range(impl->vattr, &smin, &smax);
			if (smax < smin)
				smax = smin;
			
			/* data between smin and smax is removed */
			memmove(ko->k_data + smin,
                                ko->k_data + smax,
                                ko->k_impl->datasize - smax);

			/* make the new 'impl' by starting from impl->parent
			   and accounting for all following attrs excluding
			   'impl', shifted as per memmove() */
			ko->k_impl = k_duplicate_impl(impl->parent, impl,
						      ko->k_impl, smin - smax);

			if (source_vi != NULL)
				goto store_data; /* now, re-create the attr
						    under its new format */
			err = 0;
			goto finally;   /* if attribute deletion: done */
		}
		impl = impl->parent;
	}

	if (source_vi == NULL) {
		/* deleting a non-existing attribute */
		err = PyObject_GenericSetAttr((PyObject*) ko, attr, NULL);
		goto finally;
	}

	/* setting a new attribute */
 store_data:
	p = k_extend_impl(ko->k_impl, attr, source_vi);
	err = k_extend(ko, p);
	if (err == 0) {
		k_store_vinfo(p->vattr, ko->k_data, source_data);
	}

 finally:
	vinfo_xdecref(source_vi, NULL);
	return err;
}


/*****************************************************************/

DEFINEVAR compact_impl_t* PyCompact_EmptyImpl;

DEFINEVAR PyTypeObject PyCompact_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                                      /*ob_size*/
	"_psyco.compact",                       /*tp_name*/
	sizeof(PyCompactObject),                /*tp_size*/
	0,                                      /*tp_itemsize*/
	/* methods */
	(destructor)compact_dealloc,            /* tp_dealloc */
	0,                                      /* tp_print */
	0,                                      /* tp_getattr */
	0,                                      /* tp_setattr */
	0,                                      /* tp_compare */
	0,                                      /* tp_repr */
	0,                                      /* tp_as_number */
	0,                                      /* tp_as_sequence */
	0,                                      /* tp_as_mapping */
	0,                                      /* tp_hash */
	0,                                      /* tp_call */
	0,                                      /* tp_str */
	(getattrofunc)compact_getattro,         /* tp_getattro */
	(setattrofunc)compact_setattro,         /* tp_setattro */
	0,                                      /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
		Py_TPFLAGS_BASETYPE,		/* tp_flags */
	0,                                      /* tp_doc */
	(traverseproc)compact_traverse,         /* tp_traverse */
	(inquiry)compact_clear,                 /* tp_clear */
	0,                                      /* tp_richcompare */
	0,                                      /* tp_weaklistoffset */
	0,                                      /* tp_iter */
	0,                                      /* tp_iternext */
	0,                                      /* tp_methods */
	0,                                      /* tp_members */
	0,                                      /* tp_getset */
	0,                                      /* tp_base */
	0,                                      /* tp_dict */
	0,                                      /* tp_descr_get */
	0,                                      /* tp_descr_set */
	0,                                      /* tp_dictoffset */
	0,                                      /* tp_init */
	/*PyType_GenericAlloc set below*/ 0,    /* tp_alloc */
	compact_new,                            /* tp_new */
	/*PyObject_GC_Del set below*/ 0,        /* tp_free */
};

void psyco_compact_init(void)
{
	PyCompact_EmptyImpl = &k_empty_impl;
	PyCompact_Type.ob_type = &PyType_Type;
	PyCompact_Type.tp_alloc = &PyType_GenericAlloc;
	PyCompact_Type.tp_free = &PyObject_GC_Del;
}

#else  /* !HAVE_COMPACT_OBJECT */
INITIALIZATIONFN
void psyco_compact_init(void) { }
#endif /* !HAVE_COMPACT_OBJECT */
