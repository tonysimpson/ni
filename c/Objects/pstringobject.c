#include "pstringobject.h"
#include "plistobject.h"
#include "pstructmember.h"
#include <ipyencoding.h>


static PyObject* pempty_string;


/***************************************************************/
/* string characters.                                          */
static source_virtual_t psyco_computed_char;

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
	result->array = array_new(CHARACTER_TOTAL);
	result->array->items[iOB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyString_Type)));
	result->array->items[iFIX_SIZE] = psyco_vi_One();
	result->array->items[CHARACTER_CHAR] = chrval;  assert_nonneg(chrval);
	return result;
}

DEFINEFN
vinfo_t* PsycoCharacter_New(vinfo_t* chrval)
{
	vinfo_incref(chrval);
	return PsycoCharacter_NEW(chrval);
}

inline defield_t first_character(vinfo_t* v)
{
	if (v->source == VirtualTime_New(&psyco_computed_char))
		return (defield_t) CHARACTER_CHAR;
	else
		return STR_sval;
}

DEFINEFN bool PsycoCharacter_Ord(PsycoObject* po, vinfo_t* v, vinfo_t** vord)
{
	vinfo_t* vlen;
	PyTypeObject* vtp;
	condition_code_t cc;
	vinfo_t* result = NULL;
	
	if (v->source == VirtualTime_New(&psyco_computed_char)) {
		result = vinfo_getitem(v, CHARACTER_CHAR);
		if (result != NULL) {
			vinfo_incref(result);
			goto done;
		}
	}

	vtp = Psyco_NeedType(po, v);
	if (vtp == NULL)
		return false;

	if (PyType_TypeCheck(vtp, &PyString_Type)) {
		vlen = PsycoString_GET_SIZE(po, v);
		if (vlen == NULL)
			return false;
		cc = integer_cmp_i(po, vlen, 1, Py_EQ);
		if (cc == CC_ERROR)
			return false;
		if (runtime_condition_t(po, cc))
			result = psyco_get_field(po, v, STR_sval);
	}
 done:
	*vord = result;
	return true;
}


/***************************************************************/
/* string slices.                                              */
static source_virtual_t psyco_computed_strslice;

/* a virtual string obtained as a slice of a source string STRSLICE_SOURCE.
   The slice range is defined as [STRSLICE_START:STRSLICE_START+FIX_SIZE] */

static bool compute_strslice(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* newobj;
	vinfo_t* ptr;
	vinfo_t* tmp;
	vinfo_t* source;
	vinfo_t* start;
	vinfo_t* length;
	
	source = vinfo_getitem(v, STRSLICE_SOURCE);
	start = vinfo_getitem(v, STRSLICE_START);
	length = vinfo_getitem(v, iFIX_SIZE);
	if (source==NULL || start==NULL || length==NULL)
		return false;

	tmp = integer_add(po, source, start, false);
	if (tmp == NULL)
		return false;
	ptr = integer_add_i(po, tmp, offsetof(PyStringObject, ob_sval), false);
	vinfo_decref(tmp, po);
	if (ptr == NULL)
		return false;
	
	newobj = psyco_generic_call(po, PyString_FromStringAndSize,
				    CfPure|CfReturnRef|CfPyErrIfNull,
				    "vv", ptr, length);
	vinfo_decref(ptr, po);
	if (newobj == NULL)
		return false;

	/* forget fields that were only relevant in virtual-time */
        vinfo_setitem(po, v, STRSLICE_SOURCE, NULL);
        vinfo_setitem(po, v, STRSLICE_START,  NULL);

	/* move the resulting non-virtual Python object back into 'v' */
	vinfo_move(po, v, newobj);
	return true;
}

inline vinfo_t* PsycoStrSlice_NEW(vinfo_t* source, vinfo_t* start, vinfo_t* len)
{
	/* consumes a ref to all arguments */
	vinfo_t* result = vinfo_new(VirtualTime_New(&psyco_computed_strslice));
	result->array = array_new(STRSLICE_TOTAL);
	result->array->items[iOB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyString_Type)));
	result->array->items[iFIX_SIZE]       = len;    assert_nonneg(len);
	result->array->items[STRSLICE_SOURCE] = source;
	result->array->items[STRSLICE_START]  = start;  assert_nonneg(start);
	return result;
}

static vinfo_t* strslice_real_source(PsycoObject* po, vinfo_t* s)
{
	if (s->source == VirtualTime_New(&psyco_computed_strslice)) {
		/* get a ptr to the real source memory
		   without forcing the strslice out of
		   virtual-time */
		vinfo_t* start = vinfo_getitem(s, STRSLICE_START);
		vinfo_t* src = vinfo_getitem(s, STRSLICE_SOURCE);
		if (src != NULL && start != NULL) {
			return integer_add(po, src, start, false);
		}
	}
	vinfo_incref(s);
	return s;
}


/***************************************************************/
/* string concatenations.                                      */
static source_virtual_t psyco_computed_catstr;

/* a virtual string defined as the concatenation of all the strings
   in a given list. The list must not be user-visible nor shared in
   any way, because the correct total length *must always* remain in
   FIX_SIZE.
   NB: compute_catstr is not too smart with lists of length 1, and
       does not work at all with empty lists. */

static PyObject* cimpl_concatenate(int totallen, PyObject* lst)
{
	/* C implementation -- equivalent of ''.join(lst),
	   optimized to skip checks and with a known total length */
	PyObject* result;
	extra_assert(PyList_Check(lst));

	result = PyString_FromStringAndSize(NULL, totallen);
	if (result != NULL) {
		char* dest = PyString_AS_STRING(result);
		int i, slen, count = PyList_GET_SIZE(lst);
		PyObject* s;
		
		for (i=0; i<count; i++) {
			s = PyList_GET_ITEM(lst, i);
			extra_assert(PyString_Check(s));
			slen = PyString_GET_SIZE(s);
			
			memcpy(dest, PyString_AS_STRING(s), slen);
			dest += slen;
		}
		extra_assert(dest - PyString_AS_STRING(result) == totallen);
	}
	return result;
}

static bool compute_catstr(PsycoObject* po, vinfo_t* v)
{
	int count;
	vinfo_t* s;
	vinfo_t* t;
	vinfo_t* newobj;
	int i, err;
	vinfo_t* ptr;
	vinfo_t* release_me = NULL;
	vinfo_t* list;
	vinfo_t* slen;
	
	list = vinfo_getitem(v, CATSTR_LIST);
	slen = vinfo_getitem(v, iFIX_SIZE);
	if (list==NULL || slen==NULL)
		return false;

	count = PsycoList_Load(list);
	if (count < 0) {
		/* non-virtual list */
		newobj = psyco_generic_call(po, cimpl_concatenate,
					    CfReturnRef|CfPyErrIfNull,
					    "vv", slen, list);
		if (newobj == NULL)
			return false;
		goto complete;
	}

	if (count == 2 &&
	    !is_virtualtime((s=list->array->items[VLIST_ITEMS+0])->source) &&
	    !is_virtualtime((t=list->array->items[VLIST_ITEMS+1])->source)) {
		/* optimize (for code size) the common case of the sum of two
		   non-virtual strings */
		newobj = psyco_generic_call(po,
					PyString_Type.tp_as_sequence->sq_concat,
					    CfReturnRef|CfPyErrIfNull,
					    "vv", s, t);
		if (newobj == NULL)
			return false;
		goto complete;
	}
	
	/* short virtual list: inline cimpl_concatenate */
	newobj = psyco_generic_call(po, PyString_FromStringAndSize,
				    CfReturnRef|CfPyErrIfNull,
				    "lv", (long) NULL, slen);
	if (newobj == NULL)
		return false;

	ptr = integer_add_i(po,newobj, offsetof(PyStringObject, ob_sval), false);
	if (ptr == NULL) {
	fail:  /* generic failure code */
		vinfo_xdecref(release_me, po);
		vinfo_xdecref(ptr, po);
		vinfo_decref(newobj, po);
		return false;
	}

	extra_assert(count > 0);
	i = 0;
	while (1) {
		s = list->array->items[VLIST_ITEMS + i];
		slen = psyco_get_const(po, s, FIX_size);
		if (slen == NULL)
			goto fail;
		release_me = s = strslice_real_source(po, s);
		if (s == NULL)
			goto fail;
		if (is_compiletime(slen->source)) {
			int l = CompileTime_Get(slen->source)->value;
			if (1 <= l && l <= sizeof(long)) {
				/* special-case 1-4 characters */
				bool ok;
				defield_t rdf, wdf;
				switch (l) {
				case 1: /*SIZEOF_CHAR:*/
					rdf = first_character(s);
					wdf = FMUT(DEF_ARRAY(char, 0));
					break;
				case 2: /*SIZEOF_SHORT:*/
					rdf = STR_sval2;
					wdf = FMUT(DEF_ARRAY(short, 0));
					break;
				default:
					rdf = STR_sval4;
					wdf = FMUT(DEF_ARRAY(long, 0));
					break;
					/* for 3 chars, we copy a whole word,
					   but it does not matter */
				}
				t = psyco_get_field(po, s, rdf);
				if (t == NULL)
					goto fail;
				ok = psyco_put_field(po, ptr, wdf, t);
				vinfo_decref(t, po);
				if (!ok)
					goto fail;
				goto string_done;
			}
			else if (l == 0)
				goto string_done;
		}
		else {
				/* normal case: read out of the string s */
		}
		t = integer_add_i(po, s, offsetof(PyStringObject, ob_sval),
				  false);
		if (t == NULL)
			goto fail;
		/* variable-sized memcpy */
		err = !psyco_generic_call(po, memcpy, CfNoReturnValue,
					  "vvv", ptr, t, slen);
		vinfo_decref(t, po);
		if (err)
			goto fail;

	string_done:
		vinfo_decref(release_me, po);
		release_me = NULL;
		if (++i == count)
			break;  /* done */
		t = integer_add(po, ptr, slen, false);
		vinfo_decref(ptr, po);
		ptr = t;
		if (ptr == NULL)
			goto fail;
	}
	vinfo_decref(ptr, po);

 complete:
	/* forget fields that were only relevant in virtual-time */
        vinfo_setitem(po, v, CATSTR_LIST, NULL);

	/* move the resulting non-virtual Python object back into 'v' */
	vinfo_move(po, v, newobj);
	return true;
}

inline vinfo_t* PsycoCatStr_NEW(PsycoObject* po,
				vinfo_t* totallength, vinfo_t* list)
{
	/* consumes a ref to the arguments */

	vinfo_t* result;
	int count = PsycoList_Load(list);
	if (count == 1) {
		/* optimize if 'list' contains just one string */
		result = list->array->items[VLIST_ITEMS + 0];
		if (Psyco_KnownType(result) == &PyString_Type) {
			vinfo_incref(result);
			/* XXX store totallength in result? */
			vinfo_decref(totallength, po);
			vinfo_decref(list, po);
			return result;
		}
	}
	result = vinfo_new(VirtualTime_New(&psyco_computed_catstr));
	result->array = array_new(CATSTR_TOTAL);
	result->array->items[iOB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyString_Type)));
	result->array->items[iFIX_SIZE]= totallength; assert_nonneg(totallength);
	result->array->items[CATSTR_LIST] = list;
	return result;
}

inline vinfo_t* pstring_as_catlist(vinfo_t* vs)
{
	/* the resulting list is guaranteed to be of length >= 1 */
	vinfo_t* list;
	if (vs->source == VirtualTime_New(&psyco_computed_catstr)) {
		list = vs->array->items[CATSTR_LIST];
		vinfo_incref(list);
	}
	else {
		list = PsycoList_SingletonNew(vs);
	}
	return list;
}


 /***************************************************************/
  /*** string objects meta-implementation                      ***/

static vinfo_t* pstring_item(PsycoObject* po, vinfo_t* a, vinfo_t* i)
{
	condition_code_t cc;
	vinfo_t* vlen;
        vinfo_t* result;

	vlen = psyco_get_const(po, a, FIX_size);
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
	assert_nonneg(i);

	if (psyco_knowntobe(vlen, 1) &&
	    Psyco_KnownType(a) == &PyString_Type) {
		/* if a is known to be a single character, return a */
		vinfo_incref(a);
		return a;
	}
	
	result = psyco_get_field_array(po, a, STR_sval, i);
	if (result == NULL)
		return NULL;

	return PsycoCharacter_NEW(result);
}

static vinfo_t* pstring_slice(PsycoObject* po, vinfo_t* a,
			      vinfo_t* i, vinfo_t* j)
{
	condition_code_t cc;
	vinfo_t* vlen;
	vinfo_t* slicelen;
	vinfo_t* slicestart = NULL;
	vinfo_t* result = NULL;

	vlen = psyco_get_const(po, a, FIX_size);
	if (vlen == NULL)
		return NULL;

	cc = integer_cmp(po, j, vlen, Py_GE|COMPARE_UNSIGNED);
	if (cc == CC_ERROR)
		goto fail;
	if (runtime_condition_f(po, cc)) {
		/* j < 0 or j >= vlen */
		cc = integer_cmp_i(po, j, 0, Py_LT);
		if (cc == CC_ERROR)
			goto fail;
		if (runtime_condition_f(po, cc)) {
			/* j < 0 */
			return vinfo_new(CompileTime_New((long) pempty_string));
		}
		else {
			/* j >= vlen */
			j = vlen;

			/* this also tracks the case where j==vlen in the
			   first place, for the following test */
			cc = integer_cmp_i(po, i, 0, Py_LE);
			if (cc == CC_ERROR)
				goto fail;
			if (runtime_condition_f(po, cc)) {
				/* i <= 0 */
				if (Psyco_KnownType(a) == &PyString_Type) {
					/* a[0:len(a)] */
					vinfo_incref(a);
					return a;
				}
			}
			else
				assert_nonneg(i);
		}
	}
        assert_nonneg(j);

	cc = integer_cmp(po, i, j, Py_GT|COMPARE_UNSIGNED);
	if (cc == CC_ERROR)
		goto fail;
	if (runtime_condition_f(po, cc)) {
		/* i < 0 or i > j */
		cc = integer_cmp_i(po, i, 0, Py_LT);
		if (cc == CC_ERROR)
			goto fail;
		if (runtime_condition_f(po, cc)) {
			/* i < 0 */
			i = psyco_vi_Zero();
		}
		else {
			assert_nonneg(i);
			/* i > j */
			return vinfo_new(CompileTime_New((long) pempty_string));
		}
	}
	else {
		assert_nonneg(i);
		vinfo_incref(i);
	}
	slicestart = i;

	/* at this point we have 0 <= i <= j <= vlen.
	   We don't try to capture the case i==j and special-case it to
	   an empty string because it might create uninteresting extra paths
	   for looping algorithms. */

	if (a->source == VirtualTime_New(&psyco_computed_strslice)) {
		vinfo_t* source = vinfo_getitem(a, STRSLICE_SOURCE);
		vinfo_t* start = vinfo_getitem(a, STRSLICE_START);
		if (source!=NULL && start!=NULL) {
			/* optimize slicing of a slice */
			vinfo_t* k = integer_add(po, start, slicestart, false);
			if (k == NULL)
				goto fail;
			vinfo_decref(slicestart, po);
			slicestart = k;
			a = source;
		}
	}
	
	slicelen = integer_sub(po, j, i, false);
	if (slicelen == NULL)
		goto fail;
	vinfo_incref(a);
	need_reference(po, a);
	return PsycoStrSlice_NEW(a, slicestart, slicelen);
	
 fail:
	vinfo_xdecref(slicestart, po);
	return result;
}

static vinfo_t* pstring_concat(PsycoObject* po, vinfo_t* a, vinfo_t* b)
{
	PyTypeObject* btp = Psyco_NeedType(po, b);
	if (btp == NULL)
		return NULL;

	if (PyType_TypeCheck(btp, &PyString_Type)) {
		/* we build a virtual catstr. Each of a or b may already
		   be a catstr. We convert a and b to a list
		   representation (of length 1 if they are not already
		   catstr's) and concatenate the two lists.
		   Some more optimizations are done by plist_concat(). */
		vinfo_t* lena;
		vinfo_t* lenb;
		vinfo_t* lenc;
		vinfo_t* lista;
		vinfo_t* listb;
		vinfo_t* listc;
		int listalen;
		int listblen;
		vinfo_t* vlasta;
		vinfo_t* vfirstb;

		lena = psyco_get_const(po, a, FIX_size);
		if (lena == NULL)
			return NULL;
		if (psyco_knowntobe(lena, 0) && btp == &PyString_Type) {
			/* ('' + b) is b */
			vinfo_incref(b);
			return b;
		}

		lenb = psyco_get_const(po, b, FIX_size);
		if (lenb == NULL)
			return NULL;
		if (psyco_knowntobe(lenb, 0) &&
		    Psyco_KnownType(a) == &PyString_Type) {
			/* (a + '') is a */
			vinfo_incref(a);
			return a;
		}

		lenc = integer_add(po, lena, lenb, false);
		if (lenc == NULL)
			return NULL;

		lista = pstring_as_catlist(a);
		listb = pstring_as_catlist(b);

		/* if lista ends in a constant string and listb starts
		   in a constant string, concatenate them now */
		if ((listalen=PsycoList_Load(lista)) > 0 &&
		    is_compiletime(
			(vlasta=lista->array->items[VLIST_ITEMS + listalen - 1])
			->source) &&
		    (listblen=PsycoList_Load(listb)) > 0 &&
		    is_compiletime(
			(vfirstb=listb->array->items[VLIST_ITEMS + 0])
			->source)) {

			vinfo_t* buffer[VLIST_LENGTH_MAX*2-1];
			vinfo_t* v;
			PyObject* s1 = (PyObject*)
				CompileTime_Get(vlasta->source)->value;
			PyObject* s2 = (PyObject*)
				CompileTime_Get(vfirstb->source)->value;
			Py_INCREF(s1);
			PyString_Concat(&s1, s2);
			if (s1 == NULL) {
				psyco_virtualize_exception(po);
				goto fail;
			}
			v = vinfo_new(CompileTime_NewSk(
				sk_new((long) s1, SkFlagPyObj)));

			memcpy(buffer,
			       lista->array->items + VLIST_ITEMS,
			       (listalen-1) * sizeof(vinfo_t*));
			buffer[listalen-1] = v;
			memcpy(buffer + listalen,
			       listb->array->items + VLIST_ITEMS + 1,
			       (listblen-1) * sizeof(vinfo_t*));

			listc = PsycoList_New(po, listalen+listblen-1, buffer);
			vinfo_decref(v, po);
		}
		else {
			/* general case */
			/* shortcut: direct call to psyco_plist_concat() */
			listc = psyco_plist_concat(po, lista, listb);
		}
		if (listc == NULL)
			goto fail;
		vinfo_decref(listb, po);
		vinfo_decref(lista, po);
		return PsycoCatStr_NEW(po, lenc, listc);

	 fail:
		vinfo_decref(listb, po);
		vinfo_decref(lista, po);
		vinfo_decref(lenc, po);
		return NULL;

	}

	/* fallback */
	return psyco_generic_call(po, PyString_Type.tp_as_sequence->sq_concat,
				  CfReturnRef|CfPyErrIfNull,
				  "vv", a, b);
}

/* higher-level meta-version of memcmp(): compares the data in string 'v' of length
   'vlen' with the data in string 'w' of length 'wlen'.
   Return 0 (no), 1 (yes), or -1 (error). */
inline int psyco_richmemcmp(PsycoObject* po, vinfo_t* v, vinfo_t* w,
			    vinfo_t* vlen, vinfo_t* wlen, int op)
{
	condition_code_t cc;
	vinfo_t* minlen;
	bool same_size;
	int result;
	vinfo_t* vresult;
	vinfo_t* vtest;
	vinfo_t* wtest;

	/* first compare the sizes */
	cc = integer_cmp(po, vlen, wlen, Py_EQ);
	if (cc == CC_ERROR)
		return -1;
	same_size = runtime_condition_t(po, cc);
	if (same_size) {
		/* same length, but better put in 'minlen' the compile-time
		   value if there is one */
		if (is_compiletime(wlen->source))
			minlen = wlen;
		else
			minlen = vlen;
		if (psyco_knowntobe(minlen, 0))
			return (op == Py_EQ || op == Py_LE || op == Py_GE);
	}
	else {
		/* different sizes */
		switch (op) {
		case Py_EQ:
			return 0;
		case Py_NE:
			return 1;
		default:
			break;
		}
		if (psyco_knowntobe(vlen, 0))
			return (op == Py_LT || op == Py_LE);
		if (psyco_knowntobe(wlen, 0))
			return (op == Py_GT || op == Py_GE);
		
		cc = integer_cmp(po, vlen, wlen, Py_LT);
		if (cc == CC_ERROR)
			return -1;
		if (runtime_condition_t(po, cc)) {
			/* len(v) < len(w) */
			minlen = vlen;
			/*  (v < w)   iff  (v[:minlen] <= w[:minlen])  */
			/*  (v <= w)  iff  (v[:minlen] <= w[:minlen])  */
			/*  (v > w)   iff  (v[:minlen]  > w[:minlen])  */
			/*  (v >= w)  iff  (v[:minlen]  > w[:minlen])  */
			switch (op) {
			case Py_LT: op = Py_LE; break;
			case Py_GE: op = Py_GT; break;
			default:                break;
			}
		}
		else {
			/* len(v) > len(w) */
			minlen = wlen;
			/*  (v < w)   iff  (v[:minlen] <  w[:minlen])  */
			/*  (v <= w)  iff  (v[:minlen] <  w[:minlen])  */
			/*  (v > w)   iff  (v[:minlen] >= w[:minlen])  */
			/*  (v >= w)  iff  (v[:minlen] >= w[:minlen])  */
			switch (op) {
			case Py_LE: op = Py_LT; break;
			case Py_GT: op = Py_GE; break;
			default:                break;
			}
		}
	}

	/* optimize slices */
	w = strslice_real_source(po, w);
	v = strslice_real_source(po, v);
	if (v == NULL || w == NULL)
		goto fail1;
	
	/* is 'minlen' a known and small value? */
	if (is_compiletime(minlen->source)) {
		defield_t vrdf, wrdf;
		switch (CompileTime_Get(minlen->source)->value) {
		case 1: /*SIZEOF_CHAR:*/
			vrdf = first_character(v);
			wrdf = first_character(w);
			break;
			
		case 2: /*SIZEOF_SHORT:*/
			if (op != Py_EQ && op != Py_NE) goto use_memcmp;
			vrdf = wrdf = STR_sval2;
			break;

		case 4: /*SIZEOF_LONG:*/
			if (op != Py_EQ && op != Py_NE) goto use_memcmp;
			vrdf = wrdf = STR_sval4;
			break;

		default:
			goto use_memcmp;
		}

		/* load all the characters of the (short) strings
		   into registers */
		vtest = psyco_get_field(po, v, vrdf);
		wtest = psyco_get_field(po, w, wrdf);
		if (vtest == NULL || wtest == NULL)
			goto fail2;
		cc = integer_cmp(po, vtest, wtest, op);
		vresult = NULL;
	}
	else {
	   use_memcmp:
		/* compare the 'minlen' first bytes of the data blocks */
		vtest = integer_add_i(po, v, offsetof(PyStringObject, ob_sval),
				      false);
		wtest = integer_add_i(po, w, offsetof(PyStringObject, ob_sval),
				      false);
		if (vtest == NULL || wtest == NULL)
			goto fail2;
		/* variable-sized memcmp */
		vresult = psyco_generic_call(po, memcmp, CfReturnNormal,
					     "vvv", vtest, wtest, minlen);
		cc = integer_cmp_i(po, vresult, 0, op);
	}
	
	if (cc == CC_ERROR)
		result = -1;
	else
		result = runtime_condition_t(po, cc);
	vinfo_xdecref(vresult, po);
	vinfo_decref(wtest, po);
	vinfo_decref(vtest, po);
	vinfo_decref(v, po);
	vinfo_decref(w, po);
	return result;

 fail2:
	vinfo_xdecref(wtest, po);
	vinfo_xdecref(vtest, po);
 fail1:
	vinfo_xdecref(v, po);
	vinfo_xdecref(w, po);
	return -1;
}

static vinfo_t* pstring_richcompare(PsycoObject* po, vinfo_t* v, vinfo_t* w, int op)
{
	int result;
	
	/* Make sure both arguments are strings. */
	PyTypeObject *tp = Psyco_FastType(v);
	if (!PyType_TypeCheck(tp, &PyString_Type)) {
		return psyco_vi_NotImplemented();
	}
	tp = Psyco_NeedType(po, w);
	if (tp == NULL)
		return NULL;
	if (!PyType_TypeCheck(tp, &PyString_Type)) {
		return psyco_vi_NotImplemented();
	}

	if (vinfo_known_equal(v, w)) {
		/* identical strings */
		result = (op == Py_EQ || op == Py_LE || op == Py_GE);
	}
	else {
		vinfo_t* vlen;
		vinfo_t* wlen;
		/* get the sizes */
		wlen = psyco_get_const(po, w, FIX_size);
		if (wlen == NULL)
			return NULL;
		vlen = psyco_get_const(po, v, FIX_size);
		if (vlen == NULL)
			return NULL;
		
		result = psyco_richmemcmp(po, v, w, vlen, wlen, op);
	}
	
	switch (result) {
	case 0:
		return psyco_vi_Py_False();
	case 1:
		return psyco_vi_Py_True();
	default:
		return NULL;
	}
}


INITIALIZATIONFN
void psy_stringobject_init(void)
{
	PyMappingMethods *mm;
	PySequenceMethods *m = PyString_Type.tp_as_sequence;
	Psyco_DefineMeta(m->sq_length, psyco_generic_immut_ob_size);
	Psyco_DefineMeta(m->sq_item, pstring_item);
	Psyco_DefineMeta(m->sq_slice, pstring_slice);
	Psyco_DefineMeta(m->sq_concat, pstring_concat);
        Psyco_DefineMeta(PyString_Type.tp_richcompare, pstring_richcompare);

	mm = PyString_Type.tp_as_mapping;
	if (mm) {  /* Python >= 2.3 */
		Psyco_DefineMeta(mm->mp_subscript, psyco_generic_subscript);
	}

        INIT_SVIRTUAL(psyco_computed_char, compute_char, 0, 0);
        INIT_SVIRTUAL(psyco_computed_strslice, compute_strslice,
                      NW_STRSLICES_NORMAL, NW_STRSLICES_FUNCALL);
        INIT_SVIRTUAL(psyco_computed_catstr, compute_catstr,
                      NW_CATSTRS_NORMAL, NW_CATSTRS_FUNCALL);

	pempty_string = PyString_FromString("");
}
