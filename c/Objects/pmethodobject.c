#include "pmethodobject.h"
#include "ptupleobject.h"


static bool compute_cfunction(PsycoObject* po, vinfo_t* methobj)
{
	vinfo_t* newobj;
	vinfo_t* m_self;
	vinfo_t* m_ml;
	
	/* get the fields from the Python object 'methobj' */
	m_self = get_array_item(po, methobj, CFUNC_M_SELF);
	if (m_self == NULL)
		return false;
	m_ml = get_array_item(po, methobj, CFUNC_M_ML);
	if (m_ml == NULL)
		return false;

	/* call PyCFunction_New() */
	newobj = psyco_generic_call(po, PyCFunction_New,
				    CfPure|CfReturnRef|CfPyErrIfNull,
				    "vv", m_ml, m_self);
	if (newobj == NULL)
		return false;

	/* move the resulting non-virtual Python object back into 'methobj' */
	vinfo_move(po, methobj, newobj);
	return true;
}


DEFINEVAR source_virtual_t psyco_computed_cfunction;


 /***************************************************************/
  /*** C method objects meta-implementation                    ***/

static vinfo_t* PsycoCFunction_Call(PsycoObject* po, vinfo_t* func,
				    vinfo_t* tuple, vinfo_t* kw)
{
	vinfo_t* vml = get_array_item(po, func, CFUNC_M_ML);
	if (vml == NULL)
		return NULL;
	
	if (is_compiletime(vml->source)) {
		/* optimize only if we know which C function we are calling. */
		PyMethodDef* ml = (PyMethodDef*) \
			CompileTime_Get(vml->source)->value;
		int flags = ml->ml_flags;
		int tuplesize;
		vinfo_t* carg;

		vinfo_t* vself = get_array_item(po, func, CFUNC_M_SELF);
		if (vself == NULL)
			return NULL;

		if (flags & METH_KEYWORDS) {
			return Psyco_META3(po, ml->ml_meth,
					   CfReturnRef|CfPyErrIfNull,
					   "vvv", vself, tuple, kw);
		}
		if (!psyco_knowntobe(kw, (long) NULL))
			goto use_proxy;

		switch (flags) {
		case METH_VARARGS:
			carg = tuple;
			break;
		case METH_NOARGS:
			tuplesize = PsycoTuple_Load(tuple);
			if (tuplesize != 0)
				/* if size unknown or known to be != 0 */
				goto use_proxy;
			carg = psyco_viZero;
			break;
		case METH_O:
			tuplesize = PsycoTuple_Load(tuple);
			if (tuplesize != 1)
				/* if size unknown or known to be != 1 */
				goto use_proxy;
			carg = PsycoTuple_GET_ITEM(tuple, 0);
			break;
		default:
			goto use_proxy;
		}
		return Psyco_META2(po, ml->ml_meth, CfReturnRef|CfPyErrIfNull,
				   "vv", vself, carg);
	}

	/* default, slow version */
   use_proxy:
	return psyco_generic_call(po, PyCFunction_Call,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", func, tuple, kw);
}


DEFINEFN
void psy_methodobject_init()
{
	Psyco_DefineMeta(PyCFunction_Type.tp_call, PsycoCFunction_Call);

	psyco_computed_cfunction.compute_fn = &compute_cfunction;
}
