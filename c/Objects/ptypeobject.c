#include "ptypeobject.h"
#include "ptupleobject.h"


#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */

/***************************************************************/
  /*** type objects meta-implementation                        ***/

static int cimpl_call_tp_init(PyTypeObject* type, PyObject* obj,
			      PyObject* args, PyObject* kwds)
{
	/* If the returned object is not an instance of type,
	   it won't be initialized. (Python 2.3 behavior) */
	if (!PyType_IsSubtype(obj->ob_type, type))
		return 0;
	type = obj->ob_type;
	if (PyType_HasFeature(type, Py_TPFLAGS_HAVE_CLASS) &&
	    type->tp_init != NULL)
		return type->tp_init(obj, args, kwds);
	return 0;
}

static vinfo_t* ptype_call(PsycoObject* po, vinfo_t* vtype,
			   vinfo_t* varg, vinfo_t* vkw)
{
	vinfo_t* vobj;
	PyTypeObject* type;
	PyTypeObject* otype;
	type = (PyTypeObject*) psyco_pyobj_atcompiletime(po, vtype);
	if (type == NULL)
		return NULL;
	if (type->tp_new == NULL)
		goto fallback;

	/* Ugly exception: if the call is type(o),
	   just return the type of 'o'. */
	if (type == &PyType_Type) {
		int nb_args;
		if (!psyco_knowntobe(vkw, (long) NULL))
			goto fallback;
		nb_args = PsycoTuple_Load(varg);
		if (nb_args == 1) {
			vinfo_t* v = PsycoTuple_GET_ITEM(varg, 0);
			v = get_array_item(po, v, OB_TYPE);
			vinfo_incref(v);
			return v;
		}
		if (nb_args < 0)
			goto fallback;
	}

	vobj = Psyco_META3(po, type->tp_new,
			   CfReturnRef|CfPyErrIfNull, "lvv",
			   type, varg, vkw);
	if (vobj == NULL)
		return NULL;

	otype = Psyco_NeedType(po, vobj);
	if (otype == NULL) {
		/* unknown return type, cannot promote it to compile-time
		   now because 'vobj' is not yet stored in 'po->vlocals'.
		   XXX check again why this wouldn't work */
		PycException_Clear(po);
		if (!psyco_generic_call(po, cimpl_call_tp_init,
					CfNoReturnValue|CfPyErrIfNeg,
					"vvvv", vtype, vobj, varg, vkw))
			goto error;
		return vobj;
	}
	
	/* If the returned object is not an instance of type,
	   it won't be initialized. (Python 2.3 behavior) */
	if (PyType_IsSubtype(otype, type) &&
	    PyType_HasFeature(otype, Py_TPFLAGS_HAVE_CLASS) &&
	    otype->tp_init != NULL) {
		if (!Psyco_META3(po, otype->tp_init,
				 CfNoReturnValue|CfPyErrIfNeg,
				 "vvv", vobj, varg, vkw))
			goto error;
	}
	return vobj;

 error:
	vinfo_decref(vobj, po);
	return NULL;

 fallback:
	return psyco_generic_call(po, PyType_Type.tp_call,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", vtype, varg, vkw);
}

#endif  /* NEW_STYLE_TYPES */


INITIALIZATIONFN
void psy_typeobject_init(void)
{
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
	Psyco_DefineMeta(PyType_Type.tp_call, ptype_call);
#endif
}
