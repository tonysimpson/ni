 /***************************************************************/
/***     Processor- and language-dependent code producers      ***/
 /***************************************************************/

#ifndef _PYCENCODING_H
#define _PYCENCODING_H


#include "psyco.h"
#include "processor.h"
#include "dispatcher.h"

#include "Objects/pobject.h"
#include "Objects/pdictobject.h"


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


/* emit the equivalent of the Py_INCREF() macro */
/* the PyObject* is stored in the register 'rg' */
#define INC_OB_REFCNT(rg)	do {                    \
  NEED_CC_REG(rg);                                      \
  extra_assert(offsetof(PyObject, ob_refcnt) == 0);     \
  code[0] = 0xFF;          /* INC [reg] */              \
  if (EBP_IS_RESERVED || (rg) != REG_386_EBP)           \
    {                                                   \
      extra_assert((rg) != REG_386_EBP);                \
      code[1] = (rg);                                   \
    }                                                   \
  else                                                  \
    {                                                   \
      code++;                                           \
      code[0] = 0x45;                                   \
      code[1] = 0;                                      \
    }                                                   \
  code += 2;                                            \
} while (0)

/* Py_INCREF() for a compile-time-known 'pyobj' */
#define INC_KNOWN_OB_REFCNT(pyobj)    do {      \
  NEED_CC();                                    \
  code[0] = 0xFF;  /* INC [address] */          \
  code[1] = 0x05;                               \
  *(int**)(code+2) = &(pyobj)->ob_refcnt;       \
  code += 6;                                    \
 } while (0)

/* Py_DECREF() for a compile-time 'pyobj' assuming counter cannot reach zero */
#define DEC_KNOWN_OB_REFCNT_NZ(pyobj)    do {   \
  NEED_CC();                                    \
  code[0] = 0xFF;  /* DEC [address] */          \
  code[1] = (1<<3) | 0x05;                      \
  *(int**)(code+2) = &(pyobj)->ob_refcnt;       \
  code += 6;                                    \
 } while (0)

/* like DEC_OB_REFCNT() but assume the reference counter cannot reach zero */
#define DEC_OB_REFCNT_NZ(rg)    do {            \
  NEED_CC_REG(rg);                              \
  code[0] = 0xFF;          /* DEC [reg] */      \
  if (EBP_IS_RESERVED || (rg) != REG_386_EBP)   \
    {                                           \
      extra_assert((rg) != REG_386_EBP);        \
      code[1] = 0x08 | (rg);                    \
    }                                           \
  else                                          \
    {                                           \
      code++;                                   \
      code[0] = 0x4D;                           \
      code[1] = 0;                              \
    }                                           \
  code += 2;                                    \
} while (0)

/* the equivalent of Py_DECREF() */
#define DEC_OB_REFCNT(rg)	do {                                            \
  DEC_OB_REFCNT_NZ(rg);                                                         \
  extra_assert(offsetof(PyObject, ob_refcnt) == 0);                             \
  extra_assert(offsetof(PyObject, ob_type) < 128);                              \
  extra_assert(offsetof(PyTypeObject, tp_dealloc) < 128);                       \
  CODE_FOUR_BYTES(code,                                                         \
            0x75,          /* JNZ rel8 */                                       \
            16 - 2,        /* to the end of this code block */                  \
            PUSH_REG_INSTR(REG_386_EAX),   /* XXX if COMPACT_ENCODING, */       \
            PUSH_REG_INSTR(REG_386_ECX));  /* XXX  avoid these PUSH    */       \
  CODE_FOUR_BYTES(code+4,                                                       \
            PUSH_REG_INSTR(REG_386_EDX),   /* XXX  when unnecessary    */       \
            PUSH_REG_INSTR(rg),                                                 \
            0x8B,          /* MOV EAX, [reg+ob_type] */                         \
            0x40 | (rg));                                                       \
  CODE_FOUR_BYTES(code+8,                                                       \
            offsetof(PyObject, ob_type),                                        \
            0xFF,         /* CALL [EAX+tp_dealloc] */                           \
            0x50,                                                               \
            offsetof(PyTypeObject, tp_dealloc));                                \
  CODE_FOUR_BYTES(code+12,                                                      \
            POP_REG_INSTR(REG_386_EDX),                                         \
            POP_REG_INSTR(REG_386_EDX),                                         \
            POP_REG_INSTR(REG_386_ECX),                                         \
            POP_REG_INSTR(REG_386_EAX));                                        \
  code += 16;                                                                   \
} while (0)

/* the same as above, when we know that the reference counter is
   reaching zero */
#define DEC_OB_REFCNT_Z(rg)    do {                             \
  --- note: this is not used ---                                \
  extra_assert(offsetof(PyObject, ob_refcnt) == 0);             \
  extra_assert(offsetof(PyObject, ob_type) < 128);              \
  extra_assert(offsetof(PyTypeObject, tp_dealloc) < 128);       \
  SAVE_REGS_FN_CALL;                                            \
  SET_REG_ADDR_TO_IMMED(rg, 0);                                 \
  code[0] = PUSH_REG_INSTR(rg);                                 \
  code[1] = 0x8B;          /* MOV EAX, [reg+ob_type] */         \
  code[2] = 0x40 | (rg);                                        \
  CODE_FOUR_BYTES(code+3,                                       \
            offsetof(PyObject, ob_type),                        \
            0xFF,         /* CALL [EAX+tp_dealloc] */           \
            0x50,                                               \
            offsetof(PyTypeObject, tp_dealloc));                \
  code += 7;                                                    \
  po->stack_depth += 4;                                         \
} while (0)

/* the equivalent of Py_DECREF() when we know the type of the object
   (assuming that tp_dealloc never changes for a given type) */
#define DEC_OB_REFCNT_T(rg, type)     do {                                      \
  DEC_OB_REFCNT_NZ(rg);                                                         \
  extra_assert(offsetof(PyObject, ob_refcnt) == 0);                             \
  CODE_FOUR_BYTES(code,                                                         \
            0x75,          /* JNZ rel8 */                                       \
            15 - 2,        /* to the end of this code block */                  \
            PUSH_REG_INSTR(REG_386_EAX),   /* XXX if COMPACT_ENCODING, */       \
            PUSH_REG_INSTR(REG_386_ECX));  /* XXX  avoid these PUSH    */       \
  code[4] = PUSH_REG_INSTR(REG_386_EDX);   /* XXX  when unnecessary    */       \
  code[5] = PUSH_REG_INSTR(rg);                                                 \
  code[6] = 0xE8;    /* CALL */                                                 \
  code += 11;                                                                   \
  *(long*)(code-4) = (code_t*)((type)->tp_dealloc) - code;                      \
  CODE_FOUR_BYTES(code,                                                         \
            POP_REG_INSTR(REG_386_EDX),                                         \
            POP_REG_INSTR(REG_386_EDX),                                         \
            POP_REG_INSTR(REG_386_ECX),                                         \
            POP_REG_INSTR(REG_386_EAX));                                        \
  code += 4;                                                                    \
} while (0)


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
  NonVirtualSource src = vinfo_compute(v, po);
  if (src == SOURCE_ERROR)
    return false;
  if (!is_compiletime(src))
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
  vinfo_t* vtp = vinfo_getitem(v, OB_TYPE);
  reg_t rg;
  BEGIN_CODE
  RTVINFO_IN_REG(v);
  rg = RUNTIME_REG(v);
  if (vtp != NULL && is_compiletime(vtp->source))
    DEC_OB_REFCNT_T(rg, (PyTypeObject*) (CompileTime_Get(vtp->source)->value));
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

/* make sure we have a reference on 'vi' */
inline void need_reference(PsycoObject* po, vinfo_t* vi)
{
  if ((vi->source & (TimeMask | RunTime_NoRef)) == (RunTime | RunTime_NoRef))
    {
      vi->source = add_rtref(vi->source);
      psyco_incref_rt(po, vi);
    }
}


/* internal utilities for the next two functions */
EXTERNFN void decref_create_new_ref(PsycoObject* po, vinfo_t* w);
EXTERNFN void decref_create_new_lastref(PsycoObject* po, vinfo_t* w);

/* write 'newitem' into the array of 'v' at position 'index',
   creating a new Python reference. If 'lastref', 'newitem' is supposed to
   be freed soon; this allows an eventual Python reference owned by 'newitem'
   to be moved to the array without having to emit any actual
   Py_INCREF()/Py_DECREF() code. */
inline bool write_array_item_ref(PsycoObject* po, vinfo_t* v, int index,
                                 vinfo_t* newitem, bool lastref)
{
	if (!write_array_item(po, v, index, newitem))
		return false;
	if (lastref)
		decref_create_new_lastref(po, newitem);
	else
		decref_create_new_ref(po, newitem);
	return true;
}
inline bool write_array_item_var_ref(PsycoObject* po, vinfo_t* vi,
				     int baseindex, vinfo_t* varindex,
				     vinfo_t* newitem, bool lastref)
{
	if (!write_array_item_var(po, vi, baseindex, varindex, newitem))
		return false;
	if (lastref)
		decref_create_new_lastref(po, newitem);
	else
		decref_create_new_ref(po, newitem);
	return true;
}


#endif /* _PYCENCODING_H */
