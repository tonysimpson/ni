#include "ptupleobject.h"


 /***************************************************************/
  /***   Virtual tuples                                        ***/

static source_virtual_t psyco_computed_tuple;

static bool compute_tuple(PsycoObject* po, vinfo_t* v)
{
	int i, tuple_end = v->array->count;
	
	extra_assert(tuple_end == TUPLE_OB_ITEM +
		   CompileTime_Get(v->array->items[VAR_OB_SIZE]->source)->value);
	
	/* check whether all tuple objects are constant */
	for (i=TUPLE_OB_ITEM; i<tuple_end; i++) {
		vinfo_t* vi = v->array->items[i];
		NonVirtualSource src = vinfo_compute(vi, po);
		if (src == SOURCE_ERROR)
			return false;
		if (!is_compiletime(src))
			break;  /* no */
	}
	if (i == tuple_end) {
		/* yes -- let's build a constant compile-time tuple */
		source_known_t* sk;
		PyObject* constant = PyTuple_New(tuple_end - TUPLE_OB_ITEM);
		if (constant == NULL)
			OUT_OF_MEMORY();
		for (i=TUPLE_OB_ITEM; i<tuple_end; i++) {
			PyObject* obj;
			sk = CompileTime_Get(v->array->items[i]->source);
			obj = (PyObject*) sk->value;
			Py_INCREF(obj);
			PyTuple_SET_ITEM(constant, i-TUPLE_OB_ITEM, obj);
		}
		sk = sk_new((long) constant, SkFlagPyObj);
		v->source = CompileTime_NewSk(sk);
	}
	else {
		/* no -- code a call to PyTuple_New() */
		vinfo_t* tuple = psyco_generic_call(po, PyTuple_New,
                                                    CfReturnRef|CfPyErrIfNull,
                                                    "l",
                                                    tuple_end - TUPLE_OB_ITEM);
		if (tuple == NULL)
			return false;

		/* write the storing of the objects in the tuple */
		for (i=TUPLE_OB_ITEM; i<tuple_end; i++) {
			vinfo_t* vi = v->array->items[i];
			if (!write_array_item_ref(po, tuple, i, vi, false)) {
				vinfo_decref(tuple, po);
				return false;
			}
		}
		vinfo_move(po, v, tuple);
	}
	return true;
}

DEFINEFN
vinfo_t* PsycoTuple_New(int count, vinfo_t** source)
{
	int i;
	vinfo_t* r = vinfo_new(VirtualTime_New(&psyco_computed_tuple));
	r->array = array_new(TUPLE_OB_ITEM + count);
	r->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyTuple_Type)));
	r->array->items[VAR_OB_SIZE] = vinfo_new(CompileTime_New(count));
	if (source != NULL)
		for (i=0; i<count; i++) {
			vinfo_incref(source[i]);
			r->array->items[TUPLE_OB_ITEM + i] = source[i];
		}
	return r;
}


/* do not load a constant tuple into a vinfo_array_t if longer than: */
#define CT_TUPLE_LOAD_SIZE_LIMIT    15

DEFINEFN
int PsycoTuple_Load(vinfo_t* tuple)
{
	int size;
	/* if the tuple is virtual, then all items in its
	   vinfo_array_t are already filled */
	if (tuple->source == VirtualTime_New(&psyco_computed_tuple))
		size = tuple->array->count - TUPLE_OB_ITEM;
	else if (is_compiletime(tuple->source)) {
		/* a constant tuple means constant tuple items */
		int i;
		PyObject* o = (PyObject*)(CompileTime_Get(tuple->source)->value);
		extra_assert(PyTuple_Check(o));
		size = PyTuple_GET_SIZE(o);
		if (tuple->array->count < TUPLE_OB_ITEM + size) {
			if (/*not_too_much &&*/ size > CT_TUPLE_LOAD_SIZE_LIMIT)
				return -1;
			vinfo_array_grow(tuple, TUPLE_OB_ITEM + size);
		}
		/* load the tuple into the vinfo_array_t */
		for (i=0; i<size; i++) {
			if (tuple->array->items[TUPLE_OB_ITEM + i] == NULL) {
				PyObject* item = PyTuple_GET_ITEM(o, i);
				source_known_t* sk = sk_new((long) item,
							    SkFlagPyObj);
				Py_INCREF(item);
				tuple->array->items[TUPLE_OB_ITEM + i] =
					vinfo_new(CompileTime_NewSk(sk));
			}
		}
	}
	else
		return -1;
	return size;
}


 /***************************************************************/
  /*** tuple objects meta-implementation                       ***/

static vinfo_t* ptuple_item(PsycoObject* po, vinfo_t* a, vinfo_t* i)
{
	condition_code_t cc;
	vinfo_t* vlen;
	vinfo_t* result;

	vlen = get_array_item(po, a, VAR_OB_SIZE);
	if (vlen == NULL)
		return NULL;
	
	cc = integer_cmp(po, i, vlen, Py_GE|COMPARE_UNSIGNED);
	if (cc == CC_ERROR)
		return NULL;

	if (runtime_condition_f(po, cc)) {
		PycException_SetString(po, PyExc_IndexError,
				       "tuple index out of range");
		return NULL;
	}

	result = read_immut_array_item_var(po, a, TUPLE_OB_ITEM, i, false);
	/* the tuple could be freed while the returned item is still in use */
	if (result != NULL)
		need_reference(po, result);
	return result;
}


DEFINEFN
void psy_tupleobject_init()
{
	PySequenceMethods *m = PyTuple_Type.tp_as_sequence;
	Psyco_DefineMeta(m->sq_length, psyco_generic_immut_ob_size);
	Psyco_DefineMeta(m->sq_item, ptuple_item);
	psyco_computed_tuple.compute_fn = &compute_tuple;
}
