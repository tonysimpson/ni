#include "mergepoints.h"
#include "vcompiler.h"
#include "stats.h"
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
#define IS_JUMP_INSTR(op)    (op == BREAK_LOOP ||     \
                              op == RETURN_VALUE ||   \
                              op == JUMP_FORWARD ||   \
                              op == JUMP_ABSOLUTE ||  \
                              op == CONTINUE_LOOP ||  \
                              op == RAISE_VARARGS ||  \
                              IS_EPILOGUE_INSTR(op))

/* instructions with a target: */
#define HAS_JREL_INSTR(op)   (op == JUMP_FORWARD ||   \
                              op == JUMP_IF_FALSE ||  \
                              op == JUMP_IF_TRUE ||   \
                              IS_FOR_OPCODE(op) ||    \
                              op == SETUP_LOOP ||     \
                              op == SETUP_EXCEPT ||   \
                              op == SETUP_FINALLY)

#define HAS_JABS_INSTR(op)   (op == JUMP_ABSOLUTE ||  \
                              op == CONTINUE_LOOP)

/* instructions whose target may be jumped to several times: */
#define HAS_J_MULTIPLE(op)   (op == SETUP_LOOP ||     \
                              op == SETUP_EXCEPT ||   \
                              op == SETUP_FINALLY)

/* instructions producing code dependent on the context: */
#define IS_CTXDEP_INSTR(op)  (op == LOAD_GLOBAL)

#define MAX_UNINTERRUPTED_RANGE   300  /* bytes of Python bytecode */

/* opcodes that never produce any machine code or on which for some
   other reason there is no point in setting a merge point -- the
   merge point can simply be moved on to the next instruction */
#define NO_MERGE_POINT(op)  (IS_JUMP_INSTR(op) ||       \
                             HAS_JREL_INSTR(op) ||      \
                             HAS_JABS_INSTR(op) ||      \
                             IS_SET_LINENO(op) ||       \
                             op == POP_TOP ||           \
                             op == ROT_TWO ||           \
                             op == ROT_THREE ||         \
                             op == ROT_FOUR ||          \
                             op == DUP_TOP ||           \
                             op == DUP_TOPX ||          \
                             op == POP_BLOCK ||         \
                             op == END_FINALLY ||       \
                             op == LOAD_CONST ||        \
                             op == LOAD_FAST ||         \
                             op == STORE_FAST ||        \
                             op == DELETE_FAST)

/* all other supported instructions must be listed here. */
#define OTHER_OPCODE(op)  (op == UNARY_POSITIVE ||            \
                           op == UNARY_NEGATIVE ||            \
                           op == UNARY_NOT ||                 \
                           op == UNARY_CONVERT ||             \
                           op == UNARY_INVERT ||              \
                           op == BINARY_POWER ||              \
                           op == BINARY_MULTIPLY ||           \
                           op == BINARY_DIVIDE ||             \
                           op == BINARY_MODULO ||             \
                           op == BINARY_ADD ||                \
                           op == BINARY_SUBTRACT ||           \
                           op == BINARY_SUBSCR ||             \
                           op == BINARY_LSHIFT ||             \
                           op == BINARY_RSHIFT ||             \
                           op == BINARY_AND ||                \
                           op == BINARY_XOR ||                \
                           op == BINARY_OR ||                 \
                           IS_NEW_DIVIDE(op) ||               \
                           op == INPLACE_POWER ||             \
                           op == INPLACE_MULTIPLY ||          \
                           op == INPLACE_DIVIDE ||            \
                           op == INPLACE_MODULO ||            \
                           op == INPLACE_ADD ||               \
                           op == INPLACE_SUBTRACT ||          \
                           op == INPLACE_LSHIFT ||            \
                           op == INPLACE_RSHIFT ||            \
                           op == INPLACE_AND ||               \
                           op == INPLACE_XOR ||               \
                           op == INPLACE_OR ||                \
                           op == SLICE+0 ||                   \
                           op == SLICE+1 ||                   \
                           op == SLICE+2 ||                   \
                           op == SLICE+3 ||                   \
                           op == STORE_SLICE+0 ||             \
                           op == STORE_SLICE+1 ||             \
                           op == STORE_SLICE+2 ||             \
                           op == STORE_SLICE+3 ||             \
                           op == DELETE_SLICE+0 ||            \
                           op == DELETE_SLICE+1 ||            \
                           op == DELETE_SLICE+2 ||            \
                           op == DELETE_SLICE+3 ||            \
                           op == STORE_SUBSCR ||              \
                           op == DELETE_SUBSCR ||             \
                           op == PRINT_EXPR ||                \
                           op == PRINT_ITEM ||                \
                           op == PRINT_ITEM_TO ||             \
                           op == PRINT_NEWLINE ||             \
                           op == PRINT_NEWLINE_TO ||          \
                           op == BUILD_CLASS ||               \
                           op == UNPACK_SEQUENCE ||           \
                           op == STORE_ATTR ||                \
                           op == DELETE_ATTR ||               \
                           op == STORE_GLOBAL ||              \
                           op == DELETE_GLOBAL ||             \
                           op == LOAD_GLOBAL ||               \
                           op == BUILD_TUPLE ||               \
                           op == BUILD_LIST ||                \
                           op == BUILD_MAP ||                 \
                           op == LOAD_ATTR ||                 \
                           /* COMPARE_OP special-cased */     \
                           op == IMPORT_NAME ||               \
                           op == IMPORT_FROM ||               \
                           IS_GET_ITER(op) ||                 \
                           op == CALL_FUNCTION ||             \
                           op == CALL_FUNCTION_VAR ||         \
                           op == CALL_FUNCTION_KW ||          \
                           op == CALL_FUNCTION_VAR_KW ||      \
                           op == MAKE_FUNCTION ||             \
                           op == BUILD_SLICE)

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

 /***************************************************************/
#ifdef FOR_ITER
# define IS_FOR_OPCODE(op)      (op == FOR_ITER)
#else
# define IS_FOR_OPCODE(op)      (op == FOR_LOOP)
#endif

#ifdef GET_ITER
# define IS_GET_ITER(op)        (op == GET_ITER)
#else
# define IS_GET_ITER(op)         0
#endif

#ifdef RETURN_NONE
# define IS_EPILOGUE_INSTR(op)  (op == RETURN_NONE)
#else
# define IS_EPILOGUE_INSTR(op)   0
#endif

#ifdef SET_LINENO
# define IS_SET_LINENO(op)      (op == SET_LINENO)
#else
# define IS_SET_LINENO(op)       0
#endif

#ifdef BINARY_FLOOR_DIVIDE
# define IS_NEW_DIVIDE(op)      (op == BINARY_FLOOR_DIVIDE ||         \
                                 op == BINARY_TRUE_DIVIDE ||          \
                                 op == INPLACE_FLOOR_DIVIDE ||        \
                                 op == INPLACE_TRUE_DIVIDE)
#else
# define IS_NEW_DIVIDE(op)       0
#endif
 /***************************************************************/


#define MP_OTHER           0x01
#define MP_IS_JUMP         0x02
#define MP_HAS_JREL        0x04
#define MP_HAS_JABS        0x08
#define MP_HAS_J_MULTIPLE  0x10
#define MP_IS_CTXDEP       0x20
#define MP_NO_MERGEPT      0x40

#define F(op)      ((IS_JUMP_INSTR(op)    ? MP_IS_JUMP        : 0) |    \
                    (HAS_JREL_INSTR(op)   ? MP_HAS_JREL       : 0) |    \
                    (HAS_JABS_INSTR(op)   ? MP_HAS_JABS       : 0) |    \
                    (HAS_J_MULTIPLE(op)   ? MP_HAS_J_MULTIPLE : 0) |    \
                    (IS_CTXDEP_INSTR(op)  ? MP_IS_CTXDEP      : 0) |    \
                    (NO_MERGE_POINT(op)   ? MP_NO_MERGEPT     : 0) |    \
                    (OTHER_OPCODE(op)     ? MP_OTHER          : 0))

/* opcode table -- the preprocessor expands this into several hundreds KB
   of code (which reduces down to 256 bytes!).  Hopefully not a problem
   for modern C compilers */
static const char instr_control_flow[256] = {
  F(0x00), F(0x01), F(0x02), F(0x03), F(0x04), F(0x05), F(0x06), F(0x07),
  F(0x08), F(0x09), F(0x0A), F(0x0B), F(0x0C), F(0x0D), F(0x0E), F(0x0F),
  F(0x10), F(0x11), F(0x12), F(0x13), F(0x14), F(0x15), F(0x16), F(0x17),
  F(0x18), F(0x19), F(0x1A), F(0x1B), F(0x1C), F(0x1D), F(0x1E), F(0x1F),
  F(0x20), F(0x21), F(0x22), F(0x23), F(0x24), F(0x25), F(0x26), F(0x27),
  F(0x28), F(0x29), F(0x2A), F(0x2B), F(0x2C), F(0x2D), F(0x2E), F(0x2F),
  F(0x30), F(0x31), F(0x32), F(0x33), F(0x34), F(0x35), F(0x36), F(0x37),
  F(0x38), F(0x39), F(0x3A), F(0x3B), F(0x3C), F(0x3D), F(0x3E), F(0x3F),
  F(0x40), F(0x41), F(0x42), F(0x43), F(0x44), F(0x45), F(0x46), F(0x47),
  F(0x48), F(0x49), F(0x4A), F(0x4B), F(0x4C), F(0x4D), F(0x4E), F(0x4F),
  F(0x50), F(0x51), F(0x52), F(0x53), F(0x54), F(0x55), F(0x56), F(0x57),
  F(0x58), F(0x59), F(0x5A), F(0x5B), F(0x5C), F(0x5D), F(0x5E), F(0x5F),
  F(0x60), F(0x61), F(0x62), F(0x63), F(0x64), F(0x65), F(0x66), F(0x67),
  F(0x68), F(0x69), F(0x6A), F(0x6B), F(0x6C), F(0x6D), F(0x6E), F(0x6F),
  F(0x70), F(0x71), F(0x72), F(0x73), F(0x74), F(0x75), F(0x76), F(0x77),
  F(0x78), F(0x79), F(0x7A), F(0x7B), F(0x7C), F(0x7D), F(0x7E), F(0x7F),
  F(0x80), F(0x81), F(0x82), F(0x83), F(0x84), F(0x85), F(0x86), F(0x87),
  F(0x88), F(0x89), F(0x8A), F(0x8B), F(0x8C), F(0x8D), F(0x8E), F(0x8F),
  F(0x90), F(0x91), F(0x92), F(0x93), F(0x94), F(0x95), F(0x96), F(0x97),
  F(0x98), F(0x99), F(0x9A), F(0x9B), F(0x9C), F(0x9D), F(0x9E), F(0x9F),
  F(0xA0), F(0xA1), F(0xA2), F(0xA3), F(0xA4), F(0xA5), F(0xA6), F(0xA7),
  F(0xA8), F(0xA9), F(0xAA), F(0xAB), F(0xAC), F(0xAD), F(0xAE), F(0xAF),
  F(0xB0), F(0xB1), F(0xB2), F(0xB3), F(0xB4), F(0xB5), F(0xB6), F(0xB7),
  F(0xB8), F(0xB9), F(0xBA), F(0xBB), F(0xBC), F(0xBD), F(0xBE), F(0xBF),
  F(0xC0), F(0xC1), F(0xC2), F(0xC3), F(0xC4), F(0xC5), F(0xC6), F(0xC7),
  F(0xC8), F(0xC9), F(0xCA), F(0xCB), F(0xCC), F(0xCD), F(0xCE), F(0xCF),
  F(0xD0), F(0xD1), F(0xD2), F(0xD3), F(0xD4), F(0xD5), F(0xD6), F(0xD7),
  F(0xD8), F(0xD9), F(0xDA), F(0xDB), F(0xDC), F(0xDD), F(0xDE), F(0xDF),
  F(0xE0), F(0xE1), F(0xE2), F(0xE3), F(0xE4), F(0xE5), F(0xE6), F(0xE7),
  F(0xE8), F(0xE9), F(0xEA), F(0xEB), F(0xEC), F(0xED), F(0xEE), F(0xEF),
  F(0xF0), F(0xF1), F(0xF2), F(0xF3), F(0xF4), F(0xF5), F(0xF6), F(0xF7),
  F(0xF8), F(0xF9), F(0xFA), F(0xFB), F(0xFC), F(0xFD), F(0xFE), F(0xFF),
};
#undef F


struct instrnode_s {
  int next;     /* ptr to the next instruction if MP_NO_MERGEPT, else -1 */
  int inpaths;  /* number of incoming paths to this point */
  bool mp;      /* set a mergepoint here? */
};

inline int set_merge_point(struct instrnode_s* instrnodes, int instr)
{
  int runaway = 100;
  int bestchoice = instr;
  while (instrnodes[instr].next >= 0 && !instrnodes[instr].mp && --runaway)
    {
      instr = instrnodes[instr].next;
      if (instrnodes[instr].inpaths >= 2)
        bestchoice = instr;
    }
  if (instrnodes[instr].mp)
    return 0;        /* found an already-set merge point */
  else
    {
      extra_assert(!instrnodes[bestchoice].mp);
      instrnodes[bestchoice].mp = true;  /* set merge point */
      return 1;
    }
}

DEFINEFN
PyObject* psyco_build_merge_points(PyCodeObject* co)
{
  PyObject* s;
  mergepoint_t* mp;
  int mp_flags = MP_FLAGS_EXTRA;
  int length = PyString_GET_SIZE(co->co_code);
  unsigned char* source = (unsigned char*) PyString_AS_STRING(co->co_code);
  size_t ibytes = (length+1) * sizeof(struct instrnode_s);
  struct instrnode_s* instrnodes;
  int i, lasti, count, oparg = 0;

  if (length == 0)
    {
      /* normally a code object's code string is never empty,
         but pyexpat.c has some hacks that we have to work around */
      Py_INCREF(Py_None);
      return Py_None;
    }
  instrnodes = (struct instrnode_s*) PyMem_MALLOC(ibytes);
  if (instrnodes == NULL)
    OUT_OF_MEMORY();
  memset(instrnodes, 0, ibytes);

  /* parse the bytecode once, filling the instrnodes[].next/inpaths fields */
  for (i=0; i<length; )
    {
      int i0 = i;
      char flags;
      int jtarget, next;
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
          if (op == SETUP_FINALLY)
            mp_flags |= MP_FLAGS_HAS_FINALLY;
	}
      flags = instr_control_flow[(int) op];
      if (flags == 0)
	if (op != COMPARE_OP || !SUPPORTED_COMPARE_ARG(oparg))
	  {
	    /* unsupported instruction */
            debug_printf(1 + (strcmp(PyCodeObject_NAME(co), "?")==0),
                            ("unsupported opcode %d at %s:%d\n",
                             (int) op, PyCodeObject_NAME(co), i0));
	    PyMem_FREE(instrnodes);
	    Py_INCREF(Py_None);
	    return Py_None;
	  }
      if (flags & (MP_HAS_JREL|MP_HAS_JABS))
        {
          jtarget = oparg;
          if (flags & MP_HAS_JREL)
            jtarget += i;
          if (flags & MP_HAS_J_MULTIPLE)
            instrnodes[jtarget].inpaths = 99;
          else
            instrnodes[jtarget].inpaths++;
        }
      else
        jtarget = -1;  /* no jump target */
      if (flags & MP_IS_JUMP)
        next = jtarget;
      else
        {
          next = i;
          if (flags & MP_IS_CTXDEP)
            instrnodes[next].inpaths = 99;
          else
            instrnodes[next].inpaths++;
        }
      instrnodes[i0].next = (flags & MP_NO_MERGEPT) ? next : -1;
    }

  /* set and count merge points */
  instrnodes[0].mp = true;  /* there is a merge point at the beginning */
  count = 1;
  lasti = 0;
  for (i=0; i<length; i++)
    if (instrnodes[i].inpaths > 0)
      {
        /* set merge points when there are more than one incoming path */
        /* ensure there are merge points at regular intervals */
        if (instrnodes[i].inpaths >= 2 || i-lasti > MAX_UNINTERRUPTED_RANGE)
          {
            count += set_merge_point(instrnodes, i);
            lasti = i;
          }
      }
  
  /* allocate the string buffer, one mergepoint_t per merge point plus
     the room for a final negative bitfield flags. */
  ibytes = count * sizeof(mergepoint_t) + sizeof(int);
  s = PyString_FromStringAndSize(NULL, ibytes);
  if (s == NULL)
    OUT_OF_MEMORY();
  mp = (mergepoint_t*) PyString_AS_STRING(s);

  for (i=0; i<length; i++)
    if (instrnodes[i].mp)
      {
	mp->bytecode_position = i;
	psyco_ge_init(&mp->entries);
	mp++;
      }
  extra_assert(mp - (mergepoint_t*) PyString_AS_STRING(s) == count);
  mp->bytecode_position = mp_flags;
  PyMem_FREE(instrnodes);
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

DEFINEFN
PyObject* psyco_get_merge_points(PyCodeObject* co)
{
  return PyCodeStats_MergePoints(PyCodeStats_Get(co));
}
