#include "codemanager.h"


#define BUFFER_SIGNATURE    0xE673B506   /* arbitrary */

typedef struct codemanager_buf_s {
  long signature;
  char* position;
  bool inuse;
  struct codemanager_buf_s* next;
} codemanager_buf_t;

static codemanager_buf_t* big_buffers = NULL;


inline void check_signature(codemanager_buf_t* b)
{
  if (b->signature != BUFFER_SIGNATURE)
    Py_FatalError("psyco: code buffer overwrite detected");
}

inline code_t* get_next_buffer(code_t** limit)
{
  char* p;
  codemanager_buf_t* b;
  codemanager_buf_t** bb;
  int count;
  for (b=big_buffers; b!=NULL; b=b->next)
    {
      check_signature(b);
      if (!b->inuse)
        {   /* returns the first (oldest) unlocked buffer */
          b->inuse = true;
          *limit = ((code_t*) b) - GUARANTEED_MINIMUM;
          return (code_t*) b->position;
        }
    }
  /* no more free buffer, allocate a new one */
  p = (char*) PyCore_MALLOC(BIG_BUFFER_SIZE);
  if (!p)
    return (code_t*) PyErr_NoMemory();

  /* the codemanager_buf_t structure is put at the end of the buffer,
     with its signature to detect overflows (just in case) */
  b = (codemanager_buf_t*) (p + BIG_BUFFER_SIZE - sizeof(codemanager_buf_t));
  b->signature = BUFFER_SIGNATURE;
  b->position = p;
  b->inuse = true;
  b->next = NULL;

  /* insert 'b' at the end of the chained list */
  count = 0;
  for (bb=&big_buffers; *bb!=NULL; bb=&(*bb)->next)
    count++;
  if (count > WARN_TOO_MANY_BUFFERS)
    fprintf(stderr, "psyco: warning: detected many (%d) buffers in use\n", count);
  *bb = b;
  *limit = ((code_t*) b) - GUARANTEED_MINIMUM;
  return (code_t*) p;
}

DEFINEFN int psyco_locked_buffers(void)
{
  codemanager_buf_t* b;
  int count = 0;
  for (b=big_buffers; b!=NULL; b=b->next)
    if (b->inuse)
      count++;
  return count;
}

inline void close_buffer_use(code_t* code)
{
  codemanager_buf_t* b;
  for (b=big_buffers; b!=NULL; b=b->next)
    {
      check_signature(b);
      if (b->position <= (char*) code && (char*) code <= (char*) b)
        {
          extra_assert(b->inuse);
          ALIGN_NO_FILL();
          if (code < ((code_t*) b) - BUFFER_MARGIN)
            {
              /* unlock the buffer */
              b->position = code;
              b->inuse = false;
            }
          else
            {
              /* buffer nearly full, remove it from the chained list */
              codemanager_buf_t** bb;
              for (bb=&big_buffers; *bb!=b; bb=&(*bb)->next)
                ;
              *bb = b->next;
            }
          return;
        }
    }
  Py_FatalError("psyco: code buffer allocator corruption");
}


static CodeBufferObject* new_code_buffer(PsycoObject* po, global_entries_t* ge,
                                         code_t* proxy_to)
{
  CodeBufferObject* b;
  code_t* limit;
  psyco_trash_object(NULL);

  b = PyObject_New(CodeBufferObject, &CodeBuffer_Type);
  if (b == NULL)
    return NULL;
  if (proxy_to != NULL)
    {
      limit = NULL;
      b->codeptr = proxy_to;  /* points inside another code buffer */
#ifdef STORE_CODE_END
      b->codeend = proxy_to;
      b->codemode = "proxy";
#endif
    }
  else
    {
      /* start a new code buffer */
      b->codeptr = get_next_buffer(&limit);
      if (b->codeptr == NULL)
        {
          Py_DECREF(b);
          return NULL;
        }
#ifdef STORE_CODE_END
      b->codeend = NULL;
      b->codemode = "(compiling)";
#endif
    }
  
  if (VERBOSE_LEVEL > 2)
    debug_printf(("psyco: %s code buffer %p\n",
                  proxy_to==NULL ? "new" : "proxy", b->codeptr));
  
  fpo_mark_new(&b->snapshot);
  if (po == NULL)
    fpo_mark_unused(&b->snapshot);
  else
    {
      if (is_respawning(po))
        Py_FatalError("psyco: internal bug: respawning in new_code_buffer()");
      fpo_build(&b->snapshot, po);
      if (ge != NULL)
        register_codebuf(ge, b);
      if (limit != NULL)
        po->codelimit = limit;
      po->respawn_cnt = 0;
      po->respawn_proxy = b;
    }
  return b;
}

DEFINEFN
CodeBufferObject* psyco_new_code_buffer(PsycoObject* po, global_entries_t* ge)
{
  return new_code_buffer(po, ge, NULL);
}

DEFINEFN
CodeBufferObject* psyco_proxy_code_buffer(PsycoObject* po, global_entries_t* ge)
{
  return new_code_buffer(po, ge, po->code);
}

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
  code_t* codeend = obj->codeptr + nsize;
  
  if (VERBOSE_LEVEL > 2)
    debug_printf(("psyco: disassemble %p %p    (%d bytes)\n", obj->codeptr,
                  codeend, nsize));
  else if (VERBOSE_LEVEL > 1)
    debug_printf(("[%d]", nsize));
  
  close_buffer_use(codeend);

#ifdef STORE_CODE_END
  extra_assert(obj->codeend == NULL);
  obj->codeend = codeend;
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
  
#if defined(ALL_CHECKS) && defined(STORE_CODE_END)
  if (self->codeend != NULL)
    {
      /* do not actully release, to detect calls to released code */
      /* 0xCC is the breakpoint instruction (INT 3) */
      memset(self->codeptr, 0xCC, self->codeend - self->codeptr);
      return;
    }
#endif
    
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
