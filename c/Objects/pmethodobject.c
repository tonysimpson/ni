#include "pmethodobject.h"
#include "ptupleobject.h"


static bool compute_cfunction(PsycoObject* po, vinfo_t* methobj, bool forking)
{
	vinfo_t* newobj;
	vinfo_t* m_self;
	vinfo_t* m_ml;
	if (forking) return true;
	
	/* get the fields from the Python object 'methobj' */
	m_self = vinfo_getitem(methobj, iCFUNC_M_SELF);
	if (m_self == NULL)
		return false;
	m_ml = vinfo_getitem(methobj, iCFUNC_M_ML);
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

DEFINEFN
vinfo_t* PsycoCFunction_Call(PsycoObject* po, vinfo_t* func,
                             vinfo_t* tuple, vinfo_t* kw)
{
	long mllong;
	vinfo_t* vml = psyco_get_const(po, func, CFUNC_m_ml);
	if (vml == NULL)
		return NULL;

	/* promote to compile-time the function if we do not know which one
	   it is yet */
	mllong = psyco_atcompiletime(po, vml);
	if (mllong == -1) {
		/* -1 is not a valid pointer */
		extra_assert(PycException_Occurred(po));
		return NULL;
	}
	else {
		PyMethodDef* ml = (PyMethodDef*) \
			CompileTime_Get(vml->source)->value;
		int flags = ml->ml_flags;
		int tuplesize;
		vinfo_t* carg;
                char* argumentlist = "vv";

		vinfo_t* vself = psyco_get_const(po, func, CFUNC_m_self);
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
#if HAVE_METH_O
		case METH_NOARGS:
			tuplesize = PsycoTuple_Load(tuple);
			if (tuplesize != 0)
				/* if size unknown or known to be != 0 */
				goto use_proxy;
			carg = NULL;
                        argumentlist = "vl";
			break;
		case METH_O:
			tuplesize = PsycoTuple_Load(tuple);
			if (tuplesize != 1)
				/* if size unknown or known to be != 1 */
				goto use_proxy;
			carg = PsycoTuple_GET_ITEM(tuple, 0);
			break;
#endif
		default:
			goto use_proxy;
		}
		return Psyco_META2(po, ml->ml_meth, CfReturnRef|CfPyErrIfNull,
				   argumentlist, vself, carg);
	}

	/* default, slow version */
   use_proxy:
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
	return psyco_generic_call(po, PyCFunction_Call,
				  CfReturnRef|CfPyErrIfNull,
				  "vvv", func, tuple, kw);
#else
        /* no PyCFunction_Call() */
        return psyco_generic_call(po, PyEval_CallObjectWithKeywords,
                                  CfReturnRef|CfPyErrIfNull,
                                  "vvv", func, tuple, kw);
#endif
}


INITIALIZATIONFN
void psy_methodobject_init(void)
{
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
	Psyco_DefineMeta(PyCFunction_Type.tp_call, PsycoCFunction_Call);
#endif
	psyco_computed_cfunction.compute_fn = &compute_cfunction;
}
