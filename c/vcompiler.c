#include "vcompiler.h"
#include "dispatcher.h"
#include "codemanager.h"
#include "mergepoints.h"
#include "Python/pycompiler.h"
#include "pycencoding.h"


DEFINEVAR const long psyco_zero = 0;
DEFINEVAR source_virtual_t psyco_vsource_not_important;


/*****************************************************************/

#define VI_BLOCK_COUNT   (4096 / sizeof(vinfo_t))

DEFINEVAR void** vinfo_linked_list = NULL;

DEFINEFN
void* vinfo_malloc_block()
#ifdef PSYCO_NO_LINKED_LISTS
{
  vinfo_t* p = (vinfo_t*) PyCore_MALLOC(sizeof(vinfo_t));
  if (p == NULL)
    OUT_OF_MEMORY();
  return p;
}
#else
{
  vinfo_t* p;
  vinfo_t* prev = (vinfo_t*) vinfo_linked_list;
  vinfo_t* block = (vinfo_t*) PyCore_MALLOC(VI_BLOCK_COUNT*sizeof(vinfo_t));
  if (block == NULL)
    /*return NULL;*/ OUT_OF_MEMORY();
  
  for (p=block+VI_BLOCK_COUNT; --p!=block; )
    {
      *(vinfo_t**)p = prev;
      prev = p;
    }
  vinfo_linked_list = *(void***) prev;
  return prev;
}
#endif


#define SK_BLOCK_COUNT   (1024 / sizeof(source_known_t))

DEFINEVAR void** sk_linked_list = NULL;

DEFINEFN
void* sk_malloc_block()
#ifdef PSYCO_NO_LINKED_LISTS
{
  source_known_t* p = (source_known_t*) PyCore_MALLOC(sizeof(source_known_t));
  if (p == NULL)
    OUT_OF_MEMORY();
  return p;
}
#else
{
  source_known_t* p;
  source_known_t* prev = (source_known_t*) sk_linked_list;
  source_known_t* block = (source_known_t*)      \
    PyCore_MALLOC(SK_BLOCK_COUNT*sizeof(source_known_t));
  if (block == NULL)
    /*return NULL;*/ OUT_OF_MEMORY();
  
  for (p=block+SK_BLOCK_COUNT; --p!=block; )
    {
      *(source_known_t**)p = prev;
      prev = p;
    }
  sk_linked_list = *(void***) prev;
  return prev;
}
#endif


/*****************************************************************/

DEFINEFN
vinfo_array_t* array_grow1(vinfo_array_t* array, int ncount)
{
  int i = array->count;
  extra_assert(ncount > i);
  if (i == 0)
    array = PyCore_MALLOC(sizeof(int) + ncount * sizeof(vinfo_t*));
  else
    array = PyCore_REALLOC(array, sizeof(int) + ncount * sizeof(vinfo_t*));
  if (array == NULL)
    OUT_OF_MEMORY();
  array->count = ncount;
  while (i<ncount)
    array->items[i++] = NULL;
  return array;
}


/*****************************************************************/

DEFINEFN
void sk_release(source_known_t *sk)
{
#if 0
  XXX --- XXX --- XXX --- XXX --- XXX --- XXX --- XXX --- XXX --- XXX
    The Python objects who get once in a source_known_t are never
    freed. This is so because we might have references to them left
    in the code buffers. This is tricky because the references can
    be indirect (to a sub-objects, to a field in the object structure,
                 etc...)
    So not freeing them at all is the easy way out. It is expected
    that not too many objects will get lost this way. This must be
    carefully worked out when implementing releasing of code
    buffers. It will probably require careful checks for all
    instructions that might emit an immediate value in the code,
    and for where this immediate value (indirectly or not) comes from.
  XXX --- XXX --- XXX --- XXX --- XXX --- XXX --- XXX --- XXX --- XXX
  if ((sk->refcount1_flags & SkFlagPyObj) != 0) {
    Py_XDECREF((PyObject*)(sk->value));
  }
#endif
  sk_delete(sk);
}

DEFINEFN
vinfo_t* vinfo_copy(vinfo_t* vi)
{
  vinfo_t* result = vinfo_new(vi->source);
  result->array = vi->array;
  if (result->array->count > 0)
    {
      result->array = array_new(result->array->count);
      duplicate_array(result->array, vi->array);
    }
  if (is_compiletime(result->source))
    sk_incref(CompileTime_Get(result->source));
  return result;
}


DEFINEFN
void vinfo_release(vinfo_t* vi, PsycoObject* po)
{
  switch (gettime(vi->source)) {
    
  case RunTime:
    if (po != NULL)
      {
        if (has_rtref(vi->source))
          {
            /* write Py_DECREF() when releasing the last reference to
               a run-time vinfo_t holding a reference to a Python object */
            psyco_decref_rt(po, vi);
          }
        if (!is_reg_none(vi->source))
          REG_NUMBER(po, getreg(vi->source)) = NULL;
      }
    break;

  case CompileTime:
    sk_decref(CompileTime_Get(vi->source));
    break;
    
  case VirtualTime:
    if (po != NULL && vi == po->ccreg)
      po->ccreg = NULL;
    break;
  }

  /* must be after the switch because psyco_decref_rt() will use the
     array to extract any available time information to speed up Py_DECREF(). */
  if (vi->array != NullArray)
    array_delete(vi->array, po);

  /* only virtual-time vinfos are allowed in po->ccreg */
  extra_assert(po == NULL || vi != po->ccreg);
  VINFO_FREE_1(vi);
}


DEFINEFN
void clear_tmp_marks(vinfo_array_t* array)
{
  /* clear all 'tmp' fields in the array, recursively */
  int i = array->count;
  while (i--)
    if (array->items[i] != NULL)
      {
	array->items[i]->tmp = NULL;
	if (array->items[i]->array != NullArray)
	  clear_tmp_marks(array->items[i]->array);
      }
}

#ifdef ALL_CHECKS
DEFINEFN
void assert_cleared_tmp_marks(vinfo_array_t* array)
{
  /* assert that all 'tmp' fields are NULL */
  int i = array->count;
  while (i--)
    if (array->items[i] != NULL)
      {
	assert(array->items[i]->tmp == NULL);
	if (array->items[i]->array != NullArray)
	  assert_cleared_tmp_marks(array->items[i]->array);
      }
}
#endif

DEFINEFN
bool array_contains(vinfo_array_t* array, vinfo_t* vi)
{
  int i = array->count;
  while (i--)
    if (array->items[i] != NULL)
      {
	if (array->items[i] == vi)
          return true;
	if (array->items[i]->array != NullArray)
	  if (array_contains(array->items[i]->array, vi))
            return true;
      }
  return false;
}

#ifdef ALL_CHECKS
static void coherent_array(vinfo_array_t* source, PsycoObject* po, int found[])
{
  int i = source->count;
  while (i--)
    if (source->items[i] != NULL)
      {
        Source src = source->items[i]->source;
	if (is_runtime(src) && !is_reg_none(src))
          {
            assert(REG_NUMBER(po, getreg(src)) == source->items[i]);
            found[(int) getreg(src)] = 1;
          }
        if (psyco_vsource_cc(src) != CC_ALWAYS_FALSE)
          {
            assert(po->ccreg == source->items[i]);
            found[REG_TOTAL] = 1;
          }
	if (source->items[i]->array != NullArray)
	  coherent_array(source->items[i]->array, po, found);
      }
}

static void hack_refcounts(vinfo_array_t* source, int delta, int mvalue)
{
  int i = source->count;
  while (i--)
    if (source->items[i] != NULL)
      {
        long rc = source->items[i]->refcount;
        rc = ((rc + delta) & 0xFFFF) + (rc & 0x10000);
        source->items[i]->refcount = rc;
        if ((rc & 0x10000) == mvalue)
          {
            source->items[i]->refcount ^= 0x10000;
            if (source->items[i]->array != NullArray)
              hack_refcounts(source->items[i]->array, delta, mvalue);
          }
      }
}

static vinfo_t* nonnull_refcount(vinfo_array_t* source)
{
  int i = source->count;
  while (i--)
    if (source->items[i] != NULL)
      {
        if (source->items[i]->refcount != 0x10000 &&
            /* list here the 'global' vinfo_t's whose reference counters
               might be larger than the # of references hold by this po alone */
            source->items[i] != psyco_viNone &&
            source->items[i] != psyco_viZero &&
            source->items[i] != psyco_viOne  &&
            1)
          {
            fprintf(stderr, "nonnull_refcount: item %d\n", i);
            return source->items[i];
          }
	if (source->items[i]->array != NullArray)
          {
            vinfo_t* result = nonnull_refcount(source->items[i]->array);
            if (result != NULL)
              {
                fprintf(stderr, "nonnull_refcount: in array item %d\n", i);
                return result;
              }
          }
      }
  return NULL;
}

DEFINEFN
void psyco_assert_coherent(PsycoObject* po)
{
  vinfo_array_t debug_extra_refs;
  int found[REG_TOTAL+1];
  int i;
  vinfo_t* err;
  for (i=0; i<=REG_TOTAL; i++)
    found[i] = 0;
  debug_extra_refs.count = 2;
  debug_extra_refs.items[0] = po->pr.exc;  /* normally private to pycompiler.c,*/
  debug_extra_refs.items[1] = po->pr.val;  /* but this is for debugging only */
  coherent_array(&po->vlocals, po, found);
  coherent_array(&debug_extra_refs, po, found);
  for (i=0; i<REG_TOTAL; i++)
    if (!found[i])
      assert(REG_NUMBER(po, i) == NULL);
  if (!found[REG_TOTAL])
    assert(po->ccreg == NULL);
  hack_refcounts(&po->vlocals, -1, 0);
  hack_refcounts(&debug_extra_refs, -1, 0);
  err = nonnull_refcount(&po->vlocals);
  hack_refcounts(&debug_extra_refs, +1, 0x10000);
  hack_refcounts(&po->vlocals, +1, 0x10000);
  assert(!err);  /* see nonnull_refcounts() */
}
#endif

DEFINEFN
void duplicate_array(vinfo_array_t* target, vinfo_array_t* source)
{
  /* make a depth copy of an array.
     Same requirements as psyco_duplicate().
     Do not use for arrays of length 0. */
  int i;
  for (i=0; i<source->count; i++)
    {
      vinfo_t* sourcevi = source->items[i];
      if (sourcevi == NULL)
	target->items[i] = NULL;
      else if (sourcevi->tmp != NULL)
	{
	  target->items[i] = sourcevi->tmp;
	  target->items[i]->refcount++;
	}
      else
	{
	  vinfo_t* targetvi = vinfo_copy(sourcevi);
	  targetvi->tmp = NULL;
	  target->items[i] = sourcevi->tmp = targetvi;
	}
    }
  target->count = source->count;
  
  /*return true;

 fail:
  while (i--)
    if (items[i] != NULL)
      {
        items[i]->tmp = NULL;
        target->items[i]->decref(NULL);
      }
      return false;*/
}

DEFINEFN
PsycoObject* psyco_duplicate(PsycoObject* po)
{
  /* Requires that all 'tmp' marks in 'po' are cleared.
     In the new copy all 'tmp' marks will be cleared. */
  
  int i;
  PsycoObject* result = PsycoObject_New(po->vlocals.count);
  psyco_assert_coherent(po);
  assert_cleared_tmp_marks(&po->vlocals);
  duplicate_array(&result->vlocals, &po->vlocals);

  /* set the register pointers of 'result' to the new vinfo_t's */
  for (i=0; i<REG_TOTAL; i++)
    if (REG_NUMBER(po, i) != NULL)
      REG_NUMBER(result, i) = REG_NUMBER(po, i)->tmp;
  if (po->ccreg != NULL)
    result->ccreg = po->ccreg->tmp;

  /* the rest of the data is copied with no change */
  result->stack_depth = po->stack_depth;
  result->last_used_reg = po->last_used_reg;
  result->arguments_count = po->arguments_count;
  result->respawn_cnt = po->respawn_cnt;
  result->respawn_proxy = po->respawn_proxy;
  result->code = po->code;
  result->codelimit = po->codelimit;
  pyc_data_duplicate(&result->pr, &po->pr);

  assert_cleared_tmp_marks(&result->vlocals);
  return result;
}

DEFINEFN void PsycoObject_Delete(PsycoObject* po)
{
  pyc_data_release(&po->pr);
  deallocate_array(&po->vlocals, NULL);
  PyCore_FREE(po);
}


/*****************************************************************/


typedef struct {
	CodeBufferObject*	self;
	PsycoObject* 		po;
	resume_fn_t		resume_fn;
	code_t*			write_jmp;
	condition_code_t	cond;
} coding_pause_t;

static code_t* do_resume_coding(coding_pause_t* cp)
{
  /* called when entering a coding_pause (described by 'cp') */
  code_t* code;
  code_t* target = (cp->resume_fn) (cp->po, cp+1);

  /* fix the jump to point to 'target' */
  /* safety check: do not write a JMP whose target is itself...
     would make an endless loop */
  code = cp->write_jmp;
  assert(target != code);
  if (cp->cond == CC_ALWAYS_TRUE)
    JUMP_TO(target);
  else
    FAR_COND_JUMP_TO(target, cp->cond);
  /* cannot Py_DECREF(cp->self) because the current function is returning into
     that code now, but any time later is fine: use the trash of codemanager.c */
  psyco_dump_code_buffers();
  psyco_trash_object((PyObject*) cp->self);
  return target;
}

/* Prepare a 'coding pause', i.e. a short amount of code (proxy) that will be
   called only if the execution actually reaches it to go on with compilation.
   'po' is the PsycoObject corresponding to the proxy.
   'condition' may not be CC_ALWAYS_FALSE.
   The (possibly conditional) jump to the proxy is encoded in 'calling_code'.
   When the execution reaches the proxy, 'resume_fn' is called and the proxy
   destroys itself and replaces the original jump to it by a jump to the newly
   compiled code. */
DEFINEFN
void psyco_coding_pause(PsycoObject* po, condition_code_t jmpcondition,
                        resume_fn_t resume_fn, void* extra, int extrasize)
{
  coding_pause_t* cp;
  code_t* calling_code;
  CodeBufferObject* codebuf = psyco_new_code_buffer(NULL, NULL);
  if (codebuf == NULL)
    OUT_OF_MEMORY();
  
  extra_assert(jmpcondition != CC_ALWAYS_FALSE);

  /* the proxy contains only a jump to do_resume_coding,
     followed by a coding_pause_t structure, itself followed by the
     'extra' data. */
  calling_code = po->code;
  po->code = codebuf->codeptr;
  BEGIN_CODE
  TEMP_SAVE_REGS_FN_CALLS;
  END_CODE
  cp = (coding_pause_t*) psyco_jump_proxy(po, &do_resume_coding, 1, 1);
  SHRINK_CODE_BUFFER(codebuf,
                     (code_t*)(cp+1) + extrasize - codebuf->codeptr,
                     "coding_pause");
  /* fill in the coding_pause_t structure and the following 'extra' data */
  cp->self = codebuf;
  cp->po = po;
  cp->resume_fn = resume_fn;
  cp->write_jmp = calling_code;
  cp->cond = jmpcondition;
  memcpy(cp+1, extra, extrasize);

  /* write the jump to the proxy */
  po->code = calling_code;
  BEGIN_CODE
  if (jmpcondition == CC_ALWAYS_TRUE)
    JUMP_TO(codebuf->codeptr);  /* jump always */
  else
    FAR_COND_JUMP_TO(codebuf->codeptr, jmpcondition);
  END_CODE
  psyco_dump_code_buffers();
}

/* for psyco_coding_pause(): a resume function that simply resumes compilation.
 */
static code_t* psyco_resume_compile(PsycoObject* po, void* extra)
{
  mergepoint_t* mp = psyco_exact_merge_point(po->pr.merge_points,
                                             po->pr.next_instr);
  return psyco_compile_code(po, mp)->codeptr;
  /* XXX don't know what to do with the reference returned by
     XXX po->compile_code() */
}


/* Main compiling function. Emit machine code corresponding to the state
   'po'. The compiler produces its code into 'code' and the return value is
   the end of the written code. 'po' is freed. */
DEFINEFN
code_t* psyco_compile(PsycoObject* po, mergepoint_t* mp,
                      bool continue_compilation)
{
  CodeBufferObject* oldcodebuf;
  vinfo_t* diff = mp==NULL ? INCOMPATIBLE :
                     psyco_compatible(po, &mp->entries, &oldcodebuf);

  /*psyco_assert_cleared_tmp_marks(&po->vlocals);  -- not needed -- */
  
  if (diff == COMPATIBLE)
    {
      code_t* code2 = psyco_unify(po, &oldcodebuf);
      Py_DECREF(oldcodebuf);
      return code2;
    }
  else
    {
      if (po->codelimit - po->code <= BUFFER_MARGIN && diff == INCOMPATIBLE)
        {
          /* Running out of space in this buffer. */
          
          /* Instead of going on we stop now and make ready to
             start the new buffer later, when the execution actually
             reaches this point. This forces the emission of code to
             pause at predicible intervals. Among other advantages it
             prevents long or infinite loops from exploding the memory
             while the user sees no progression in the execution of
             her program.
           */
          psyco_coding_pause(po, CC_ALWAYS_TRUE, &psyco_resume_compile, NULL, 0);
          return po->code;
        }

      /* Enough space left, continue in the same buffer. */
      if (mp != NULL)
        {
          CodeBufferObject* codebuf = psyco_proxy_code_buffer(po, &mp->entries);
          if (codebuf == NULL)
            OUT_OF_MEMORY();
#ifdef CODE_DUMP_FILE
          codebuf->chained_list = psyco_codebuf_chained_list;
          psyco_codebuf_chained_list = codebuf;
#endif
          Py_DECREF(codebuf);
        }
      
      if (diff != INCOMPATIBLE)
        {
          /* diff points to a vinfo_t: make it run-time */
          psyco_unfix(po, diff);
          /* start over (maybe we have already seen this new state) */
          return psyco_compile(po, mp, continue_compilation);
        }

      if (continue_compilation)
        return NULL;  /* I won't actually compile myself, let the caller know */
      
      /* call the entry point function which performs the actual compilation */
      return GLOBAL_ENTRY_POINT(po);
    }
}

DEFINEFN
void psyco_compile_cond(PsycoObject* po, mergepoint_t* mp,
                        condition_code_t condition)
{
  CodeBufferObject* oldcodebuf;
  vinfo_t* diff = mp==NULL ? INCOMPATIBLE :
                     psyco_compatible(po, &mp->entries, &oldcodebuf);
  PsycoObject* po2 = PsycoObject_Duplicate(po);

  extra_assert(condition < CC_TOTAL);

  if (diff == COMPATIBLE)
    {
      /* try to emit:
                           JNcond Label
                           <unification-and-jump>
                          Label:

         if <unification-and-jump> is only a JMP, recode the whole as a single
                           Jcond <unification-jump-target>
      */
      code_t* code2 = po->code + SIZE_OF_SHORT_CONDITIONAL_JUMP;
      code_t* target;
      code_t* codeend;
      
      po2->code = code2;
      po2->codelimit = code2 + RANGE_OF_SHORT_CONDITIONAL_JUMP;
      codeend = psyco_unify(po2, &oldcodebuf);
      Py_DECREF(oldcodebuf);
      BEGIN_CODE
      if (IS_A_SINGLE_JUMP(code2, codeend, target))
        FAR_COND_JUMP_TO(target, condition);
      else
        {
          SHORT_COND_JUMP_TO(codeend, INVERT_CC(condition));
          code = codeend;
        }
      END_CODE
    }
  else
    {
      /* Use the conditional-compiling abilities of
         coding_pause(); it will write a Jcond to a proxy
         which will perform the actual compilation later.
      */
      psyco_coding_pause(po2, condition, &psyco_resume_compile, NULL, 0);
      po->code = po2->code;
    }
}

/* Simplified interface to compile() without using a previously
   existing code buffer. Return a new code buffer. */
DEFINEFN
CodeBufferObject* psyco_compile_code(PsycoObject* po, mergepoint_t* mp)
{
  code_t* code1;
  CodeBufferObject* codebuf;
  CodeBufferObject* oldcodebuf;
  vinfo_t* diff = mp==NULL ? INCOMPATIBLE :
                     psyco_compatible(po, &mp->entries, &oldcodebuf);

  /*psyco_assert_cleared_tmp_marks(&po->vlocals);  -- not needed -- */

  if (diff == COMPATIBLE)
    return psyco_unify_code(po, oldcodebuf);

  /* start a new buffer */
  codebuf = psyco_new_code_buffer(po, mp==NULL ? NULL : &mp->entries);
  if (codebuf == NULL)
    OUT_OF_MEMORY();
  po->code = codebuf->codeptr;
  
  if (diff != INCOMPATIBLE)
    {
      psyco_unfix(po, diff);
      /* start over (maybe we have already seen this new state) */
      code1 = psyco_compile(po, mp, false);
    }
  else
    {
      /* call the entry point function which performs the actual compilation
         (this is the usual case) */
      code1 = GLOBAL_ENTRY_POINT(po);
    }

  /* we have written some code into a new codebuf, now shrink it to
     its actual size */
  psyco_shrink_code_buffer(codebuf, code1 - codebuf->codeptr);
  psyco_dump_code_buffers();
  return codebuf;
}


/*****************************************************************/

static bool computed_do_not_use(PsycoObject* po, vinfo_t* vi)
{
  fprintf(stderr, "psyco: internal error (computed_do_not_use)\n");
  extra_assert(0);     /* stop if debugging */
  vi->source = SOURCE_DUMMY;
  return true;
}

DEFINEFN
void psyco_compiler_init()
{
  psyco_vsource_not_important.compute_fn = &computed_do_not_use;
}


/*****************************************************************/
