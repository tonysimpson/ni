#include "codemanager.h"

#ifdef STORE_CODE_END
# define SET_CODE_END(proxy)    (b->codeend = (proxy) ? b->codeptr : NULL,   \
                                 b->codemode = (proxy) ? "proxy" : "(compiling)")
#else
# define SET_CODE_END(proxy)    /* nothing */
#endif

#define NEW_CODE_BUFFER(extrasize, codepointer, setcodelimit)   \
{                                                               \
  PyObject* o;                                                  \
  CodeBufferObject* b;                                          \
  psyco_trash_object(NULL);                                     \
                                                                \
  /* PyObject_New is inlined */                                 \
  o = PyObject_MALLOC(sizeof(CodeBufferObject) + (extrasize));  \
  if (o == NULL)                                                \
    return (CodeBufferObject*) PyErr_NoMemory();                \
  b = (CodeBufferObject*) PyObject_INIT(o, &CodeBuffer_Type);   \
  b->codeptr = (codepointer);                                   \
  SET_CODE_END(!setcodelimit);                                  \
  if (VERBOSE_LEVEL > 2)                                        \
    debug_printf(("psyco: %s code buffer %p\n",                 \
         extrasize ? "new" : "proxy", b->codeptr));             \
                                                                \
  fpo_mark_new(&b->snapshot);                                   \
  if (po == NULL)                                               \
    fpo_mark_unused(&b->snapshot);                              \
  else                                                          \
    {                                                           \
      assert(!is_respawning(po));                               \
      fpo_build(&b->snapshot, po);                              \
      if (ge != NULL)                                           \
        register_codebuf(ge, b);                                \
      if (setcodelimit)                                         \
        po->codelimit = b->codeptr + BIG_BUFFER_SIZE -          \
                                     GUARANTEED_MINIMUM;        \
      po->respawn_cnt = 0;                                      \
      po->respawn_proxy = b;                                    \
    }                                                           \
  return b;                                                     \
}

DEFINEFN
CodeBufferObject* psyco_new_code_buffer(PsycoObject* po, global_entries_t* ge)
     NEW_CODE_BUFFER(BIG_BUFFER_SIZE,                                           \
                     ((code_t*)(b+1)),   /* buffer is after the object */       \
                     1)

DEFINEFN
CodeBufferObject* psyco_proxy_code_buffer(PsycoObject* po, global_entries_t* ge)
     NEW_CODE_BUFFER(0,                                         \
                     po->code,  /* buffer is elsewhere */       \
                     0)

#if 0    /* not used in this version */
DEFINEFN
CodeBufferObject* psyco_new_code_buffer_size(int size)
{
  PyObject* o;
  CodeBufferObject* b;
  
  /* PyObject_New is inlined */
  o = PyObject_MALLOC(sizeof(CodeBufferObject) + size);
  if (o == NULL)
    return (CodeBufferObject*) PyErr_NoMemory();
  b = (CodeBufferObject*) PyObject_INIT(o, &CodeBuffer_Type);
  b->codeptr = (code_t*) (b + 1);
  b->po = NULL;
  if (VERBOSE_LEVEL > 2)
    debug_printf(("psyco: new_code_buffer_size(%d) %p\n", size, b->codeptr));
  return b;
}
#endif

#if CODE_DUMP
DEFINEVAR CodeBufferObject* psyco_codebuf_chained_list = NULL;
DEFINEVAR void** psyco_codebuf_spec_dict_list = NULL;
#endif

DEFINEFN
void psyco_shrink_code_buffer(CodeBufferObject* obj, int nsize)
{
  void* ndata;
  extra_assert(0 < nsize && nsize <= BIG_BUFFER_SIZE - GUARANTEED_MINIMUM);
  ndata = PyObject_REALLOC(obj, sizeof(CodeBufferObject) + nsize);
  //printf("psyco: shrink_code_buffer %p to %d\n", obj->codeptr, nsize);
  if (VERBOSE_LEVEL > 2)
    debug_printf(("psyco: disassemble %p %p    (%d bytes)\n", obj->codeptr,
                  obj->codeptr + nsize, nsize));
  else if (VERBOSE_LEVEL > 1)
    debug_printf(("[%d]", nsize));
  assert(ndata == obj);   /* don't know what to do if this is not the case */
#ifdef STORE_CODE_END
  extra_assert(obj->codeend == NULL);
  obj->codeend = obj->codeptr + nsize;
  obj->codemode = "normal";
#endif
#if CODE_DUMP
  obj->chained_list = psyco_codebuf_chained_list;
  psyco_codebuf_chained_list = obj;
#endif
}

/* int psyco_tie_code_buffer(PsycoObject* po) */
/* { */
/*   CodeBufferObject* b = po->respawn_proxy; */
/*   global_entries_t* ge = GET_UNUSED_SNAPSHOT(b->snapshot); */
/*   if (psyco_snapshot(b, po, ge)) */
/*     return -1; */
/*   po->respawn_cnt = 0; */
/*   return 0; */
/* } */


/*****************************************************************/

static PyObject* trashed = NULL;

DEFINEFN
void psyco_trash_object(PyObject* obj)
{
  Py_XDECREF(trashed);
  trashed = obj;
}


/*****************************************************************/

static PyObject* codebuf_repr(CodeBufferObject* self)
{
  char buf[100];
  sprintf(buf, "<code buffer ptr %p at %p>",
	  self->codeptr, self);
  return PyString_FromString(buf);
}

static void codebuf_dealloc(CodeBufferObject* self)
{
#if CODE_DUMP
  CodeBufferObject** ptr = &psyco_codebuf_chained_list;
  void** chain;
  while (*ptr != NULL)
    {
      if (*ptr == self)
        {
          *ptr = self->chained_list;
          break;
        }
      ptr = &((*ptr)->chained_list);
    }
  for (chain = psyco_codebuf_spec_dict_list; chain; chain=(void**)*chain)
    if (self->codeptr < (code_t*)chain && (code_t*)chain <= self->codeend)
      assert(!"releasing a code buffer with a spec_dict");
#endif
  if (VERBOSE_LEVEL > 2)
    debug_printf(("psyco: releasing code buffer %p at %p\n",
                  self->codeptr, self));
  fpo_release(&self->snapshot);
  PyObject_Del(self);
}

DEFINEVAR
PyTypeObject CodeBuffer_Type = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size*/
	"CodeBuffer",		/*tp_name*/
	sizeof(CodeBufferObject),	/*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)codebuf_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	0,			/*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	(reprfunc)codebuf_repr,	/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
	0,			/*tp_call*/
};
