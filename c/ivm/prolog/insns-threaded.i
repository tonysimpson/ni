
#ifdef TRACE
#define VM_TRACE(name)          \
        fprintf(stderr, "VM_TRACE %s %ld\n", name, accum)
#else
#define VM_TRACE(name)
#endif

static const void* opcodetable[] = {
  &&lblorigin,    /* 0 */
  &&lblopcode1,  /* [inv] */
  &&lblopcode2,  /* [abs_o] */
  &&lblopcode3,  /* [neg_o] */
  &&lblopcode4,  /* [load1] */
  &&lblopcode5,  /* [load1u] */
  &&lblopcode6,  /* [load2] */
  &&lblopcode7,  /* [load2u] */
  &&lblopcode8,  /* [load4] */
  &&lblopcode9,  /* [load8] */
  &&lblopcode10,  /* [or] */
  &&lblopcode11,  /* [and] */
  &&lblopcode12,  /* [xor] */
  &&lblopcode13,  /* [add] */
  &&lblopcode14,  /* [add_o] */
  &&lblopcode15,  /* [sub_o] */
  &&lblopcode16,  /* [mul_o] */
  &&lblopcode17,  /* [lshift] */
  &&lblopcode18,  /* [rshift] */
  &&lblopcode19,  /* [urshift] */
  &&lblopcode20,  /* [cmpeq] */
  &&lblopcode21,  /* [cmplt] */
  &&lblopcode22,  /* [cmpltu] */
  &&lblopcode23,  /* [settos(0)] */
  &&lblopcode24,  /* [settos(1:255)] */
  &&lblopcode25,  /* [settos(1:maxint)] */
  &&lblopcode26,  /* [pushn(char)] */
  &&lblopcode27,  /* [pushn(int)] */
  &&lblopcode28,  /* [immed(0)] */
  &&lblopcode29,  /* [immed(1)] */
  &&lblopcode30,  /* [immed(char)] */
  &&lblopcode31,  /* [immed(int)] */
  &&lblopcode32,  /* [s_push(0)] */
  &&lblopcode33,  /* [s_push(1:255)] */
  &&lblopcode34,  /* [s_push(1:maxint)] */
  &&lblopcode35,  /* [s_pop(0)] */
  &&lblopcode36,  /* [s_pop(1:255)] */
  &&lblopcode37,  /* [s_pop(1:maxint)] */
  &&lblopcode38,  /* [ref_push(char)] */
  &&lblopcode39,  /* [ref_push(int)] */
  &&lblopcode40,  /* [stackgrow] */
  &&lblopcode41,  /* [assertdepth(char)] */
  &&lblopcode42,  /* [assertdepth(int)] */
  &&lblopcode43,  /* [dynamicfreq(indirect(word_t))] */
  &&lblopcode44,  /* [flag_push] */
  &&lblopcode45,  /* [cmpz] */
  &&lblopcode46,  /* [jcondnear(indirect(code_t))] */
  &&lblopcode47,  /* [jcondfar(indirect(word_t))] */
  &&lblopcode48,  /* [jumpfar(indirect(word_t))] */
  &&lblopcode49,  /* [cbuild1(indirect(word_t))] */
  &&lblopcode50,  /* [cbuild2(indirect(word_t))] */
  &&lblopcode51,  /* [store1] */
  &&lblopcode52,  /* [store2] */
  &&lblopcode53,  /* [store4] */
  &&lblopcode54,  /* [store8] */
  &&lblopcode55,  /* [incref] */
  &&lblopcode56,  /* [decref] */
  &&lblopcode57,  /* [decrefnz(indirect(word_t))] */
  &&lblopcode58,  /* [exitframe] */
  &&lblopcode59,  /* [ret(0)] */
  &&lblopcode60,  /* [ret(1:255)] */
  &&lblopcode61,  /* [ret(1:maxint)] */
  &&lblopcode62,  /* [retval] */
  &&lblopcode63,  /* [pushretval] */
  &&lblopcode64,  /* [pyenter(indirect(word_t))] */
  &&lblopcode65,  /* [pyleave] */
  &&lblopcode66,  /* [vmcall(indirect(word_t))] */
  &&lblopcode67,  /* [ccall0(indirect(word_t))] */
  &&lblopcode68,  /* [ccall1(indirect(word_t))] */
  &&lblopcode69,  /* [ccall2(indirect(word_t))] */
  &&lblopcode70,  /* [ccall3(indirect(word_t))] */
  &&lblopcode71,  /* [ccall4(indirect(word_t))] */
  &&lblopcode72,  /* [ccall5(indirect(word_t))] */
  &&lblopcode73,  /* [ccall6(indirect(word_t))] */
  &&lblopcode74,  /* [ccall7(indirect(word_t))] */
  &&lblopcode75,  /* [checkdict(indirect(word_t),indirect(word_t),indirect(word_t),indirect(word_t))] */
#if PSYCO_DEBUG
  &&lblorigin,    /* 76 */
  &&lblorigin,    /* 77 */
  &&lblorigin,    /* 78 */
  &&lblorigin,    /* 79 */
  &&lblorigin,    /* 80 */
  &&lblorigin,    /* 81 */
  &&lblorigin,    /* 82 */
  &&lblorigin,    /* 83 */
  &&lblorigin,    /* 84 */
  &&lblorigin,    /* 85 */
  &&lblorigin,    /* 86 */
  &&lblorigin,    /* 87 */
  &&lblorigin,    /* 88 */
  &&lblorigin,    /* 89 */
  &&lblorigin,    /* 90 */
  &&lblorigin,    /* 91 */
  &&lblorigin,    /* 92 */
  &&lblorigin,    /* 93 */
  &&lblorigin,    /* 94 */
  &&lblorigin,    /* 95 */
  &&lblorigin,    /* 96 */
  &&lblorigin,    /* 97 */
  &&lblorigin,    /* 98 */
  &&lblorigin,    /* 99 */
  &&lblorigin,    /* 100 */
  &&lblorigin,    /* 101 */
  &&lblorigin,    /* 102 */
  &&lblorigin,    /* 103 */
  &&lblorigin,    /* 104 */
  &&lblorigin,    /* 105 */
  &&lblorigin,    /* 106 */
  &&lblorigin,    /* 107 */
  &&lblorigin,    /* 108 */
  &&lblorigin,    /* 109 */
  &&lblorigin,    /* 110 */
  &&lblorigin,    /* 111 */
  &&lblorigin,    /* 112 */
  &&lblorigin,    /* 113 */
  &&lblorigin,    /* 114 */
  &&lblorigin,    /* 115 */
  &&lblorigin,    /* 116 */
  &&lblorigin,    /* 117 */
  &&lblorigin,    /* 118 */
  &&lblorigin,    /* 119 */
  &&lblorigin,    /* 120 */
  &&lblorigin,    /* 121 */
  &&lblorigin,    /* 122 */
  &&lblorigin,    /* 123 */
  &&lblorigin,    /* 124 */
  &&lblorigin,    /* 125 */
  &&lblorigin,    /* 126 */
  &&lblorigin,    /* 127 */
  &&lblorigin,    /* 128 */
  &&lblorigin,    /* 129 */
  &&lblorigin,    /* 130 */
  &&lblorigin,    /* 131 */
  &&lblorigin,    /* 132 */
  &&lblorigin,    /* 133 */
  &&lblorigin,    /* 134 */
  &&lblorigin,    /* 135 */
  &&lblorigin,    /* 136 */
  &&lblorigin,    /* 137 */
  &&lblorigin,    /* 138 */
  &&lblorigin,    /* 139 */
  &&lblorigin,    /* 140 */
  &&lblorigin,    /* 141 */
  &&lblorigin,    /* 142 */
  &&lblorigin,    /* 143 */
  &&lblorigin,    /* 144 */
  &&lblorigin,    /* 145 */
  &&lblorigin,    /* 146 */
  &&lblorigin,    /* 147 */
  &&lblorigin,    /* 148 */
  &&lblorigin,    /* 149 */
  &&lblorigin,    /* 150 */
  &&lblorigin,    /* 151 */
  &&lblorigin,    /* 152 */
  &&lblorigin,    /* 153 */
  &&lblorigin,    /* 154 */
  &&lblorigin,    /* 155 */
  &&lblorigin,    /* 156 */
  &&lblorigin,    /* 157 */
  &&lblorigin,    /* 158 */
  &&lblorigin,    /* 159 */
  &&lblorigin,    /* 160 */
  &&lblorigin,    /* 161 */
  &&lblorigin,    /* 162 */
  &&lblorigin,    /* 163 */
  &&lblorigin,    /* 164 */
  &&lblorigin,    /* 165 */
  &&lblorigin,    /* 166 */
  &&lblorigin,    /* 167 */
  &&lblorigin,    /* 168 */
  &&lblorigin,    /* 169 */
  &&lblorigin,    /* 170 */
  &&lblorigin,    /* 171 */
  &&lblorigin,    /* 172 */
  &&lblorigin,    /* 173 */
  &&lblorigin,    /* 174 */
  &&lblorigin,    /* 175 */
  &&lblorigin,    /* 176 */
  &&lblorigin,    /* 177 */
  &&lblorigin,    /* 178 */
  &&lblorigin,    /* 179 */
  &&lblorigin,    /* 180 */
  &&lblorigin,    /* 181 */
  &&lblorigin,    /* 182 */
  &&lblorigin,    /* 183 */
  &&lblorigin,    /* 184 */
  &&lblorigin,    /* 185 */
  &&lblorigin,    /* 186 */
  &&lblorigin,    /* 187 */
  &&lblorigin,    /* 188 */
  &&lblorigin,    /* 189 */
  &&lblorigin,    /* 190 */
  &&lblorigin,    /* 191 */
  &&lblorigin,    /* 192 */
  &&lblorigin,    /* 193 */
  &&lblorigin,    /* 194 */
  &&lblorigin,    /* 195 */
  &&lblorigin,    /* 196 */
  &&lblorigin,    /* 197 */
  &&lblorigin,    /* 198 */
  &&lblorigin,    /* 199 */
  &&lblorigin,    /* 200 */
  &&lblorigin,    /* 201 */
  &&lblorigin,    /* 202 */
  &&lblorigin,    /* 203 */
  &&lblorigin,    /* 204 */
  &&lblorigin,    /* 205 */
  &&lblorigin,    /* 206 */
  &&lblorigin,    /* 207 */
  &&lblorigin,    /* 208 */
  &&lblorigin,    /* 209 */
  &&lblorigin,    /* 210 */
  &&lblorigin,    /* 211 */
  &&lblorigin,    /* 212 */
  &&lblorigin,    /* 213 */
  &&lblorigin,    /* 214 */
  &&lblorigin,    /* 215 */
  &&lblorigin,    /* 216 */
  &&lblorigin,    /* 217 */
  &&lblorigin,    /* 218 */
  &&lblorigin,    /* 219 */
  &&lblorigin,    /* 220 */
  &&lblorigin,    /* 221 */
  &&lblorigin,    /* 222 */
  &&lblorigin,    /* 223 */
  &&lblorigin,    /* 224 */
  &&lblorigin,    /* 225 */
  &&lblorigin,    /* 226 */
  &&lblorigin,    /* 227 */
  &&lblorigin,    /* 228 */
  &&lblorigin,    /* 229 */
  &&lblorigin,    /* 230 */
  &&lblorigin,    /* 231 */
  &&lblorigin,    /* 232 */
  &&lblorigin,    /* 233 */
  &&lblorigin,    /* 234 */
  &&lblorigin,    /* 235 */
  &&lblorigin,    /* 236 */
  &&lblorigin,    /* 237 */
  &&lblorigin,    /* 238 */
  &&lblorigin,    /* 239 */
  &&lblorigin,    /* 240 */
  &&lblorigin,    /* 241 */
  &&lblorigin,    /* 242 */
  &&lblorigin,    /* 243 */
  &&lblorigin,    /* 244 */
  &&lblorigin,    /* 245 */
  &&lblorigin,    /* 246 */
  &&lblorigin,    /* 247 */
  &&lblorigin,    /* 248 */
  &&lblorigin,    /* 249 */
  &&lblorigin,    /* 250 */
  &&lblorigin,    /* 251 */
  &&lblorigin,    /* 252 */
  &&lblorigin,    /* 253 */
  &&lblorigin,    /* 254 */
  &&lblorigin,    /* 255 */
#endif
};
goto *opcodetable[bytecode_nextopcode()];

lblorigin:
psyco_fatal_msg("invalid vm opcode");

lblopcode1:  /* [inv] */
{
word_t local1;
word_t local2;
local1 = ~accum;
local2 = local1;
accum = local2;
VM_TRACE("inv");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode2:  /* [abs_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = abs_o(accum);
local2 = ovf_check(abs_o,macro_args(accum));
local3 = local1;
accum = local3;
flag = local2;
VM_TRACE("abs_o");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode3:  /* [neg_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = -accum;
local2 = ovf_check(neg_o,macro_args(accum));
local3 = local1;
accum = local3;
flag = local2;
VM_TRACE("neg_o");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode4:  /* [load1] */
{
word_t local1;
word_t local2;
local1 = *(char*)accum;
local2 = local1;
accum = local2;
VM_TRACE("load1");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode5:  /* [load1u] */
{
word_t local1;
word_t local2;
local1 = *(unsigned char*)accum;
local2 = local1;
accum = local2;
VM_TRACE("load1u");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode6:  /* [load2] */
{
word_t local1;
word_t local2;
local1 = *(short*)accum;
local2 = local1;
accum = local2;
VM_TRACE("load2");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode7:  /* [load2u] */
{
word_t local1;
word_t local2;
local1 = *(unsigned short*)accum;
local2 = local1;
accum = local2;
VM_TRACE("load2u");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode8:  /* [load4] */
{
word_t local1;
word_t local2;
local1 = *(int*)accum;
local2 = local1;
accum = local2;
VM_TRACE("load4");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode9:  /* [load8] */
{
word_t local1;
word_t local2;
local1 = *(word_t*)accum;
local2 = local1;
accum = local2;
VM_TRACE("load8");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode10:  /* [or] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) | accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
VM_TRACE("or");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode11:  /* [and] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) & accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
VM_TRACE("and");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode12:  /* [xor] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) ^ accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
VM_TRACE("xor");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode13:  /* [add] */
{
accum = stack_pop() + accum;
VM_TRACE("add");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode14:  /* [add_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = stack_nth(0)+accum;
local2 = ovf_check(add_o,macro_args(stack_nth(0),accum));
local3 = local1;
accum = local3;
stack_shift_pos(1);
flag = local2;
VM_TRACE("add_o");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode15:  /* [sub_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = stack_nth(0)-accum;
local2 = ovf_check(sub_o,macro_args(stack_nth(0),accum));
local3 = local1;
accum = local3;
stack_shift_pos(1);
flag = local2;
VM_TRACE("sub_o");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode16:  /* [mul_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = stack_nth(0)*accum;
local2 = ovf_check(mul_o,macro_args(stack_nth(0),accum));
local3 = local1;
accum = local3;
stack_shift_pos(1);
flag = local2;
VM_TRACE("mul_o");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode17:  /* [lshift] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) << accum;
local2 = local1;
accum = local2;
VM_TRACE("lshift");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode18:  /* [rshift] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) >> accum;
local2 = local1;
accum = local2;
VM_TRACE("rshift");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode19:  /* [urshift] */
{
word_t local1;
word_t local2;
local1 = ((unsigned)stack_nth(0)) >> accum;
local2 = local1;
accum = local2;
VM_TRACE("urshift");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode20:  /* [cmpeq] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) == accum;
local2 = stack_nth(1);
accum = local2;
stack_shift_pos(2);
flag = local1;
VM_TRACE("cmpeq");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode21:  /* [cmplt] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) < accum;
local2 = stack_nth(1);
accum = local2;
stack_shift_pos(2);
flag = local1;
VM_TRACE("cmplt");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode22:  /* [cmpltu] */
{
word_t local1;
word_t local2;
local1 = ((unsigned)stack_nth(0)) < ((unsigned)accum);
local2 = stack_nth(1);
accum = local2;
stack_shift_pos(2);
flag = local1;
VM_TRACE("cmpltu");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode23:  /* [settos(0)] */
{
VM_TRACE("settos(0)");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode24:  /* [settos(1:255)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(code_t);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
VM_TRACE("settos(1:255)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode25:  /* [settos(1:maxint)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
VM_TRACE("settos(1:maxint)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode26:  /* [pushn(char)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(char);
local2 = accum;
stack_nth(-1) = local2;
stack_shift(-1);
stack_shift((-local1));
local3 = stack_nth(0);
accum = local3;
VM_TRACE("pushn(char)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode27:  /* [pushn(int)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = accum;
stack_nth(-1) = local2;
stack_shift(-1);
stack_shift((-local1));
local3 = stack_nth(0);
accum = local3;
VM_TRACE("pushn(int)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode28:  /* [immed(0)] */
{
stack_push(accum);
accum = 0;
VM_TRACE("immed(0)");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode29:  /* [immed(1)] */
{
stack_push(accum);
accum = 1;
VM_TRACE("immed(1)");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode30:  /* [immed(char)] */
{
stack_push(accum);
accum = bytecode_next(char);
VM_TRACE("immed(char)");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode31:  /* [immed(word_t)] */
{
stack_push(accum);
accum = bytecode_next(word_t);
VM_TRACE("immed(int)");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode32:  /* [s_push(0)] */
{
stack_push(accum);
VM_TRACE("s_push(0)");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode33:  /* [s_push(1:255)] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(code_t);
local2 = stack_nth(local1-1);
local3 = local2;
local4 = accum;
stack_nth(-1) = local4;
accum = local3;
VM_TRACE("s_push(1:255)");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode34:  /* [s_push(1:maxint)] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(word_t);
local2 = stack_nth(local1-1);
local3 = local2;
local4 = accum;
stack_nth(-1) = local4;
accum = local3;
VM_TRACE("s_push(1:maxint)");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode35:  /* [s_pop(0)] */
{
word_t local1;
accum = accum;
local1 = stack_nth(0);
accum = local1;
VM_TRACE("s_pop(0)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode36:  /* [s_pop(1:255)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(code_t);
stack_nth(local1-1) = accum;
local2 = stack_nth(0);
accum = local2;
VM_TRACE("s_pop(1:255)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode37:  /* [s_pop(1:maxint)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
stack_nth(local1-1) = accum;
local2 = stack_nth(0);
accum = local2;
VM_TRACE("s_pop(1:maxint)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode38:  /* [ref_push(char)] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(char);
local2 = (word_t) &stack_nth(local1-1);
local3 = local2;
local4 = accum;
stack_nth(-1) = local4;
accum = local3;
VM_TRACE("ref_push(char)");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode39:  /* [ref_push(int)] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(word_t);
local2 = (word_t) &stack_nth(local1-1);
local3 = local2;
local4 = accum;
VM_TRACE("ref_push(int)");
stack_nth(-1) = local4;
accum = local3;
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode40:  /* [stackgrow] */
{
VM_TRACE("stackgrow");
impl_stackgrow(VM_EXTRA_STACK_SIZE);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode41:  /* [assertdepth(char)] */
{
word_t local1;
local1 = bytecode_next(char);
VM_TRACE("assertdepth");
/* debugging assertion */
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode42:  /* [assertdepth(int)] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("assertdepth");
/* debugging assertion */
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode43:  /* [dynamicfreq(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("dynamicfreq");
impl_dynamicfreq;
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode44:  /* [flag_push] */
{
word_t local1;
word_t local2;
word_t local3;
impl_debug_check_flag(flag);
local1 = flag;
impl_debug_forget_flag(flag);
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
VM_TRACE("flag_push");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode45:  /* [cmpz] */
{
word_t local1;
word_t local2;
local1 = !accum;
local2 = stack_nth(0);
accum = local2;
VM_TRACE("cmpz");
stack_shift_pos(1);
flag = local1;
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode46:  /* [jcondnear(indirect(code_t))] */
{
word_t local1;
local1 = bytecode_next(code_t);
VM_TRACE("jcondnear");
impl_debug_check_flag(flag);
impl_jcond(flag,nextip+local1);
impl_debug_forget_flag(flag);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode47:  /* [jcondfar(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("jcondfar");
impl_debug_check_flag(flag);
impl_jcond(flag,local1);
impl_debug_forget_flag(flag);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode48:  /* [jumpfar(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("jumpfar");
impl_jump(local1);
}
goto *opcodetable[bytecode_nextopcode()];


lblopcode49:  /* [cbuild1(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("cbuild1");
impl_cbuild1(local1);
}
goto *opcodetable[bytecode_nextopcode()];


lblopcode50:  /* [cbuild2(indirect(word_t))] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
impl_cbuild2(local1,accum);
local2 = stack_nth(0);
accum = local2;
VM_TRACE("cbuild2");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode51:  /* [store1] */
{
word_t local1;
*(char*)stack_nth(0) = (char)accum;
local1 = stack_nth(1);
accum = local1;
VM_TRACE("store1");
stack_shift_pos(2);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode52:  /* [store2] */
{
word_t local1;
*(short*)stack_nth(0) = (short)accum;
local1 = stack_nth(1);
accum = local1;
VM_TRACE("store2");
stack_shift_pos(2);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode53:  /* [store4] */
{
word_t local1;
*(int*)stack_nth(0) = accum;
local1 = stack_nth(1);
accum = local1;
VM_TRACE("store4");
stack_shift_pos(2);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode54:  /* [store8] */
{
word_t local1;
*(int64_t*)stack_nth(0) = accum;
local1 = stack_nth(1);
accum = local1;
VM_TRACE("store8");
stack_shift_pos(2);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode55:  /* [incref] */
{
word_t local1;
impl_incref(accum);
local1 = stack_nth(0);
accum = local1;
VM_TRACE("incref");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode56:  /* [decref] */
{
word_t local1;
impl_decref(accum);
local1 = stack_nth(0);
accum = local1;
VM_TRACE("decref");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode57:  /* [decrefnz(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("decrefnz");
impl_decrefnz(local1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode58:  /* [exitframe] */
{
impl_exitframe(stack_nth(1),stack_nth(0),accum);
accum = stack_nth(2);
stack_shift_pos(3);
VM_TRACE("exitframe");
/*
#define impl_exitframe(tb, val, exc)  stack_savesp();               \
                                      if (exc) cimpl_finalize_frame_locals( \
                                                   (PyObject*) exc,     \
                                                   (PyObject*) val,     \
                                                   (PyObject*) tb)

    PyObject* exception = accum;
    stack_savesp();
    if (exception) {
        PyObject* val = stack_pop();
        PyObject* tb = stack_pop();
        cimpl_finalize_frame_locals(exception, val, tb);
    }
    accum = stack_pop();
    VM_TRACE("exitframe");
*/
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode59:  /* [ret(0)] */
{
VM_TRACE("ret(0)");
impl_ret(accum);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode60:  /* [ret(1:255)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(code_t);
impl_ret(accum);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
VM_TRACE("ret(1:255)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode61:  /* [ret(1:maxint)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
impl_ret(accum);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
VM_TRACE("ret(1:maxint)");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode62:  /* [retval] */
{
word_t local1;
retval = accum;
local1 = stack_nth(0);
accum = local1;
VM_TRACE("retval");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode63:  /* [pushretval] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = retval;
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
VM_TRACE("pushretval");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode64:  /* [pyenter(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
VM_TRACE("pyenter");
impl_pyenter(local1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode65:  /* [pyleave] */
{
VM_TRACE("pyleave");
impl_pyleave;
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode66:  /* [vmcall(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(word_t);
local2 = impl_vmcall(local1);
impl_stackgrow(VM_INITIAL_MINIMAL_STACK_SIZE);
local3 = local2;
local4 = accum;
stack_nth(-1) = local4;
accum = local3;
VM_TRACE("vmcall");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode67:  /* [ccall0(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(word_t);
local2 = impl_ccall(0,local1,macro_noarg);
local3 = local2;
local4 = accum;
stack_nth(-1) = local4;
accum = local3;
VM_TRACE("ccall0");
stack_shift(-1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode68:  /* [ccall1(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(1,local1,macro_args(accum));
local3 = local2;
accum = local3;
VM_TRACE("ccall1");
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode69:  /* [ccall2(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(2,local1,macro_args(accum,stack_nth(0)));
local3 = local2;
accum = local3;
VM_TRACE("ccall2");
stack_shift_pos(1);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode70:  /* [ccall3(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(3,local1,macro_args(accum,stack_nth(0),stack_nth(1)));
local3 = local2;
accum = local3;
VM_TRACE("ccall3");
stack_shift_pos(2);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode71:  /* [ccall4(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(4,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2)));
local3 = local2;
accum = local3;
VM_TRACE("ccall4");
stack_shift_pos(3);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode72:  /* [ccall5(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(5,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2),stack_nth(3)));
local3 = local2;
accum = local3;
VM_TRACE("ccall5");
stack_shift_pos(4);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode73:  /* [ccall6(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(6,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2),stack_nth(3),stack_nth(4)));
local3 = local2;
accum = local3;
VM_TRACE("ccall6");
stack_shift_pos(5);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode74:  /* [ccall7(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(7,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2),stack_nth(3),stack_nth(4),stack_nth(5)));
local3 = local2;
accum = local3;
VM_TRACE("ccall7");
stack_shift_pos(6);
}
goto *opcodetable[bytecode_nextopcode()];

lblopcode75:  /* [checkdict(indirect(word_t),indirect(word_t),indirect(word_t),indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
word_t local5;
local1 = bytecode_next(word_t);
local2 = bytecode_next(word_t);
local3 = bytecode_next(word_t);
local4 = bytecode_next(word_t);
local5 = impl_checkdict(local1,local2,local3,local4);
flag = local5;
VM_TRACE("checkdict");
}
goto *opcodetable[bytecode_nextopcode()];

