#include "mergepoints.h"
#include "vcompiler.h"
#include "Python/pycinternal.h"


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
                              IS_EPILOGUE_INSTR(op))

#ifdef FOR_ITER
# define COMMON_FOR_OPCODE    FOR_ITER
#else
# define COMMON_FOR_OPCODE    FOR_LOOP
#endif

#ifdef RETURN_NONE
# define IS_EPILOGUE_INSTR(op)  ((op) == RETURN_NONE)
#else
# define IS_EPILOGUE_INSTR(op)   0
#endif

/* instructions with a target: */
#define HAS_JREL_INSTR(op)   ((op) == JUMP_FORWARD ||   \
                              (op) == JUMP_IF_FALSE ||  \
                              (op) == JUMP_IF_TRUE ||   \
                              (op) == COMMON_FOR_OPCODE || \
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

/* all other supported instructions must be listed here. */
static const unsigned char other_opcodes[] = {
  POP_TOP,
  ROT_TWO,
  ROT_THREE,
  ROT_FOUR,
  DUP_TOP,
  DUP_TOPX,
  UNARY_POSITIVE,
  UNARY_NEGATIVE,
  UNARY_NOT,
  UNARY_CONVERT,
  UNARY_INVERT,
  BINARY_POWER,
  BINARY_MULTIPLY,
  BINARY_DIVIDE,
  BINARY_MODULO,
  BINARY_ADD,
  BINARY_SUBTRACT,
  BINARY_SUBSCR,
  BINARY_LSHIFT,
  BINARY_RSHIFT,
  BINARY_AND,
  BINARY_XOR,
  BINARY_OR,
#ifdef BINARY_FLOOR_DIVIDE
  BINARY_FLOOR_DIVIDE,
  BINARY_TRUE_DIVIDE,
  INPLACE_FLOOR_DIVIDE,
  INPLACE_TRUE_DIVIDE,
#endif
  INPLACE_POWER,
  INPLACE_MULTIPLY,
  INPLACE_DIVIDE,
  INPLACE_MODULO,
  INPLACE_ADD,
  INPLACE_SUBTRACT,
  INPLACE_LSHIFT,
  INPLACE_RSHIFT,
  INPLACE_AND,
  INPLACE_XOR,
  INPLACE_OR,
  SLICE+0,
  SLICE+1,
  SLICE+2,
  SLICE+3,
  STORE_SLICE+0,
  STORE_SLICE+1,
  STORE_SLICE+2,
  STORE_SLICE+3,
  DELETE_SLICE+0,
  DELETE_SLICE+1,
  DELETE_SLICE+2,
  DELETE_SLICE+3,
  STORE_SUBSCR,
  DELETE_SUBSCR,
  PRINT_EXPR,
  PRINT_ITEM,
  PRINT_ITEM_TO,
  PRINT_NEWLINE,
  PRINT_NEWLINE_TO,
  POP_BLOCK,
  END_FINALLY,
  BUILD_CLASS,
  UNPACK_SEQUENCE,
  STORE_ATTR,
  DELETE_ATTR,
  STORE_GLOBAL,
  DELETE_GLOBAL,
  LOAD_CONST,
  LOAD_GLOBAL,
  LOAD_FAST,
  STORE_FAST,
  DELETE_FAST,
  BUILD_TUPLE,
  BUILD_LIST,
  BUILD_MAP,
  LOAD_ATTR,
  /* COMPARE_OP see below */
  IMPORT_NAME,
  IMPORT_FROM,
#ifdef GET_ITER
  GET_ITER,
#endif
#ifdef SET_LINENO
  SET_LINENO,
#endif
  CALL_FUNCTION,
  CALL_FUNCTION_VAR,
  CALL_FUNCTION_KW,
  CALL_FUNCTION_VAR_KW,
  MAKE_FUNCTION,
  BUILD_SLICE,
  0 };

#define SUPPORTED_COMPARE_ARG(oparg)  ( \
                              (oparg) == Py_LT ||  \
                              (oparg) == Py_LE ||  \
                              (oparg) == Py_EQ ||  \
                              (oparg) == Py_NE ||  \
                              (oparg) == Py_GT ||  \
                              (oparg) == Py_GE ||  \
                              (oparg) == PyCmp_IS        ||  \
                              (oparg) == PyCmp_IS_NOT    ||  \
                              (oparg) == PyCmp_IN        ||  \
                              (oparg) == PyCmp_NOT_IN    ||  \
                              (oparg) == PyCmp_EXC_MATCH ||  \
                              0)

static PyObject* CodeMergePoints = NULL;
static char instr_control_flow[256];


#define MP_SUPPORTED       0x01
#define MP_IS_JUMP         0x02
#define MP_HAS_JREL        0x04
#define MP_HAS_JABS        0x08
#define MP_HAS_J_MULTIPLE  0x10
#define MP_IS_CTXDEP       0x20


inline void init_merge_points(void)
{
  if (CodeMergePoints == NULL)
    {
      int i;
      const unsigned char* p;
      for (i=0; i<256; i++) {
	char b = 0;
	if (IS_JUMP_INSTR(i))    b |= MP_IS_JUMP;
	if (HAS_JREL_INSTR(i))   b |= MP_HAS_JREL;
	if (HAS_JABS_INSTR(i))   b |= MP_HAS_JABS;
	if (HAS_J_MULTIPLE(i))   b |= MP_HAS_J_MULTIPLE;
	if (IS_CTXDEP_INSTR(i))  b |= MP_IS_CTXDEP;
	instr_control_flow[i] = b;
      }
      for (p=other_opcodes; *p; p++)
	instr_control_flow[(int)(*p)] |= MP_SUPPORTED;
      
      CodeMergePoints = PyDict_New();
      if (CodeMergePoints == NULL)
        OUT_OF_MEMORY();
    }
}

inline PyObject* build_merge_points(PyCodeObject* co)
{
  PyObject* s;
  mergepoint_t* mp;
  int mp_flags = MP_FLAGS_EXTRA;
  int length = PyString_GET_SIZE(co->co_code);
  unsigned char* source = (unsigned char*) PyString_AS_STRING(co->co_code);
  char* paths = (char*) PyMem_MALLOC(length+1);
  int i, lasti, count, oparg = 0;
  if (paths == NULL)
    OUT_OF_MEMORY();
  memset(paths, 0, length+1);
  paths[0] = 2;  /* always a merge point at the beginning of the bytecode */
  
  for (i=0; i<length; )
    {
#if VERBOSE_LEVEL
      int i0 = i;
#endif
      char flags;
      int jtarget;
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
          if (op == SETUP_EXCEPT)
            mp_flags |= MP_FLAGS_HAS_EXCEPT;
	}
      flags = instr_control_flow[(int) op];
      if (flags == 0)
	if (op != COMPARE_OP || !SUPPORTED_COMPARE_ARG(oparg))
	  {
	    /* unsupported instruction */
#if VERBOSE_LEVEL
            debug_printf(("psyco: unsupported instruction: "
                          "bytecode %d at %s:%d\n",
                          (int) op, PyCodeObject_NAME(co), i0));
#endif            
	    PyMem_FREE(paths);
	    Py_INCREF(Py_None);
	    return Py_None;
	  }
      if (flags & MP_IS_CTXDEP)
	paths[i] = 2;
      else if (!(flags & MP_IS_JUMP))
	paths[i]++;
      if (flags & MP_HAS_JREL)
        jtarget = i+oparg;
      else if (flags & MP_HAS_JABS)
        jtarget = oparg;
      else
        continue;  /* not a jump */

#ifdef SET_LINENO
      /* always ignore SET_LINENO opcodes.
         This could better group merge points, and
         it also ensures that there is a merge point right
         over a FOR_LOOP opcode (see FOR_LOOP in pycompiler.c). */
      while (source[jtarget] == SET_LINENO ||
             (source[jtarget] == EXTENDED_ARG &&
              source[jtarget+3] == SET_LINENO))
        jtarget += 3;
#endif
      if (flags & MP_HAS_J_MULTIPLE)
        paths[jtarget] = 2;
      else if (paths[jtarget] < 2)
        paths[jtarget]++;
    }

  /* count the merge points */
  count = 0;
  lasti = 0;
  for (i=0; i<length; i++)
    {
      if (i-lasti > MAX_UNINTERRUPTED_RANGE && paths[i] > 0)
	paths[i] = 2;  /* ensure there are merge points at regular intervals */
      if (paths[i] > 1)
	{
	  lasti = i;
	  count++;
	}
    }

  /* allocate the string buffer, one mergepoint_t per merge point plus
     the room for a final negative bitfield flags. */
  count = count * sizeof(mergepoint_t) + sizeof(int);
  s = PyString_FromStringAndSize(NULL, count);
  if (s == NULL)
    OUT_OF_MEMORY();
  mp = (mergepoint_t*) PyString_AS_STRING(s);

  for (i=0; i<length; i++)
    if (paths[i] > 1)
      {
	mp->bytecode_position = i;
	psyco_ge_init(&mp->entries);
	mp++;
      }
  mp->bytecode_position = mp_flags;
  PyMem_FREE(paths);
  return s;
}


DEFINEFN
PyObject* psyco_get_merge_points(PyCodeObject* co)
{
  PyObject* s;
  init_merge_points();

  /* cache results -- warning, don't cache on 'co->co_code' because
     although the position of the merge points really depend on the
     bytecode only, we use the 'entries' field to store pointer to
     already-compiled code, which depends on the other things in
     'co'. */
  s = PyDict_GetItem(CodeMergePoints, (PyObject*) co);
  if (s == NULL)
    {
      s = build_merge_points(co);
      if (PyDict_SetItem(CodeMergePoints, (PyObject*) co, s))
        OUT_OF_MEMORY();
      Py_DECREF(s);  /* one ref left in the dict */
    }
  return s;
}

DEFINEFN
mergepoint_t* psyco_next_merge_point(PyObject* mergepoints,
                                     int position)
{
  mergepoint_t* array;
  int bufsize;
  extra_assert(PyString_Check(mergepoints));
  array = (mergepoint_t*) PyString_AS_STRING(mergepoints);
  bufsize = PyString_GET_SIZE(mergepoints);
  extra_assert((bufsize % sizeof(mergepoint_t)) == sizeof(int));
  bufsize /= sizeof(mergepoint_t);
  extra_assert(bufsize > 0);
  do {
    int test = bufsize/2;
    if (position > array[test].bytecode_position)
      {
        ++test;
        array += test;
        bufsize -= test;
      }
    else
      {
        bufsize = test;
      }
  } while (bufsize > 0);
  return array;
}
