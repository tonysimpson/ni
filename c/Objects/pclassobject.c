#include "pclassobject.h"
#include "ptupleobject.h"


DEFINEFN
vinfo_t* PsycoMethod_New(PyObject* func, vinfo_t* self, PyObject* cls)
{
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_method));
	
	extra_assert(METHOD_SIZE > METHOD_IM_FUNC);
	extra_assert(METHOD_SIZE > METHOD_IM_SELF);
	extra_assert(METHOD_SIZE > METHOD_IM_CLASS);
	
	result->array = array_new(METHOD_SIZE);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyMethod_Type)));
        
        Py_INCREF(func);
	result->array->items[METHOD_IM_FUNC] =
		vinfo_new(CompileTime_NewSk(sk_new((long) func, SkFlagPyObj)));
        
	vinfo_incref(self);
	result->array->items[METHOD_IM_SELF] = self;
        
        Py_INCREF(cls);
	result->array->items[METHOD_IM_CLASS] =
		vinfo_new(CompileTime_NewSk(sk_new((long) cls, SkFlagPyObj)));
        
	return result;
}


static bool compute_method(PsycoObject* po, vinfo_t* methobj)
{
	vinfo_t* newobj;
	vinfo_t* im_func;
	vinfo_t* im_self;
	vinfo_t* im_class;
	
	/* get the fields from the Python object 'methobj' */
	im_func = get_array_item(po, methobj, METHOD_IM_FUNC);
	if (im_func == NULL)
		return false;
	im_self = get_array_item(po, methobj, METHOD_IM_SELF);
	if (im_self == NULL)
		return false;
	im_class = get_array_item(po, methobj, METHOD_IM_CLASS);
	if (im_class == NULL)
		return false;

	/* call PyMethod_New() */
	newobj = psyco_generic_call(po, PyMethod_New,
				    CfPure|CfReturnRef|CfPyErrIfNull,
				    "vvv", im_func, im_self, im_class);
	if (newobj == NULL)
		return false;

	/* move the resulting non-virtual Python object back into 'methobj' */
	vinfo_move(po, methobj, newobj);
	return true;
}


DEFINEVAR source_virtual_t psyco_computed_method;


 /***************************************************************/
  /*** instance method objects meta-implementation             ***/


static vinfo_t* pinstancemethod_call(PsycoObject* po, vinfo_t* methobj,
				     vinfo_t* arg, vinfo_t* kw)
{
	vinfo_t* im_func;
	vinfo_t* im_self;
	vinfo_t* result;
	
	/* get the 'im_self' field from the Python object 'methobj' */
	im_self = get_array_item(po, methobj, METHOD_IM_SELF);
	if (im_self == NULL)
		return NULL;

	if (psyco_knowntobe(im_self, (long) NULL)) {
		/* Unbound methods, XXX implement me */
		goto fallback;
	}
	else {
		int argcount = PsycoTuple_Load(arg);
		int i;
		vinfo_t* newarg;
		if (argcount < 0)
			goto fallback;
		
		newarg = PsycoTuple_New(argcount+1, NULL);
		vinfo_incref(im_self);
		PsycoTuple_GET_ITEM(newarg, 0) = im_self;
		for (i = 0; i < argcount; i++) {
			vinfo_t* v =  PsycoTuple_GET_ITEM(arg, i);
			vinfo_incref(v);
			PsycoTuple_GET_ITEM(newarg, i+1) = v;
		}
		arg = newarg;
	}

	im_func = get_array_item(po, methobj, METHOD_IM_FUNC);
	if (im_func == NULL)
		result = NULL;
	else
		result = PsycoObject_Call(po, im_func, arg, kw);
	vinfo_decref(arg, po);
	return result;

  fallback:
	return psyco_generic_call(po, PyMethod_Type.tp_call,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", methobj, arg, kw);
}


DEFINEFN
void psy_classobject_init()
{
	Psyco_DefineMeta(PyMethod_Type.tp_call, pinstancemethod_call);
	psyco_computed_method.compute_fn = &compute_method;
}
