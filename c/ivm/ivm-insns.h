 /***************************************************************/
/***    Link to the Prolog-generated instruction producers     ***/
 /***************************************************************/

#ifndef _IVMINSNS_H
#define _IVMINSNS_H

#include "../vcompiler.h"

#ifndef META_ASSERT_DEPTH
# define META_ASSERT_DEPTH        0
#endif

#ifndef VM_STRESS_STACK
# define VM_STRESS_STACK          0
#endif

/* Some tunable run-time virtual machine parameters.
   Keep in mind that each thread has its own VM stack. */
#if !VM_STRESS_STACK
# define VM_INITIAL_MINIMAL_STACK_SIZE      4096
# define VM_EXTRA_STACK_SIZE                8192
# define VM_STACK_SIZE_MARGIN               2048  /* power of 2 */
# define VM_STACK_BLOCK                    16384
#else
# define VM_INITIAL_MINIMAL_STACK_SIZE      512
# define VM_EXTRA_STACK_SIZE                512
# define VM_STACK_SIZE_MARGIN               1024  /* power of 2 */
# define VM_STACK_BLOCK                     2048
#endif


/* The virtual machine uses opcode compression: some sequences of opcodes
   can be compressed into a single opcode which expects all the arguments of
   the individual opcodes. A single, slightly more complicated opcode is
   typically much faster to interpret than several simpler opcodes.

   Each INSN_xxx() macro detects if the most recently generated opcode can
   be combined with the new one. For this purpose, we store an extra byte
   at '*code' which is the most recently emitted opcode. In other words,
   'code' points (as normally) just past the most recently written byte of
   the vm bytecode, but during code generation this bytecode is immediately
   following by an extra byte which is a copy of the most recently emitted
   opcode instruction.
   (This is necessary: the most recent opcode instruction starts just a few
   bytes before '*code', but we don't know how many exactly -- it depends on
   the number of arguments.)
*/

#define LATEST_OPCODE        (extra_assert(*code <= LAST_DEFINED_OPCODE), *code)
#define INIT_CODE_EMISSION(code)         (*(code) = 0)
#define POST_CODEBUFFER_SIZE             1  /* byte for the LATEST_OPCODE */
#define INSN_EMIT_macro_opcode(opcode)   (*code++ = (opcode),           \
                                          *code = (opcode))
#define INSN_CODE_LABEL()                (INIT_CODE_EMISSION(code), code)

typedef long word_t;

#include "prolog/insns-igen-h.i"


/* stack handling */
#define INSNPOPPED(N)   (po->stack_depth -= (N)*sizeof(long))
#define INSNPUSHED(N)   (po->stack_depth += (N)*sizeof(long))

/* meta-instructions */
#define INSN_nv_push(nvsource)    do {                  \
  if (is_compiletime(nvsource))                         \
    INSN_immed(CompileTime_Get(nvsource)->value);       \
  else                                                  \
    INSN_rt_push(nvsource);                             \
} while (0)

#define INSN_rt_push(rtsource)   INSN_s_push(CURRENT_STACK_POSITION(rtsource))
#define INSN_rt_pop(rtsource)    INSN_s_pop(CURRENT_STACK_POSITION(rtsource))
#define INSN_rtcc_push(cc)       do {                           \
  extra_assert((cc) < CC_TOTAL);                                \
  INSN_s_push((po->stack_depth - (cc) + 1) / sizeof(long));     \
  if ((cc) & 1)                                                 \
    INSN_not();                                                 \
} while (0)

#if META_ASSERT_DEPTH
# define META_assertdepth(x)   BEGIN_CODE INSN_assertdepth(x); END_CODE
#else
# define META_assertdepth(x)   /* nothing */
#endif



#endif /* _IVMINSNS_H */
