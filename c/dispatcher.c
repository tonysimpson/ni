#include "dispatcher.h"
#include "codemanager.h"
#include "mergepoints.h"
#include "Python/pycompiler.h"   /* for pyc_data_xxx() */
#include "pycencoding.h"  /* for INC_OB_REFCNT() */


 /***************************************************************/
/***                       Snapshots                           ***/
 /***************************************************************/


DEFINEFN
void fpo_build(FrozenPsycoObject* fpo, PsycoObject* po)
{
  clear_tmp_marks(&po->vlocals);
  fpo->fz_vlocals = array_new(po->vlocals.count);
  duplicate_array(fpo->fz_vlocals, &po->vlocals);
  fpo->fz_stuff.as_int =
    (po->stack_depth<<8) | ((int) po->last_used_reg);
  fpo->fz_arguments_count = po->arguments_count;
  fpo->fz_pyc_data = pyc_data_new(&po->pr);
}

DEFINEFN
void fpo_release(FrozenPsycoObject* fpo)
{
  if (fpo->fz_pyc_data != NULL)
    pyc_data_delete(fpo->fz_pyc_data);
  array_delete(fpo->fz_vlocals, NULL);
}

static void find_regs_array(vinfo_array_t* source, PsycoObject* po)
{
  int i = source->count;
  while (i--)
    {
      vinfo_t* a = source->items[i];
      if (a != NULL)
        {
          Source src = a->source;
          if (is_runtime(src) && !is_reg_none(src))
            REG_NUMBER(po, getreg(src)) = a;
          else if (psyco_vsource_cc(src) != CC_ALWAYS_FALSE)
            po->ccreg = a;
          if (a->array != NullArray)
            find_regs_array(a->array, po);
        }
    }
}

DEFINEFN
PsycoObject* fpo_unfreeze(FrozenPsycoObject* fpo)
{
  /* rebuild a PsycoObject from 'this' */
  PsycoObject* po = PsycoObject_New(fpo->fz_vlocals->count);
  po->stack_depth = get_stack_depth(fpo);
  po->last_used_reg = (reg_t)(fpo->fz_stuff.as_int & 0xFF);
  po->arguments_count = fpo->fz_arguments_count;
  assert_cleared_tmp_marks(fpo->fz_vlocals);
  duplicate_array(&po->vlocals, fpo->fz_vlocals);
  clear_tmp_marks(fpo->fz_vlocals);
  find_regs_array(&po->vlocals, po);
  frozen_copy(&po->pr, fpo->fz_pyc_data);
  pyc_data_build(po, psyco_get_merge_points(po->pr.co));
  return po;
}


/*****************************************************************
 * Respawning is restoring a frozen compiler state into a live
 * PsycoObject and restarting the compilation. It will produce
 * exactly the same code up to a given point. This point was
 * a jump in the run-time code, pointing to code that we did
 * not compile yet. The purpose of 'replaying' the compilation
 * is to rebuild exactly the same state as the compiler had when
 * it emitted the jump instruction in the first place. At this
 * point we can go on with the real compilation of the missing
 * code.
 *
 * This is all based on the idea that we want to avoid tons
 * of copies of PsycoObjects all around for all pending
 * compilation branches, and there are a lot of them -- e.g. all
 * instructions that could trigger an exception have such a
 * (generally uncompiled) branch.
 *****/

typedef struct respawn_s {
  CodeBufferObject* self;
  code_t* write_jmp;
  char cond;
  short respawn_cnt;
  CodeBufferObject* respawn_from;
} respawn_t;

static code_t* do_respawn(respawn_t* rs)
{
  /* called when entering a not-compiled branch requiring a respawn */
  code_t* code;
  CodeBufferObject* firstcodebuf;
  CodeBufferObject* codebuf;
  PsycoObject* po;

  /* we might have a chain of code buffers, each containing a conditional
     jump to the next one. It ends with a proxy (not-yet-compiled) calling
     the function do_respawn().
     
     +----------+
     |  ...     |
     |  Jcond ----------->  +----------+
     |  ...     |           |  ...     |
     +----------+           |  Jcond ----------->   +-----------------+
                            |  ...     |            | CALL do_respawn |
                            +----------+            +-----------------+

     The structure 'rs' is stored in the last block (the proxy).
     'rs->respawn_from' is the previous code buffer.
     'rs->respawn_from->snapshot.fz_respawned_from' is the previous one.
     etc.
  */
  int respawn_cnt = rs->respawn_cnt;
  /* find the first code buffer in the chain */
  for (firstcodebuf = rs->respawn_from;
       firstcodebuf->snapshot.fz_respawned_from != NULL;
       firstcodebuf = firstcodebuf->snapshot.fz_respawned_from)
    respawn_cnt = firstcodebuf->snapshot.fz_respawned_cnt;
  /* respawn there */
  po = fpo_unfreeze(&firstcodebuf->snapshot);
  
  codebuf = psyco_new_code_buffer(NULL, NULL);
  if (codebuf == NULL)
    OUT_OF_MEMORY();
  codebuf->snapshot.fz_stuff.respawning = rs;
  codebuf->snapshot.fz_respawned_cnt = rs->respawn_cnt;
  codebuf->snapshot.fz_respawned_from = firstcodebuf;
  code = codebuf->codeptr;
  po->code = code;
  po->codelimit = code + BIG_BUFFER_SIZE - GUARANTEED_MINIMUM;
  /* respawn by restarting the Python compiler at the beginning of the
     instruction where it left. It will probably re-emit a few machine
     instructions -- not needed, they will be trashed, but this has
     rebuilt the correct PsycoObject state. This occurs when eventually
     a positive DETECT_RESPAWN() is issued. */
  po->respawn_cnt = - respawn_cnt;
  po->respawn_proxy = codebuf;

  code = GLOBAL_ENTRY_POINT(po);
  
  SHRINK_CODE_BUFFER(codebuf, code - codebuf->codeptr, "respawned");
  /* make sure DETECT_RESPAWN() succeeded */
  extra_assert(codebuf->snapshot.fz_respawned_from == rs->respawn_from);

  /* fix the jump to point to 'codebuf->codeptr' */
  code = rs->write_jmp;
/*   if (rs->cond == CC_ALWAYS_TRUE) */
/*     JUMP_TO(codebuf->codeptr); */
/*   else */
    FAR_COND_JUMP_TO(codebuf->codeptr, rs->cond);
  /* cannot Py_DECREF(cp->self) because the current function is returning into
     that code now, but any time later is fine: use the trash of codemanager.c */
  psyco_trash_object((PyObject*) rs->self);
  psyco_dump_code_buffers();
  /* XXX don't know what to do with this reference to codebuf */
  return codebuf->codeptr;
}

DEFINEFN
void psyco_respawn_detected(PsycoObject* po)
{
  /* this is called when detect_respawn() succeeds. We can now proceed
     to the next code block in the picture above. When we reach the
     last one, it means we are about to compile the 'special' branch of
     a conditional jump -- the one that is not compiled yet.
  */
  CodeBufferObject* codebuf = po->respawn_proxy;
  CodeBufferObject* current = codebuf->snapshot.fz_respawned_from;
  respawn_t* rs = codebuf->snapshot.fz_stuff.respawning;

  /* 'codebuf' is the new block we are writing.
     'current' is the block we are currently respawning.
     'rs->respawn_from' is the last block before the proxy calling
        do_respawn(). */
  if (current == rs->respawn_from)
    {
      /* respawn finished */
      extra_assert(codebuf->snapshot.fz_vlocals == NullArray);
      fpo_build(&codebuf->snapshot, po);
    }
  else
    {
      /* proceed to the next block */
      CodeBufferObject* nextblock;
      int respawn_cnt = rs->respawn_cnt;
      for (nextblock = rs->respawn_from;
           nextblock->snapshot.fz_respawned_from != current;
           nextblock = nextblock->snapshot.fz_respawned_from)
        respawn_cnt = nextblock->snapshot.fz_respawned_cnt;
      codebuf->snapshot.fz_respawned_from = nextblock;
      po->respawn_cnt = - respawn_cnt;
    }
  /* restart at the beginning of the buffer, overriding the code written
     so far. XXX when implementing freeing of code, be careful that this
     lost code does not looses references to other objects as well.
     Use is_respawning() to bypass the creation of all references when
     compiling during respawn. */
  po->code = codebuf->codeptr;
}

DEFINEFN
void psyco_prepare_respawn(PsycoObject* po, condition_code_t jmpcondition)
{
  /* ignore calls to psyco_prepare_respawn() while currently respawning */
  if (!is_respawning(po))
    {
      respawn_t* rs;
      code_t* calling_code;
      CodeBufferObject* codebuf = psyco_new_code_buffer(NULL, NULL);
      if (codebuf == NULL)
        OUT_OF_MEMORY();
  
      extra_assert(jmpcondition < CC_TOTAL);

      /* the proxy contains only a jump to do_respawn,
         followed by a respawn_t structure */
      calling_code = po->code;
      po->code = codebuf->codeptr;
      BEGIN_CODE
      TEMP_SAVE_REGS_FN_CALLS;
      END_CODE
      rs = (respawn_t*) psyco_jump_proxy(po, &do_respawn, 1, 1);
      SHRINK_CODE_BUFFER(codebuf,
                         (code_t*)(rs+1) - codebuf->codeptr,
                         "respawn");
      /* fill in the respawn_t structure */
      extra_assert(po->respawn_proxy != NULL);
      rs->self = codebuf;
      rs->write_jmp = calling_code;
      rs->cond = jmpcondition;
      rs->respawn_cnt = po->respawn_cnt;
      rs->respawn_from = po->respawn_proxy;

      /* write the jump to the proxy */
      po->code = calling_code;
      BEGIN_CODE
      /*   if (jmpcondition == CC_ALWAYS_TRUE) */
      /*     JUMP_TO(codebuf->codeptr); */
      /*   else */
        FAR_COND_JUMP_TO(codebuf->codeptr, jmpcondition);
      END_CODE
      psyco_dump_code_buffers();
    }
  else
    po->code = po->respawn_proxy->codeptr;  /* respawning: come back at the
                                               beginning of the trash memory for
                                               the next instructions */
}

DEFINEFN
int runtime_NON_NULL_f(PsycoObject* po, vinfo_t* vi)
{
  condition_code_t cc = integer_NON_NULL(po, vi);
  return cc == CC_ERROR ? -1 : runtime_condition_f(po, cc);
}

DEFINEFN
int runtime_NON_NULL_t(PsycoObject* po, vinfo_t* vi)
{
  condition_code_t cc = integer_NON_NULL(po, vi);
  return cc == CC_ERROR ? -1 : runtime_condition_t(po, cc);
}


/*****************************************************************/

static bool compatible_array(vinfo_array_t* aa, vinfo_array_t* bb,
                             vinfo_array_t** result)
{
  /* 'aa' is the array from the live PsycoObject. 'bb' is from the snapshop.
     Test for compatibility. More precisely, it must be allowable for the
     living state 'aa' to jump back to the code already produced for the
     state 'bb'. There might be more information in 'aa' than in 'bb' (which
     will then just be discarded), but the converse is not allowable.
     Moreover, if two slots in the arrays point to the same vinfo_t in 'bb',
     they must also do so in 'aa' because the code compiled from 'bb' might
     have used this fact. Conversely, shared pointers in 'aa' need *not* be
     shared any more in 'bb'.

     The sharing rule only applies to run-time values: for compile-time and
     virtual-time values, being shared or not is irrelevant to the compilation
     process.
     
     Return value of compatible_array(): false if incompatible, otherwise
     fills the 'result' array with the variables to un-promote.
  */
  int i;
  int count = bb->count;
  if (aa->count != count)
    {
      if (aa->count < count)   /* array too short; ok only if the extra items */
	{                      /*   in 'bb' are all NULL.                     */
	  for (i=aa->count; i<count; i++)
	    if (bb->items[i] != NULL)
	      return false;   /* differs */
	  count = aa->count;
	}
      else      /* array too long; fail unless all the extra items are NULL */
	{
	  for (i=aa->count; i>count; )
	    if (aa->items[--i] != NULL)
	      return false;   /* differs */
	}
    }
  for (i=0; i<count; i++)
    {
      vinfo_t* b = bb->items[i];
      if (b != NULL)     /* if b == NULL, any value in 'a' is ok. */
	{
          long diff;
          vinfo_t* a = aa->items[i];
          /* we store in the 'tmp' fields of the 'bb' arrays pointers to the
             vinfo_t that matched in 'aa'. We assume that all 'tmp' fields
             are NULL initially. If, walking in the 'bb' arrays, we encounter
             the same 'b' several times, we use these 'tmp' pointers to make
             sure they all matched the same 'a'. */
	  if (b->tmp != NULL)
	    {
              /* This 'b' has already be seen. */
	      if (b->tmp == a)
                continue;  /* quotient graph */

              /* at this point, the graph 'bb' is not a quotient of the
                 graph 'aa', i.e. two nodes are shared in 'aa' but not
                 in 'bb'. This is not acceptable if it is a run-time
                 value. */
              if (is_runtime(b->source))
		goto incompatible;
	    }
          
          /* A new 'b', let's check if its 'a' matches. */
          if (a == NULL)
            goto incompatible;  /* NULL not compatible with non-NULL */
          b->tmp = a;
          diff = ((long)a->source) ^ ((long)b->source);
          if (diff != 0)
            {
              if ((diff & TimeMask) != 0)
                goto incompatible;  /* not the same TIME_MASK */
              if (is_runtime(a->source))
                {
                  if ((diff & RunTime_NoRef) != 0)
                    {
                      /* from 'with ref' to 'without ref' or vice-versa:
                         a source in 'a' with reference cannot pass for
                         a source in 'b' without reference */
                      if ((a->source & RunTime_NoRef) == 0)
                        goto incompatible;
                    }
                }
              else
                {
                  if (is_virtualtime(a->source))
                    goto incompatible;  /* different virtual sources */
                  if (KNOWN_SOURCE(a)->value != KNOWN_SOURCE(b)->value)
                    {
                      if ((KNOWN_SOURCE(b)->refcount1_flags &
                           SkFlagFixed) != 0)
                        goto incompatible;  /* b's value is fixed */
                      else {
                        /* approximative match, might un-promote 'a' from
                           compile-time to run-time. */
                        //fprintf(stderr, "psyco: compatible_array() with vinfo_t* a=%p, b=%p\n", a, b);
                        int i, ocount = (*result)->count;
                        /* do not add several time the same value to the array */
                        for (i=0; i<ocount; i++)
                          if ((*result)->items[i] == a)
                            break;
                        if (i==ocount)
                          {
                            *result = array_grow1(*result, ocount+1);
                            (*result)->items[ocount] = a;
                          }
                      }
                    }
                }
            }
          if (a->array != b->array)
            {                     /* can only be equal if both ==NullArray */
              if (!compatible_array(a->array, b->array, result))
                goto incompatible;
            }
        }
    }
  return true;

 incompatible:     /* we have to reset the 'tmp' fields to NULL,
                      but only as far as we actually progressed */
  for (; i>=0; i--)
    if (bb->items[i] != NULL)
      {
	bb->items[i]->tmp = NULL;
	if (bb->items[i]->array != NullArray)
	  clear_tmp_marks(bb->items[i]->array);
      }
  return false;
}

DEFINEFN
vinfo_array_t* psyco_compatible(PsycoObject* po, global_entries_t* patterns,
                                CodeBufferObject** matching)
{
  int i;
  vinfo_array_t* bestresult = NULL;
  PyObject* plist = patterns->fatlist;
  extra_assert(PyList_Check(plist));
  i = PyList_GET_SIZE(plist);
  while (i--)    /* the most dummy algorithm: step by step in the list, */
    {            /* checking for a match at each step.                  */
      vinfo_array_t* differences = NullArray;
      CodeBufferObject* codebuf = (CodeBufferObject*) PyList_GET_ITEM(plist, i);
      extra_assert(CodeBuffer_Check(codebuf));
      /* invariant: all snapshot.fz_vlocals in the fatlist have
         all their 'tmp' fields set to NULL. */
      assert_cleared_tmp_marks(codebuf->snapshot.fz_vlocals);
      if (compatible_array(&po->vlocals, codebuf->snapshot.fz_vlocals,
                           &differences))
	{
          /* compatible_array() leaves data in the 'tmp' fields.
             It must be cleared unless it is the final result of
             psyco_compatible() itself. */
	  if (differences == NullArray)
	    {
              /* Total match */
	      *matching = codebuf;
	      return NullArray;
	    }
          else
            {
              /* Partial match, clear 'tmp' fields */
              clear_tmp_marks(codebuf->snapshot.fz_vlocals);
              if (bestresult != NULL)
                {
                  if (bestresult->count <= differences->count)
                    continue;  /* not better than the previous partial match */
                  array_release(bestresult);
                }
              /* Record the first partial match we found */
              bestresult = differences;
              *matching = codebuf;
            }
	}
      else   /* compatible_array() should have reset all 'tmp' fields */
	assert_cleared_tmp_marks(codebuf->snapshot.fz_vlocals);
    }
  return bestresult;
}

DEFINEFN
void psyco_stabilize(CodeBufferObject* lastmatch)
{
  clear_tmp_marks(lastmatch->snapshot.fz_vlocals);
}


 /***************************************************************/
/***                         Unification                       ***/
 /***************************************************************/

struct dmove_s {
  PsycoObject* po;
  char* usages;   /* buffer: array of vinfo_t*, see ORIGINAL_VINFO() below */
  int usages_size;
  vinfo_t* copy_regs[REG_TOTAL];
  code_t* code_origin;
  code_t* code_limit;
  CodeBufferObject* private_codebuf;
};

static code_t* data_new_buffer(code_t* code, struct dmove_s* dm)
{
  /* creates a new buffer containing a copy of the already-written code */
  CodeBufferObject* codebuf;
  int codesize;
  assert(dm->private_codebuf == NULL);  /* otherwise it means we overwrote
                                           the BIG_BUFFER_SIZE */
  codebuf = psyco_new_code_buffer(NULL, NULL);
  if (codebuf == NULL)
    OUT_OF_MEMORY();
  /* copy old code to new buffer */
  codesize = code - dm->code_origin;
  memcpy(codebuf->codeptr, dm->code_origin, codesize);
  dm->private_codebuf = codebuf;
  dm->code_origin = codebuf->codeptr;
  dm->code_limit = dm->code_origin + BIG_BUFFER_SIZE;
  return codebuf->codeptr + codesize;
}

#define ORIGINAL_VINFO(spos)   (*(vinfo_t**)(dm->usages + (spos)))

static void data_original_table(struct dmove_s* dm, vinfo_array_t* bb)
{
  int i = bb->count;
  while (i--)
    {
      vinfo_t* b = bb->items[i];
      if (b != NULL)
        {
	  if (is_runtime(b->source) && RUNTIME_STACK(b->tmp) < dm->usages_size)
            ORIGINAL_VINFO(RUNTIME_STACK(b->tmp)) = b->tmp;
          if (b->array != NullArray)
	    data_original_table(dm, b->array);
        }
    }
}

static code_t* data_update_stack(code_t* code, struct dmove_s* dm,
                                 vinfo_array_t* bb)
{
  PsycoObject* po = dm->po;
  int i = bb->count;
  while (i--)
    {
      vinfo_t* b = bb->items[i];
      if (b != NULL)
        {
          if (is_runtime(b->source))
            {
              char rg;
              vinfo_t* overridden;
              vinfo_t* a = b->tmp;   /* source value */
              long dststack = RUNTIME_STACK(b);
              long srcstack = RUNTIME_STACK(a);

              /* check for values passing from no-reference to reference
                 or vice-versa */
              if (((a->source ^ b->source) & RunTime_NoRef) != 0)
                {
                  /* from 'with ref' to 'no ref' is forbidden
                     by psyco_compatible() */
                  extra_assert((a->source & RunTime_NoRef) != 0);
                  RTVINFO_IN_REG(a);
                  rg = RUNTIME_REG(a);
                  INC_OB_REFCNT(rg);
                  a->source &= ~RunTime_NoRef;
                }
              
              rg = RUNTIME_REG(b);
              if (rg != REG_NONE)
                dm->copy_regs[(int)rg] = a;
              if (dststack == RUNTIME_STACK_NONE || dststack == srcstack)
                goto done;
              rg = RUNTIME_REG(a);
              if (rg == REG_NONE)
                {
                  NEED_FREE_REG(rg);
                  LOAD_REG_FROM_EBP_BASE(rg, srcstack);
                  REG_NUMBER(po, rg) = a;
                  SET_RUNTIME_REG_TO(a, rg);
                }
              a->source = RunTime_NewStack(dststack, getreg(a->source), false);
              overridden = ORIGINAL_VINFO(dststack);
              if (overridden == NULL)
                goto can_save_only;
              ORIGINAL_VINFO(dststack) = NULL;
              
              if (!RUNTIME_REG_IS_NONE(overridden))
                {
                  SET_RUNTIME_STACK_TO_NONE(overridden);
                can_save_only:
                  SAVE_REG_TO_EBP_BASE(rg, dststack);
                }
              else
                {
                  XCHG_REG_AND_EBP_BASE(rg, dststack);
                  SET_RUNTIME_REG_TO(overridden, rg);
                  SET_RUNTIME_STACK_TO_NONE(overridden);
                }
              
              if (code > dm->code_limit)
                /* oops, buffer overflow. Start a new buffer */
                code = data_new_buffer(code, dm);
            }
        done:
          if (b->array != NullArray)
            code = data_update_stack(code, dm, b->array);
        }
    }
  return code;
}

DEFINEFN
code_t* psyco_unify(PsycoObject* po, CodeBufferObject** target)
{
  /* Update 'this' to match 'target', then jump to 'target'. */

  int i;
  struct dmove_s dm;
  code_t* code = po->code;
  CodeBufferObject* target_codebuf = *target;
  int sdepth = get_stack_depth(&target_codebuf->snapshot);
  char pops[REG_TOTAL+2];

  psyco_assert_coherent(po);
  if (sdepth > po->stack_depth)
    {
      /* more items in the target stack (uncommon case).
         Let the stack grow. */
      STACK_CORRECTION(sdepth - po->stack_depth);
      po->stack_depth = sdepth;
    }
  dm.usages_size = sdepth + sizeof(vinfo_t**);
  dm.usages = (char*) PyCore_MALLOC(dm.usages_size);
  if (dm.usages == NULL)
    OUT_OF_MEMORY();
  memset(dm.usages, 0, dm.usages_size);   /* set to all NULL */
  memset(dm.copy_regs, 0, sizeof(dm.copy_regs));
  data_original_table(&dm, target_codebuf->snapshot.fz_vlocals);

  dm.po = po;
  dm.code_origin = code;
  dm.code_limit = po->codelimit == NULL ? code : po->codelimit;
  dm.private_codebuf = NULL;

  /* update the stack */
  code = data_update_stack(code, &dm, target_codebuf->snapshot.fz_vlocals);

  /* update the registers (1): reg-to-reg moves and exchanges */
  memset(pops, -1, sizeof(pops));
  for (i=0; i<REG_TOTAL; i++)
    {
      vinfo_t* a = dm.copy_regs[i];
      if (a != NULL)
        {
          char rg = RUNTIME_REG(a);
          if (rg != REG_NONE)
            {
              if (rg != i)
                {
                  vinfo_t* c = REG_NUMBER(po, i);
                  if (c != NULL)
                    {
                      SET_RUNTIME_REG_TO(c, rg);
                      REG_NUMBER(po, rg) = c;
                      XCHG_REGS(i, rg);
                    }
                  else
                    LOAD_REG_FROM_REG(i, rg);
                  /* an update is omitted because we are about to
                     release 'this' anyway: 'REG_NUMBER(po, i) = a;' */
                }
              dm.copy_regs[i] = NULL;
            }
          else
            {  /* prepare the step (2) below by looking for registers
                  whose source is near the top of the stack */
              int from_tos = po->stack_depth - RUNTIME_STACK(a);
              extra_assert(from_tos >= 0);
              if (from_tos < REG_TOTAL*sizeof(void*))
                {
                  char* target = pops + (from_tos / sizeof(void*));
                  if (*target == -1)
                    *target = i;
                  else
                    *target = -2;
                }
            }
        }
    }
  /* update the registers (2): stack-to-register POPs */
  for (i=0; pops[i]>=0 || pops[i+1]>=0; i++)
    {
      char reg = pops[i];
      if (reg<0)
        {  /* If there is only one 'garbage' stack entry, POP it as well.
              If there are more, give up and use regular MOVs to load the rest */
          po->stack_depth -= 4;
          reg = pops[++i];
          POP_REG(reg);
        }
      POP_REG(reg);
      dm.copy_regs[(int) reg] = NULL;
      po->stack_depth -= 4;
    }
  /* update the registers (3): stack-to-register loads */
  for (i=0; i<REG_TOTAL; i++)
    {
      vinfo_t* a = dm.copy_regs[i];
      if (a != NULL)
        LOAD_REG_FROM_EBP_BASE(i, RUNTIME_STACK(a));
    }

  /* done */
  STACK_CORRECTION(sdepth - po->stack_depth);
  if (code > dm.code_limit)  /* start a new buffer if we wrote past the end */
    code = data_new_buffer(code, &dm);
  JUMP_TO(target_codebuf->codeptr);
  
  /* start a new buffer if the last JUMP_TO overflowed,
     but not if we had no room at all in the first place. */
  if (code > dm.code_limit && po->codelimit != NULL)
    code = data_new_buffer(code, &dm);
  
  PyCore_FREE(dm.usages);
  psyco_stabilize(target_codebuf);
  if (dm.private_codebuf == NULL)
    Py_INCREF(target_codebuf);      /* no new buffer created */
  else
    {
      SHRINK_CODE_BUFFER(dm.private_codebuf, code - dm.code_origin,
                         "unify");
      *target = dm.private_codebuf;
      /* add a jump from the original code buffer to the new one */
      code = po->code;
      JUMP_TO(dm.private_codebuf->codeptr);
      psyco_dump_code_buffers();
    }
  PsycoObject_Delete(po);
  return code;
}

DEFINEFN
CodeBufferObject* psyco_unify_code(PsycoObject* po, CodeBufferObject* target)
{
  /* simplified interface to psyco_unify() without using a previously
     existing code buffer. */

  code_t localbuf[GUARANTEED_MINIMUM];
  /* relies on the fact that psyco_unify() has no room at all in localbuf.
     Anything but the final JMP will trigger the creation of a new code
     buffer. */
  po->code = localbuf;
  po->codelimit = NULL;
  psyco_unify(po, &target);
  return target;
}

#define KEEP_MARK   ((vinfo_t*) 1)

static int mark_to_keep(vinfo_array_t* array, bool virtual_parent)
{
  int i, total=0;
  for (i=array->count; i--; )
    {
      vinfo_t* vi = array->items[i];
      if (vi != NULL)
        {
          if (is_runtime(vi->source) && vi->tmp == NULL)
            {
              if (!virtual_parent)
                continue;
              /* mark this item to be kept */
              vi->tmp = KEEP_MARK;
              total++;
            }
          if (vi->array != NullArray)
            total += mark_to_keep(vi->array, is_virtualtime(vi->source));
        }
    }
  return total;
}

static void remove_non_marked(vinfo_array_t* array)
{
  int i;
  for (i=array->count; i--; )
    {
      vinfo_t* vi = array->items[i];
      if (vi != NULL)
        {
          if (is_runtime(vi->source) && vi->tmp == NULL)
            {
              /* remove this item */
              array->items[i] = NULL;
              vinfo_decref(vi, NULL);
            }
          else if (vi->array != NullArray)
            remove_non_marked(vi->array);
        }
    }
}

DEFINEFN
int psyco_simplify_array(vinfo_array_t* array)
{
  /* We remove a run-time vinfo_t if it is not directly in 'array' and
     if it is not in the sub-vinfo_array_t of any virtual-time source. */
  
  /* First mark with a non-NULL value all run-time sources that must
     be kept. */
  int total = mark_to_keep(array, true);

  /* Remove all non-marked run-time sources */
  remove_non_marked(array);

  /* Done */
  return total;
}


 /***************************************************************/
/***                Promotion and un-promotion                 ***/
 /***************************************************************/


/*****************************************************************/
 /***   Promotion of a run-time variable into a fixed           ***/
  /***   compile-time one                                        ***/


/* Implementation tactics */

/* 0 -- use a plain Python dictionary whose keys
        are the value to promote and values the code objects.
   1 -- put code buffers in a chained list,
        with the most used items moving forward in the list.
   2 -- order the code buffers as a binary search tree,
        with the most used nodes moving up in the tree (not implemented)

   (1) seems slower than (0) even if the structure is never actually used
   (i.e. PROMOTION_FAST_COMMON_CASE catches 100% of the cases). Is it
   related to the fact that (1) mixes data at the beginning of
   each code buffer? */
#define PROMOTION_TACTIC             1

/* Define to 1 to emit a "compare/jump-if-equal" pair of instructions
   that checks for the most common case (actually the last seen one). */
#define PROMOTION_FAST_COMMON_CASE   1


#if PROMOTION_TACTIC == 1
typedef struct rt_local_buf_s {
# ifdef CODE_DUMP_FILE
  long signature;
# endif
  struct rt_local_buf_s* next;
  long key;
} rt_local_buf_t;
#endif

typedef struct { /* produced at compile time and read by the dispatcher */
  PsycoObject* po;        /* state before promotion */
  vinfo_t* fix;           /* variable to promote */
#if PROMOTION_FAST_COMMON_CASE
  code_t* jump_if_equal_code;  /* for FIX_JUMP_IF_EQUAL() */
#endif
  
#if PROMOTION_TACTIC == 0
  PyObject* spec_dict;    /* local cache (promotions to already-seen values) */
# ifdef CODE_DUMP_FILE
  void** chained_list;    /* must be last, with spec_dict just before */
# endif
#endif

#if PROMOTION_TACTIC == 1
  rt_local_buf_t* local_chained_list;
#endif
} rt_promotion_t;

inline code_t* fix_fast_common_case(rt_promotion_t* fs, long value,
                                    code_t* codeptr)
{
#if PROMOTION_FAST_COMMON_CASE
  FIX_JUMP_IF_EQUAL(fs->jump_if_equal_code, value, codeptr);
#endif
  return codeptr;
}

#if PROMOTION_TACTIC == 0
#define NEED_PYOBJ_KEY
inline code_t* lookup_old_promotion_values(rt_promotion_t* fs,
                                           PyObject* key)
{
  /* have we already seen this value? */
  CodeBufferObject* codebuf;
  codebuf = (CodeBufferObject*) PyDict_GetItem(fs->spec_dict, key);
  if (codebuf != NULL)   /* yes */
    return codebuf->codeptr;
  return NULL;  /* no */
}
#endif  /* PROMOTION_TACTIC == 0 */

#if PROMOTION_TACTIC == 1
inline code_t* lookup_old_promotion_values(rt_promotion_t* fs,
                                           long value)
{
  rt_local_buf_t** ppbuf;
  if (fs->local_chained_list == NULL)
    return NULL;  /* not found (list is empty) */

#if PROMOTION_FAST_COMMON_CASE
  /* 'fs->local_chained_list' points to the current head
     of the list, which we know is not what we are looking
     for because otherwise the CMP/JE instructions
     would have found it and we would not be here */
  extra_assert(fs->local_chained_list->key != value);
#else
  if (fs->local_chained_list->key == value)
    return (code_t*)(fs->local_chained_list+1);  /* it is the head of the list */
#endif

  ppbuf = &fs->local_chained_list->next;
  while (1)
    {
      rt_local_buf_t* buf = *ppbuf;
      if (buf == NULL)
        return NULL;  /* not found (list exhausted) */
      if (buf->key == value)
        {
          /* found inside the list, put it at the head */
          *ppbuf = buf->next;
          buf->next = fs->local_chained_list;
          fs->local_chained_list = buf;
          return (code_t*)(buf+1);
        }
      ppbuf = &buf->next;
    }
}
#endif  /* PROMOTION_TACTIC == 1 */


static code_t* do_promotion_internal(rt_promotion_t* fs,
#ifdef NEED_PYOBJ_KEY
                                     PyObject* key,
#else
                                     long key,
#endif
                                     source_known_t* sk)
{
  CodeBufferObject* codebuf;
  code_t* result;
  vinfo_t* v;
  PsycoObject* newpo;
  PsycoObject* po = fs->po;
  mergepoint_t* mp;

  /* get a copy of the compiler state */
  newpo = PsycoObject_Duplicate(po);
  if (newpo == NULL)
    OUT_OF_MEMORY();
  /* store the copy back into 'fs' and use the old 'po' to compile.
     We do so because in 'newpo' all 'tmp' fields are now NULL,
     but no longer in 'po'. */
  fs->po = newpo;
  v = fs->fix;      /* get the variable we will promote to compile-time... */
  fs->fix = v->tmp; /*     ...and update 'fs' with its copy in 'newpo'     */
  
  /* fix the value of 'v' */
  CHKTIME(v->source, RunTime);   /* from run-time to compile-time */
  extra_assert(RUNTIME_REG_IS_NONE(v));   /* taken care of in
                                             finish_promotion() */
  v->source = CompileTime_NewSk(sk);
  /* compile from this new state, in which 'v' has been promoted to
     compile-time. */
  mp = psyco_exact_merge_point(po->pr.merge_points, po->pr.next_instr);

#if PROMOTION_TACTIC == 0
  codebuf = psyco_compile_code(po, mp);

  /* store the new code buffer into the local cache */
  if (PyDict_SetItem(fs->spec_dict, key, (PyObject*) codebuf))
    OUT_OF_MEMORY();
  Py_DECREF(codebuf);  /* there is a reference left
                          in the dictionary */
  result = codebuf->codeptr;
#endif

#if PROMOTION_TACTIC == 1
  codebuf = psyco_new_code_buffer(NULL, NULL);
  if (codebuf == NULL)
    OUT_OF_MEMORY();
  {
    code_t* codeend;
    rt_local_buf_t* buf = (rt_local_buf_t*) codebuf->codeptr;
# ifdef CODE_DUMP_FILE
    buf->signature = 0x66666666;
# endif
    buf->next = fs->local_chained_list;
    buf->key = key;
    fs->local_chained_list = buf;
    result = (code_t*)(buf+1);
    
    po->code = result;
    po->codelimit = codebuf->codeptr + BIG_BUFFER_SIZE - GUARANTEED_MINIMUM;
    codeend = psyco_compile(po, mp, false);
    psyco_shrink_code_buffer(codebuf, codeend - codebuf->codeptr);
    /* XXX don't know what to do with reference to 'codebuf' */
  }
#endif

  psyco_dump_code_buffers();
  return result;
}


/* NOTE: the following two functions must be as fast as possible, because
   they are called from the run-time code even during normal (non-compiling)
   execution. */
static code_t* do_promotion_long(rt_promotion_t* fs, long value)
{
  /* need a PyObject* key for the local cache dictionary */
  code_t* result;

#ifdef NEED_PYOBJ_KEY
  PyObject* key1 = PyInt_FromLong(value);
  if (key1 == NULL)
    OUT_OF_MEMORY();
#else
  long key1 = value;
#endif

  /* have we already seen this value? */
  result = lookup_old_promotion_values(fs, key1);
  if (result == NULL)
    {
      /* no -> we must build new code */
      result = do_promotion_internal(fs, key1, sk_new(value, SkFlagFixed));
    }
#ifdef NEED_PYOBJ_KEY
  Py_DECREF(key1);
#endif
  /* done -> jump to the codebuf */
  return fix_fast_common_case(fs, value, result);
}

static code_t* do_promotion_pyobj(rt_promotion_t* fs, PyObject* key)
{
  code_t* result;

#ifdef NEED_PYOBJ_KEY
  PyObject* key1 = key;
#else
  long key1 = (long) key;
#endif

  /* have we already seen this value? */
  result = lookup_old_promotion_values(fs, key1);
  if (result == NULL)
    {
      /* no -> we must build new code */
      Py_INCREF(key);
      result = do_promotion_internal(fs, key1, sk_new((long) key,
                                                      SkFlagFixed|SkFlagPyObj));
    }
  /* done -> jump to the codebuf */
  return fix_fast_common_case(fs, (long) key, result);
}

DEFINEFN
code_t* psyco_finish_promotion(PsycoObject* po, vinfo_t* fix, long kflags)
{
  long xsource;
  rt_promotion_t* fs;
#if PROMOTION_FAST_COMMON_CASE
  code_t* jeqcode;
#endif
  void* do_promotion;

  BEGIN_CODE
#if PROMOTION_FAST_COMMON_CASE
  RTVINFO_IN_REG(fix);
#endif
  xsource = fix->source;
#if PROMOTION_FAST_COMMON_CASE
  RESERVE_JUMP_IF_EQUAL(RSOURCE_REG(xsource));
  jeqcode = code;
#endif
  if (PROMOTION_FAST_COMMON_CASE || !RSOURCE_REG_IS_NONE(xsource))
    {
      /* remove from 'po->regarray' this value which will soon no longer
         be RUN_TIME */
      REG_NUMBER(po, RSOURCE_REG(xsource)) = NULL;
      SET_RUNTIME_REG_TO_NONE(fix);
    }
#if PROMOTION_FAST_COMMON_CASE
  TEMP_SAVE_REGS_FN_CALLS;
#else
  SAVE_REGS_FN_CALLS;  /* save the registers EAX, ECX and EDX if needed
                          and mark them invalid because of the CALL below */
#endif
  CALL_SET_ARG_FROM_RT(xsource, 1, 2);  /* argument index 1 out of total 2 */
  END_CODE

  /* write the code that calls the proxy 'do_promotion' */
  if ((kflags & SkFlagPyObj) == 0)
    do_promotion = &do_promotion_long;
  else
    do_promotion = &do_promotion_pyobj;
  fs = (rt_promotion_t*) psyco_jump_proxy(po, do_promotion,
                                          PROMOTION_FAST_COMMON_CASE, 2);

  /* fill in the constant structure that 'do_promotion' will get as parameter */
  clear_tmp_marks(&po->vlocals);
  psyco_assert_coherent(po);
  fs->po = po;    /* don't release 'po' */
  fs->fix = fix;
#if PROMOTION_FAST_COMMON_CASE
  fs->jump_if_equal_code = jeqcode;
#endif
  
#if PROMOTION_TACTIC == 0
  fs->spec_dict = PyDict_New();
  if (fs->spec_dict == NULL)
    OUT_OF_MEMORY();
# ifdef CODE_DUMP_FILE
  fs->chained_list = psyco_codebuf_spec_dict_list;
  psyco_codebuf_spec_dict_list = (void**)&fs->chained_list;
# endif
#endif

#if PROMOTION_TACTIC == 1
  fs->local_chained_list = NULL;
#endif
  
  return (code_t*)(fs+1);  /* end of code == end of 'fs' structure */
}


/*****************************************************************/
 /***   Promotion of certain run-time values into               ***/
  /***   compile-time ones (promotion only occurs for certain    ***/
   /***   values, e.g. for types that we know how to optimize).   ***/

typedef struct { /* produced at compile time and read by the dispatcher */
  fixed_switch_t* rts;    /* special values */
  PsycoObject* po;        /* state before promotion */
  vinfo_t* fix;           /* variable to promote */
  long kflags;            /* flags after promotion */
  code_t* switchcodeend;  /* end of the private copy of the switch code */
} rt_fixed_switch_t;

static code_t* do_fixed_switch(rt_fixed_switch_t* rtfxs, long value)
{
  int item;
  CodeBufferObject* codebuf;
  fixed_switch_t* rts = rtfxs->rts;
  vinfo_t* v;
  PsycoObject* newpo;
  PsycoObject* po = rtfxs->po;

  /* get a copy of the compiler state */
  newpo = psyco_duplicate(po);
  /* store the copy back into rtfxs and use the old 'po' to compile.
     We do so because in 'newpo' all 'tmp' fields are now NULL,
     but no longer in 'po'. */
  rtfxs->po = newpo;
  v = rtfxs->fix;      /* get the variable we will promote to compile-time... */
  rtfxs->fix = v->tmp; /*   ...and update 'rtfxs' with its copy in 'newpo'    */

  item = psyco_switch_lookup(rts, value);   /* which value did we found? */
  if (item == -1)
    {
      /* none --> go into 'default' mode */
      /* abuse the 'array' field to point to this fixed_switch_t
         to mean 'known to be none of the special values'.
         See known_to_be_default(). */
      v->array = NullArrayAt(rts->zero);
    }
  else
    {
      /* fix the value of 'v' to the one we found */
      CHKTIME(v->source, RunTime);   /* from run-time to compile-time */
      if (!RUNTIME_REG_IS_NONE(v))
        REG_NUMBER(po, RUNTIME_REG(v)) = NULL;
      v->source = CompileTime_NewSk(sk_new(value, rtfxs->kflags));
    }

  /* compile from this new state */
  codebuf = psyco_compile_code(po,  psyco_exact_merge_point(po->pr.merge_points,
                                                            po->pr.next_instr));

  /* store the pointer to the new code directly into
     the original code that jumped to do_fixed_switch() */
  psyco_fix_switch_case(rts, rtfxs->switchcodeend, item, codebuf->codeptr);
  
  return codebuf->codeptr;  /* jump there */
  /* XXX no place to store the reference to codebuf */
}

DEFINEFN
code_t* psyco_finish_fixed_switch(PsycoObject* po, vinfo_t* fix, long kflags,
                                  fixed_switch_t* special_values)
{
  rt_fixed_switch_t* rtfxs;
  code_t* switchcodeend;
  CHKTIME(fix->source, RunTime);
  extra_assert(fix->array->count == 0);  /* cannot fix array values,
                                            because of known_to_be_default() */
  BEGIN_CODE
  NEED_CC();
  RTVINFO_IN_REG(fix);
  switchcodeend = code = psyco_write_run_time_switch(special_values, code,
                                                     RUNTIME_REG(fix));
  
  TEMP_SAVE_REGS_FN_CALLS;   /* save all registers that might be clobbered */
  CALL_SET_ARG_FROM_RT(fix->source, 1, 2);/* argument index 1 out of total 2 */
  END_CODE

  /* write the code that calls the proxy 'do_fixed_switch' */
  rtfxs = (rt_fixed_switch_t*) psyco_jump_proxy(po, &do_fixed_switch, 1, 2);

  /* fill in the constant struct that 'do_fixed_switch' will get as parameter */
  clear_tmp_marks(&po->vlocals);
  psyco_assert_coherent(po);
  rtfxs->rts = special_values;
  rtfxs->po = po;
  rtfxs->fix = fix;
  rtfxs->kflags = kflags;
  rtfxs->switchcodeend = switchcodeend;
  return (code_t*)(rtfxs+1);
}


/*****************************************************************/
 /***   Un-Promotion from non-fixed compile-time into run-time  ***/

DEFINEFN
void psyco_unfix(PsycoObject* po, vinfo_t* vi)
{
  /* Convert 'vi' from compile-time-known to run-time-variable. */
  vinfo_t* newvi;
  source_known_t* sk;

  /*printf("psyco_unfix(%p, %p, %p)\n", po, code, vi);*/
  extra_assert(array_contains(&po->vlocals, vi));
  CHKTIME(vi->source, CompileTime);

  newvi = make_runtime_copy(po, vi);
  /* make_runtime_copy() never fails for compile-time sources */
  extra_assert(newvi != NULL);

  /* release 'vi->source' and move 'newvi->source' into it */
  sk = CompileTime_Get(vi->source);
  if (sk->refcount1_flags & SkFlagPyObj) {
    /* XXX can't release the PyObject anywhere, because we
       write a pointer to it in the code itself. We should
       somehow transfer the ownership of this reference to
       the CodeBufferObject. Fix me when implementing
       memory releasing! */
    sk->refcount1_flags &= ~SkFlagPyObj;
  }
  sk_decref(sk);
  vinfo_move(po, vi, newvi);
}
