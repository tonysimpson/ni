#include "mergepoints.h"
#include "vcompiler.h"
#include <opcode.h>


 /***************************************************************/
/***                Tables of code merge points                ***/
 /***************************************************************/

/* for each code object we build a bitarray specifying which
   positions in the byte code are potential merge points.
   A "merge point" is an instruction which can be executed
   immediately after two or more other instructions,
   typically jump targets.

   The respawn mecanisms (see psyco_prepare_respawn()) require
   that a merge point be also added after each instruction
   whose produced machine code might depend on external data.
   We should also avoid too long uninterrupted ranges of
   instructions without a single merge point.
*/

/* instructions that cause an unconditional jump: */
#define IS_JUMP_INSTR(op)    ((op) == BREAK_LOOP ||     \
                              (op) == RETURN_VALUE ||   \
                              (op) == JUMP_FORWARD ||   \
                              (op) == JUMP_ABSOLUTE ||  \
                              (op) == CONTINUE_LOOP ||  \
                              (op) == RAISE_VARARGS ||  \
                              0)

/* instructions with a target: */
#define HAS_JREL_INSTR(op)   ((op) == JUMP_FORWARD ||   \
                              (op) == JUMP_IF_FALSE ||  \
                              (op) == JUMP_IF_TRUE ||   \
                              (op) == FOR_LOOP ||       \
                              (op) == SETUP_LOOP ||     \
                              (op) == SETUP_EXCEPT ||   \
                              (op) == SETUP_FINALLY ||  \
                              0)
     
#define HAS_JABS_INSTR(op)   ((op) == JUMP_ABSOLUTE ||  \
                              (op) == CONTINUE_LOOP ||  \
                              0)

/* instructions whose target may be jumped to several times: */
#define HAS_J_MULTIPLE(op)   ((op) == SETUP_LOOP ||     \
                              (op) == SETUP_EXCEPT ||   \
                              (op) == SETUP_FINALLY ||  \
                              0)

/* instructions producing code dependent on the context: */
#define IS_CTXDEP_INSTR(op)  ((op) == LOAD_GLOBAL ||    \
                              0)

#define MAX_UNINTERRUPTED_RANGE   300  /* bytes of Python bytecode */

DEFINEFN
char* psyco_get_merge_points(PyCodeObject* co)
{
  static PyObject* CodeMergePoints = NULL;
  PyObject* s;
  if (CodeMergePoints == NULL)
    {
      CodeMergePoints = PyDict_New();
      if (CodeMergePoints == NULL)
        OUT_OF_MEMORY();
    }

  /* cache results */
  s = PyDict_GetItem(CodeMergePoints, co->co_code);
  if (s == NULL)
    {
      int length = PyString_GET_SIZE(co->co_code);
      int bytelength = (length+7)/8;
      unsigned char* source = (unsigned char*) PyString_AS_STRING(co->co_code);
      char* target;
      char* paths = (char*) PyCore_MALLOC(length);
      int i, lasti, oparg = 0;
      if (paths == NULL)
        OUT_OF_MEMORY();
      memset(paths, 0, length);
      
      for (i=0; i<length; )
        {
          unsigned char op = source[i++];
          if (HAS_ARG(op))
            {
              i += 2;
              oparg = (source[i-1]<<8) + source[i-2];
              if (op == EXTENDED_ARG)
                {
                  op = source[i++];
                  assert(HAS_ARG(op) && op != EXTENDED_ARG);
                  i += 2;
                  oparg = oparg<<16 | ((source[i-1]<<8) + source[i-2]);
                }
              if (HAS_JREL_INSTR(op) && paths[i+oparg] < 2)
                {
                  if (HAS_J_MULTIPLE(op))
                    paths[i+oparg] = 2;
                  else
                    paths[i+oparg]++;
                }
              if (HAS_JABS_INSTR(op) && paths[oparg] < 2)
                {
                  if (HAS_J_MULTIPLE(op))
                    paths[oparg] = 2;
                  else
                    paths[oparg]++;
                }
              if (IS_CTXDEP_INSTR(op))
                paths[i] = 2;
            }
          if (!IS_JUMP_INSTR(op))
            paths[i]++;
        }

      s = PyString_FromStringAndSize(NULL, bytelength);
      if (s == NULL)
        OUT_OF_MEMORY();
      target = PyString_AS_STRING(s);
      memset(target, 0, bytelength);

/*       printf("mergepoints.c: "); */
      lasti = 0;
      for (i=0; i<length; i++)
        if (paths[i] > 1 || (i-lasti > MAX_UNINTERRUPTED_RANGE && paths[i] > 0))
          {
            lasti = i;
            SET_ARRAY_BIT(target, i);
/*             printf("[%d]", i); */
          }
/*       printf("\n"); */
      
      PyCore_FREE(paths);
      if (PyDict_SetItem(CodeMergePoints, co->co_code, s))
        OUT_OF_MEMORY();
    }
  return PyString_AS_STRING(s);
}
