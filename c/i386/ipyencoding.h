 /***************************************************************/
/***     Processor- and language-dependent code producers      ***/
 /***************************************************************/

#ifndef _IPYENCODING_H
#define _IPYENCODING_H


#include "../psyco.h"
#include "../processor.h"
#include "../dispatcher.h"

#include "../Objects/pobject.h"
#include "../Objects/pdictobject.h"


#if HAVE_struct_dictobject
# define ma_SIZE                 ma_mask
# define MA_SIZE_TO_LAST_USED    0
#else
# define ma_SIZE                 ma_size
# define MA_SIZE_TO_LAST_USED    (-1)
#endif

/* Note: the following macro must output a fixed number of bytes of
   code, so that DICT_ITEM_UPDCHANGED() can be called later
   to update an existing code buffer */
#define DICT_ITEM_IFCHANGED(code, index, key, value, jmptarget, mprg)  do {     \
  extra_assert(0 < offsetof(PyDictObject, ma_SIZE) &&                           \
                   offsetof(PyDictObject, ma_SIZE) < 128);                      \
  extra_assert(0 < offsetof(PyDictObject, ma_table) &&                          \
                   offsetof(PyDictObject, ma_table) < 128);                     \
  code[0] = 0x81;           /* CMP [...], imm32 */                              \
  code[1] = 0x40 | (7<<3) | mprg;   /* CMP [mpreg->ma_mask], ... */             \
  code[2] = offsetof(PyDictObject, ma_SIZE);                                    \
  *(long*)(code+3) = (index) - MA_SIZE_TO_LAST_USED;                            \
  /* perform the load before checking the CMP outcome */                        \
  code[7] = 0x8B;                                                               \
  code[8] = 0x40 | (mprg<<3) | mprg;   /* MOV mpreg, [mpreg->ma_table] */       \
  CODE_FOUR_BYTES(code+9,                                                       \
            offsetof(PyDictObject, ma_table),                                   \
            0x70 | CC_L,                 /* JL +22 (to 'JNE target') */         \
            34 - 12,                                                            \
            0x81);       /* CMP [mpreg+dictentry*index+me_key], key */          \
  code[13] = 0x80 | (7<<3) | mprg;                                              \
  *(long*)(code+14) = (index)*sizeof(PyDictEntry) +                             \
                                  offsetof(PyDictEntry, me_key);                \
  *(long*)(code+18) = (long)(key);                                              \
  CODE_FOUR_BYTES(code+22,                                                      \
            0x70 | CC_NE,              /* JNE +10 (to 'JNE target') */          \
            34 - 24,                                                            \
            0x81,        /* CMP [mpreg+dictentry*index+me_value], value */      \
            0x80 | (7<<3) | mprg);                                              \
  *(long*)(code+26) = (index)*sizeof(PyDictEntry) +                             \
                                  offsetof(PyDictEntry, me_value);              \
  *(long*)(code+30) = (long)(value);                                            \
  code[34] = 0x0F;                       /* JNE target */                       \
  code[35] = 0x80 | CC_NE;                                                      \
  code += 40;                                                                   \
  *(long*)(code-4) = ((code_t*)(jmptarget)) - code;                             \
} while (0)

#define DICT_ITEM_UPDCHANGED(code, index)        do {                   \
  *(long*)(code+3) = (index) - MA_SIZE_TO_LAST_USED;                    \
  *(long*)(code+14) = (index)*sizeof(PyDictEntry) +                     \
                                  offsetof(PyDictEntry, me_key);        \
  *(long*)(code+26) = (index)*sizeof(PyDictEntry) +                     \
                                  offsetof(PyDictEntry, me_value);      \
  code += 40;                                                           \
} while (0)


/* A cleaner interface to the two big macros above: quickly
   checking for a change in a dictionary of globals.
   XXX 'dict' must never be released! */
inline PsycoObject* dictitem_check_change(PsycoObject* po,
                                          code_t* onchange_target,
                                          PyDictObject* dict, PyDictEntry* ep)
{
  int index        = ep - dict->ma_table;
  PyObject* key    = ep->me_key;
  PyObject* result = ep->me_value;
  reg_t mprg;
  PsycoObject* po_copy;
  
  Py_INCREF(key);    /* XXX these become immortal */
  Py_INCREF(result); /* XXX                       */
  
  BEGIN_CODE
  NEED_CC();
  NEED_FREE_REG(mprg);
  END_CODE
		
  po_copy = PsycoObject_Duplicate(po);

  /* write code that quickly checks that the same
     object is still in place in the dictionary */
  BEGIN_CODE
  LOAD_REG_FROM_IMMED(mprg, (long) dict);
  DICT_ITEM_IFCHANGED(code, index, key, result, onchange_target, mprg);
  END_CODE

  return po_copy;
}

inline code_t* dictitem_update_nochange(code_t* originalmacrocode,
                                        PyDictObject* dict, PyDictEntry* new_ep)
{
  int index = new_ep - dict->ma_table;
  originalmacrocode += SIZE_OF_LOAD_REG_FROM_IMMED;
  DICT_ITEM_UPDCHANGED(originalmacrocode, index);
  return originalmacrocode;
}


/* emit the equivalent of the Py_INCREF() macro */
/* the PyObject* is stored in the register 'rg' */
/* XXX if Py_REF_DEBUG is set (Python debug mode), the
       following will not properly update _Py_RefTotal.
       Don't trust _Py_RefTotal with Psyco.     */
#define INC_OB_REFCNT(rg)			do {    \
  NEED_CC_REG(rg);                                      \
  INC_OB_REFCNT_internal(rg);                           \
} while (0)
/* same as above, preserving the cc */
#define INC_OB_REFCNT_CC(rg)			do {    \
  bool _save_ccreg = po->ccreg != NULL;                 \
  if (_save_ccreg) PUSH_CC_FLAGS();                     \
  INC_OB_REFCNT_internal(rg);                           \
  if (_save_ccreg) POP_CC_FLAGS();                      \
} while (0)
#define INC_OB_REFCNT_internal(rg)		do {    \
  code[0] = 0xFF;          /* INC [reg] */              \
  if ((EBP_IS_RESERVED || (rg) != REG_386_EBP) &&       \
      offsetof(PyObject, ob_refcnt) == 0)               \
    {                                                   \
      extra_assert((rg) != REG_386_EBP);                \
      code[1] = (rg);                                   \
    }                                                   \
  else                                                  \
    {                                                   \
      code++;                                           \
      extra_assert(offsetof(PyObject, ob_refcnt) < 128);\
      code[0] = 0x40 | (rg);                            \
      code[1] = (code_t) offsetof(PyObject, ob_refcnt); \
    }                                                   \
  code += 2;                                            \
} while (0)

/* Py_INCREF() for a compile-time-known 'pyobj' */
#define INC_KNOWN_OB_REFCNT(pyobj)    do {              \
  NEED_CC();                                            \
  code[0] = 0xFF;  /* INC [address] */                  \
  code[1] = 0x05;                                       \
  *(int**)(code+2) = &(pyobj)->ob_refcnt;               \
  code += 6;                                            \
 } while (0)

/* Py_DECREF() for a compile-time 'pyobj' assuming counter cannot reach zero */
#define DEC_KNOWN_OB_REFCNT_NZ(pyobj)    do {           \
  NEED_CC();                                            \
  code[0] = 0xFF;  /* DEC [address] */                  \
  code[1] = (1<<3) | 0x05;                              \
  *(int**)(code+2) = &(pyobj)->ob_refcnt;               \
  code += 6;                                            \
 } while (0)

/* like DEC_OB_REFCNT() but assume the reference counter cannot reach zero */
#define DEC_OB_REFCNT_NZ(rg)    do {                    \
  NEED_CC_REG(rg);                                      \
  code[0] = 0xFF;          /* DEC [reg] */              \
  if ((EBP_IS_RESERVED || (rg) != REG_386_EBP) &&       \
      offsetof(PyObject, ob_refcnt) == 0)               \
    {                                                   \
      extra_assert((rg) != REG_386_EBP);                \
      code[1] = 0x08 | (rg);                            \
    }                                                   \
  else                                                  \
    {                                                   \
      code++;                                           \
      extra_assert(offsetof(PyObject, ob_refcnt) < 128);\
      code[0] = 0x48 | (rg);                            \
      code[1] = (code_t) offsetof(PyObject, ob_refcnt); \
    }                                                   \
  code += 2;                                            \
} while (0)

/* internal utilities for the macros below */
EXTERNFN code_t* decref_dealloc_calling(code_t* code, PsycoObject* po, reg_t rg,
                                        destructor fn);

/* the equivalent of Py_DECREF().
   XXX Same remark as INC_OB_REFCNT().
   We correctly handle the Py_TRACE_REFS case,
   however, by calling the _Py_Dealloc() function.
   Slow but correct (and you have the debugging Python
   version anyway, so you are not looking for top speed
   but just testing things). */
#ifdef Py_TRACE_REFS
/* debugging only */
# define DEC_OB_REFCNT(rg)  (code=decref_dealloc_calling(code, po, rg,  \
                                                         _Py_Dealloc))
#else
# define DEC_OB_REFCNT(rg)  (code=decref_dealloc_calling(code, po, rg, NULL))
#endif

/* the equivalent of Py_DECREF() when we know the type of the object
   (assuming that tp_dealloc never changes for a given type) */
#ifdef Py_TRACE_REFS
/* debugging only */
# define DEC_OB_REFCNT_T(rg, type)  (code=decref_dealloc_calling(code, po, rg, \
                                                                 _Py_Dealloc))
#else
# define DEC_OB_REFCNT_T(rg, type)  (code=decref_dealloc_calling(code, po, rg, \
                                                          (type)->tp_dealloc))
#endif


/***************************************************************/
 /***   generic reference counting functions                  ***/

/* emit Py_INCREF(v) for run-time v */
inline void psyco_incref_rt(PsycoObject* po, vinfo_t* v)
{
  reg_t rg;
  BEGIN_CODE
  RTVINFO_IN_REG(v);
  rg = RUNTIME_REG(v);
  INC_OB_REFCNT(rg);
  END_CODE
}

/* emit Py_INCREF(v) */
inline bool psyco_incref_v(PsycoObject* po, vinfo_t* v)
{
  if (!compute_vinfo(v, po)) return false;
  if (!is_compiletime(v->source))
    psyco_incref_rt(po, v);
  else
    {
      BEGIN_CODE
      INC_KNOWN_OB_REFCNT((PyObject*) CompileTime_Get(v->source)->value);
      END_CODE
    }
  return true;
}

/* emit Py_DECREF(v) for run-time v. Used by vcompiler.c when releasing a
   run-time vinfo_t holding a reference to a Python object. */
inline void psyco_decref_rt(PsycoObject* po, vinfo_t* v)
{
  PyTypeObject* tp = Psyco_KnownType(v);
  reg_t rg;
  BEGIN_CODE
  RTVINFO_IN_REG(v);
  rg = RUNTIME_REG(v);
  if (tp != NULL)
    DEC_OB_REFCNT_T(rg, tp);
  else
    DEC_OB_REFCNT(rg);
  END_CODE
}

/* emit Py_DECREF(v) for any v */
inline void psyco_decref_v(PsycoObject* po, vinfo_t* v)
{
  switch (gettime(v->source)) {
    
  case RunTime:
    psyco_decref_rt(po, v);
    break;

  case CompileTime:
    BEGIN_CODE
    DEC_KNOWN_OB_REFCNT_NZ((PyObject*) CompileTime_Get(v->source)->value);
    END_CODE
    break;
  }
}


/* can eat a reference if we had one in the first place, and
   if no one else will require it (i.e. there is only one reference
   left to 'vi') */
inline bool eat_reference(vinfo_t* vi)
{
  if (has_rtref(vi->source) && vi->refcount == 1)
    {
      vi->source = remove_rtref(vi->source);
      return true;
    }
  else
    return false;
}

/* force a reference to be consumed */
EXTERNFN void consume_reference(PsycoObject* po, vinfo_t* vi);

/* make sure we have a reference on 'vi' */
inline void need_reference(PsycoObject* po, vinfo_t* vi)
{
  if ((vi->source & (TimeMask | RunTime_NoRef)) == (RunTime | RunTime_NoRef))
    {
      vi->source = add_rtref(vi->source);
      psyco_incref_rt(po, vi);
    }
}


/* to store a new reference to a Python object into a memory structure,
   use psyco_put_field() or psyco_put_field_array() to store the value
   proper and then one of the following two functions to adjust the
   reference counter: */

/* normal case */
EXTERNFN void decref_create_new_ref(PsycoObject* po, vinfo_t* w);

/* if 'w' is supposed to be freed soon, this function tries (if possible)
   to move an eventual Python reference owned by 'w' to the memory
   structure.  This avoids a Py_INCREF()/Py_DECREF() pair.
   Returns 'true' if the reference was successfully transfered;
   'false' does not mean failure. */
EXTERNFN bool decref_create_new_lastref(PsycoObject* po, vinfo_t* w);


/* called by psyco_emit_header() */
#define INITIALIZE_FRAME_LOCALS(nframelocal)   do {     \
  STACK_CORRECTION(4*((nframelocal)-1));                \
  PUSH_IMMED(0);    /* f_exc_type, initially NULL */    \
} while (0)

/* called by psyco_finish_return() */
#define FINALIZE_FRAME_LOCALS(nframelocal)     do {                     \
  CODE_FOUR_BYTES(code,                                                 \
            0x83,                                                       \
            0x3C,               /* CMP [ESP], 0 */                      \
            0x24,                                                       \
            0);                                                         \
  code[4] = 0x70 | CC_E;        /* JE exit */                           \
  code[5] = 11 - 6;                                                     \
  code[6] = 0xE8;               /* CALL cimpl_finalize_frame_locals */  \
  code += 11;                                                           \
  *(long*)(code-4) = (code_t*)(&cimpl_finalize_frame_locals) - code;    \
} while (0)

/* implemented in pycompiler.c */
EXTERNFN void cimpl_finalize_frame_locals(PyObject*, PyObject*, PyObject*);

#endif /* _IPYENCODING_H */
