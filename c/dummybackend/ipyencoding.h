#ifndef _IPYENCODING_H
#define _IPYENCODING_H

#include "../processor.h"
#include "../dispatcher.h"

#include "../Objects/pobject.h"
#include "../Objects/pdictobject.h"

#define INITIALIZE_FRAME_LOCALS(nframelocal)
#define WRITE_FRAME_EPILOGUE(retval, nframelocal) 

static void psyco_incref_rt(PsycoObject* po, vinfo_t* v)
{
}

static void psyco_incref_nv(PsycoObject* po, vinfo_t* v)
{
}

static void psyco_decref_rt(PsycoObject* po, vinfo_t* v)
{
}

static void psyco_decref_c(PsycoObject* po, PyObject* o)
{
}

/* to store a new reference to a Python object into a memory structure,
 *    use psyco_put_field() or psyco_put_field_array() to store the value
 *       proper and then one of the following two functions to adjust the
 *          reference counter: */

/* normal case */
EXTERNFN void decref_create_new_ref(PsycoObject* po, vinfo_t* w);

/* if 'w' is supposed to be freed soon, this function tries (if possible)
 *    to move an eventual Python reference owned by 'w' to the memory
 *       structure.  This avoids a Py_INCREF()/Py_DECREF() pair.
 *          Returns 'true' if the reference was successfully transfered;
 *             'false' does not mean failure. */
EXTERNFN bool decref_create_new_lastref(PsycoObject* po, vinfo_t* w);


/* A cleaner interface to the two big macros above: quickly
   checking if a globals' dictionary still map the given key to
   the given value.
   XXX 'dict' must never be released! */
static void* dictitem_check_change(PsycoObject* po,
                                   PyDictObject* dict, PyDictEntry* ep)
{
  return NULL;
}

static void dictitem_update_nochange(void* originalmacrocode,
                                     PyDictObject* dict, PyDictEntry* new_ep)
{
}
#define DICT_ITEM_CHECK_CC     CC_NE
#endif
