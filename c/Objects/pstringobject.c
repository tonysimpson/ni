#include "pstringobject.h"



/***************************************************************/
/* string characters.
   Should be changed to arbitrary string slices. */
static source_virtual_t psyco_computed_char;

#define CHARACTER_CHAR     STR_OB_SVAL

static PyObject* cimpl_character(char c)
{
	return PyString_FromStringAndSize(&c, 1);
}

static bool compute_char(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* chrval;
	vinfo_t* newobj;

	chrval = vinfo_getitem(v, CHARACTER_CHAR);
	if (chrval == NULL)
		return false;

	newobj = psyco_generic_call(po, cimpl_character,
				    CfPure|CfReturnRef|CfPyErrIfNull,
				    "v", chrval);
	if (newobj == NULL)
		return false;

	/* move the resulting non-virtual Python object back into 'v' */
	vinfo_move(po, v, newobj);
	return true;
}


inline vinfo_t* PsycoCharacter_NEW(vinfo_t* chrval)
{
	/* consumes a ref to 'chrval' */
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_char));
	result->array = array_new(CHARACTER_CHAR+1);
	result->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyString_Type)));
	result->array->items[VAR_OB_SIZE] = psyco_vi_One();
	result->array->items[CHARACTER_CHAR] = chrval;
	return result;
}

DEFINEFN
vinfo_t* PsycoCharacter_New(vinfo_t* chrval)
{
	vinfo_incref(chrval);
	return PsycoCharacter_NEW(chrval);
}


 /***************************************************************/
  /*** string objects meta-implementation                      ***/

static vinfo_t* pstring_item(PsycoObject* po, vinfo_t* a, vinfo_t* i)
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
				       "string index out of range");
		return NULL;
	}
	
	result = read_immut_array_item_var(po, a, STR_OB_SVAL, i, true);
	if (result == NULL)
		return NULL;

	return PsycoCharacter_NEW(result);
}


INITIALIZATIONFN
void psy_stringobject_init(void)
{
	PySequenceMethods *m = PyString_Type.tp_as_sequence;
	Psyco_DefineMeta(m->sq_length, psyco_generic_immut_ob_size);
	Psyco_DefineMeta(m->sq_item, pstring_item);
	psyco_computed_char.compute_fn = &compute_char;
}
