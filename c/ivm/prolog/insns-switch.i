case 1:  /* [inv] */
{
word_t local1;
word_t local2;
local1 = ~accum;
local2 = local1;
accum = local2;
}
continue;

case 2:  /* [abs_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = abs_o(accum);
local2 = ovf_check(abs_o,macro_args(accum));
local3 = local1;
accum = local3;
flag = local2;
}
continue;

case 3:  /* [neg_o] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = -accum;
local2 = ovf_check(neg_o,macro_args(accum));
local3 = local1;
accum = local3;
flag = local2;
}
continue;

case 4:  /* [load1] */
{
word_t local1;
word_t local2;
local1 = *(char*)accum;
local2 = local1;
accum = local2;
}
continue;

case 5:  /* [load1u] */
{
word_t local1;
word_t local2;
local1 = *(unsigned char*)accum;
local2 = local1;
accum = local2;
}
continue;

case 6:  /* [load2] */
{
word_t local1;
word_t local2;
local1 = *(short*)accum;
local2 = local1;
accum = local2;
}
continue;

case 7:  /* [load2u] */
{
word_t local1;
word_t local2;
local1 = *(unsigned short*)accum;
local2 = local1;
accum = local2;
}
continue;

case 8:  /* [load4] */
{
word_t local1;
word_t local2;
local1 = *(int*)accum;
local2 = local1;
accum = local2;
}
continue;

case 9:  /* [load8] */
{
word_t local1;
word_t local2;
local1 = *(int64_t*)accum;
local2 = local1;
accum = local2;
}
continue;

case 10:  /* [or] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) | accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 11:  /* [and] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) & accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 12:  /* [xor] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) ^ accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 13:  /* [add] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0)+accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 14:  /* [add_o] */
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
}
continue;

case 15:  /* [sub_o] */
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
}
continue;

case 16:  /* [mul_o] */
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
}
continue;

case 17:  /* [lshift] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) << accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 18:  /* [rshift] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) >> accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 19:  /* [urshift] */
{
word_t local1;
word_t local2;
local1 = ((unsigned)stack_nth(0)) >> accum;
local2 = local1;
accum = local2;
stack_shift_pos(1);
}
continue;

case 20:  /* [cmpeq] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) == accum;
local2 = stack_nth(1);
accum = local2;
stack_shift_pos(2);
flag = local1;
}
continue;

case 21:  /* [cmplt] */
{
word_t local1;
word_t local2;
local1 = stack_nth(0) < accum;
local2 = stack_nth(1);
accum = local2;
stack_shift_pos(2);
flag = local1;
}
continue;

case 22:  /* [cmpltu] */
{
word_t local1;
word_t local2;
local1 = ((unsigned)stack_nth(0)) < ((unsigned)accum);
local2 = stack_nth(1);
accum = local2;
stack_shift_pos(2);
flag = local1;
}
continue;

case 23:  /* [settos(0)] */
{
}
continue;

case 24:  /* [settos(1:255)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(code_t);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 25:  /* [settos(1:maxint)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 26:  /* [pushn(char)] */
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
stack_shift_pos(1);
}
continue;

case 27:  /* [pushn(int)] */
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
stack_shift_pos(1);
}
continue;

case 28:  /* [immed(0)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = 0;
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
stack_shift(-1);
}
continue;

case 29:  /* [immed(1)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = 1;
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
stack_shift(-1);
}
continue;

case 30:  /* [immed(char)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(char);
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
stack_shift(-1);
}
continue;

case 31:  /* [immed(int)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
stack_shift(-1);
}
continue;

case 32:  /* [s_push(0)] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = accum;
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
stack_shift(-1);
}
continue;

case 33:  /* [s_push(1:255)] */
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
stack_shift(-1);
}
continue;

case 34:  /* [s_push(1:maxint)] */
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
stack_shift(-1);
}
continue;

case 35:  /* [s_pop(0)] */
{
word_t local1;
accum = accum;
local1 = stack_nth(0);
accum = local1;
stack_shift_pos(1);
}
continue;

case 36:  /* [s_pop(1:255)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(code_t);
stack_nth(local1-1) = accum;
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 37:  /* [s_pop(1:maxint)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
stack_nth(local1-1) = accum;
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 38:  /* [ref_push(char)] */
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
stack_shift(-1);
}
continue;

case 39:  /* [ref_push(int)] */
{
word_t local1;
word_t local2;
word_t local3;
word_t local4;
local1 = bytecode_next(word_t);
local2 = (word_t) &stack_nth(local1-1);
local3 = local2;
local4 = accum;
stack_nth(-1) = local4;
accum = local3;
stack_shift(-1);
}
continue;

case 40:  /* [stackgrow] */
{
impl_stackgrow(VM_EXTRA_STACK_SIZE);
}
continue;

case 41:  /* [assertdepth(char)] */
{
word_t local1;
local1 = bytecode_next(char);
/* debugging assertion */
}
continue;

case 42:  /* [assertdepth(int)] */
{
word_t local1;
local1 = bytecode_next(word_t);
/* debugging assertion */
}
continue;

case 43:  /* [dynamicfreq(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
impl_dynamicfreq;
}
continue;

case 44:  /* [flag_push] */
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
stack_shift(-1);
}
continue;

case 45:  /* [cmpz] */
{
word_t local1;
word_t local2;
local1 = !accum;
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
flag = local1;
}
continue;

case 46:  /* [jcondnear(indirect(code_t))] */
{
word_t local1;
local1 = bytecode_next(code_t);
impl_debug_check_flag(flag);
impl_jcond(flag,nextip+local1);
impl_debug_forget_flag(flag);
}
continue;

case 47:  /* [jcondfar(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
impl_debug_check_flag(flag);
impl_jcond(flag,local1);
impl_debug_forget_flag(flag);
}
continue;

case 48:  /* [jumpfar(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
impl_jump(local1);
}
continue;

case 49:  /* [cbuild1(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
impl_cbuild1(local1);
}
continue;

case 50:  /* [cbuild2(indirect(word_t))] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
impl_cbuild2(local1,accum);
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 51:  /* [store1] */
{
word_t local1;
*(char*)stack_nth(0) = (char)accum;
local1 = stack_nth(1);
accum = local1;
stack_shift_pos(2);
}
continue;

case 52:  /* [store2] */
{
word_t local1;
*(short*)stack_nth(0) = (short)accum;
local1 = stack_nth(1);
accum = local1;
stack_shift_pos(2);
}
continue;

case 53:  /* [store4] */
{
word_t local1;
*(int*)stack_nth(0) = accum;
local1 = stack_nth(1);
accum = local1;
stack_shift_pos(2);
}
continue;

case 54:  /* [store8] */
{
word_t local1;
*(int64_t*)stack_nth(0) = accum;
local1 = stack_nth(1);
accum = local1;
stack_shift_pos(2);
}
continue;

case 55:  /* [incref] */
{
word_t local1;
impl_incref(accum);
local1 = stack_nth(0);
accum = local1;
stack_shift_pos(1);
}
continue;

case 56:  /* [decref] */
{
word_t local1;
impl_decref(accum);
local1 = stack_nth(0);
accum = local1;
stack_shift_pos(1);
}
continue;

case 57:  /* [decrefnz(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
impl_decrefnz(local1);
}
continue;

case 58:  /* [exitframe] */
{
word_t local1;
impl_exitframe(stack_nth(1),stack_nth(0),accum);
local1 = stack_nth(2);
accum = local1;
stack_shift_pos(3);
}
continue;

case 59:  /* [ret(0)] */
{
impl_ret(accum);
}
continue;

case 60:  /* [ret(1:255)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(code_t);
impl_ret(accum);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 61:  /* [ret(1:maxint)] */
{
word_t local1;
word_t local2;
local1 = bytecode_next(word_t);
impl_ret(accum);
stack_shift(local1-1);
local2 = stack_nth(0);
accum = local2;
stack_shift_pos(1);
}
continue;

case 62:  /* [retval] */
{
word_t local1;
retval = accum;
local1 = stack_nth(0);
accum = local1;
stack_shift_pos(1);
}
continue;

case 63:  /* [pushretval] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = retval;
local2 = local1;
local3 = accum;
stack_nth(-1) = local3;
accum = local2;
stack_shift(-1);
}
continue;

case 64:  /* [pyenter(indirect(word_t))] */
{
word_t local1;
local1 = bytecode_next(word_t);
impl_pyenter(local1);
}
continue;

case 65:  /* [pyleave] */
{
impl_pyleave;
}
continue;

case 66:  /* [vmcall(indirect(word_t))] */
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
stack_shift(-1);
}
continue;

case 67:  /* [ccall0(indirect(word_t))] */
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
stack_shift(-1);
}
continue;

case 68:  /* [ccall1(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(1,local1,macro_args(accum));
local3 = local2;
accum = local3;
}
continue;

case 69:  /* [ccall2(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(2,local1,macro_args(accum,stack_nth(0)));
local3 = local2;
accum = local3;
stack_shift_pos(1);
}
continue;

case 70:  /* [ccall3(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(3,local1,macro_args(accum,stack_nth(0),stack_nth(1)));
local3 = local2;
accum = local3;
stack_shift_pos(2);
}
continue;

case 71:  /* [ccall4(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(4,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2)));
local3 = local2;
accum = local3;
stack_shift_pos(3);
}
continue;

case 72:  /* [ccall5(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(5,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2),stack_nth(3)));
local3 = local2;
accum = local3;
stack_shift_pos(4);
}
continue;

case 73:  /* [ccall6(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(6,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2),stack_nth(3),stack_nth(4)));
local3 = local2;
accum = local3;
stack_shift_pos(5);
}
continue;

case 74:  /* [ccall7(indirect(word_t))] */
{
word_t local1;
word_t local2;
word_t local3;
local1 = bytecode_next(word_t);
local2 = impl_ccall(7,local1,macro_args(accum,stack_nth(0),stack_nth(1),stack_nth(2),stack_nth(3),stack_nth(4),stack_nth(5)));
local3 = local2;
accum = local3;
stack_shift_pos(6);
}
continue;

case 75:  /* [checkdict(indirect(word_t),indirect(word_t),indirect(word_t),indirect(word_t))] */
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
}
continue;

