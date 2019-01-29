import distorm3
import pointbreak
from pointbreak import types
from itertools import count
import intervaltree
from collections import namedtuple

Py_ssize_t = types.int64
PyTypeObject_pointer = types.pointer_type(None)
PyObject_HEAD = [
    ('ob_refcnt', Py_ssize_t),
    ('ob_type', PyTypeObject_pointer)
]
PyObject_VAR_HEAD = PyObject_HEAD + [
    ('ob_size', Py_ssize_t)
]
PyTypeObject = types.struct_type(
    *PyObject_VAR_HEAD + [
        ('tp_name', types.c_string_pointer)
    ]
    # incomplete - but that's ok for our use
)
PyStringObject = types.struct_type(
    *PyObject_VAR_HEAD + [
        ('ob_shash', types.int64),
        ('ob_sstate', types.int32),
        ('ob_sval', types.offset()),
    ]
)


def pystringobject_to_str(s):
    return s.ob_sval.read(s.ob_size)


PyTypeObject_pointer.referenced_type = PyTypeObject
PyObject = types.struct_type(*PyObject_HEAD)
PyVarObject = types.struct_type(*PyObject_VAR_HEAD)
PyObject_pointer = types.pointer_type(PyObject)
PyStringObject_pointer = types.pointer_type(PyStringObject)
PyCodeObject = types.struct_type(*PyObject_HEAD + [
    ('co_argcount', types.int32),
    ('co_nlocals', types.int32),
    ('co_stacksize', types.int32),
    ('co_flags', types.int32),
    ('co_code', PyStringObject_pointer),
    ('co_consts', PyObject_pointer),
    ('co_names', PyObject_pointer),
    ('co_varnames', PyObject_pointer),
    ('co_freevars', PyObject_pointer),
    ('co_cellvars', PyObject_pointer),
    ('co_filename', PyStringObject_pointer),
    ('co_name', PyStringObject_pointer),
    ('co_firstlineno', types.int32),
    ('co_lnotab', PyStringObject_pointer),
    ('co_zombieframe', types.uint64),
    ('co_weakreflist', PyObject_pointer)
])
PyFrameObject_pointer = types.pointer_type(None)
PyFrameObject = types.struct_type(
    *PyObject_VAR_HEAD + [
        ('f_code', types.pointer_type(PyCodeObject)),
        ('f_back', PyFrameObject_pointer)
    ]
    #incomplete

)
CodeBufferObject = types.struct_type(
    *PyObject_HEAD + [
        ('codestart', types.uint64),
    ]
)
PyFrameObject_pointer.referenced_type = PyFrameObject
vinfo_t_pointer = types.pointer_type(None)
vinfo_array_t = types.struct_type(
    ('count', types.int32),
    ('items', types.array_type(7, vinfo_t_pointer, checked=False))
)
vinfo_t = types.struct_type(
    ('refcount', types.int32),
    ('source', types.uint64),
    ('array', types.pointer_type(vinfo_array_t)),
    ('tmp', vinfo_t_pointer),
)
vinfo_t_pointer.referenced_type = vinfo_t
PyTryBlock = types.struct_type(
    ('b_type', types.int32),
    ('b_handler', types.int32),
    ('b_level', types.int32),
)
CO_MAXBLOCKS = 20
pyc_data_t = types.struct_type(
    ('co', types.pointer_type(PyCodeObject)),
    ('next_instr', types.int32),
    ('auto_recursion', types.int16),
    ('is_inlining', types.int8),
    ('iblock', types.uint8),
    ('blockstack', types.array_type(CO_MAXBLOCKS, PyTryBlock)),
    ('stack_base', types.int32),
    ('stack_level', types.int32),
    ('merge_points', types.uint64),
    ('exc', vinfo_t),
    ('val', vinfo_t),
    ('tb', vinfo_t),
    ('f_builtins', types.uint64),
    ('changing_globals', types.uint64),
)
reg_t = types.int32 # XXX - an enum type?
PsycoObject = types.struct_type(
    ('code', types.uint64),
    ('codelimit', types.uint64),
    ('stack_depth', types.int32),
    ('reg_array', types.array_type(16, vinfo_t_pointer)),
    ('ccregs', types.array_type(2, vinfo_t_pointer)),
    ('last_used_reg', reg_t),
    ('respawn_cnt', types.int32),
    ('respawn_proxy', types.uint64),
    ('pr', pyc_data_t),
    ('vlocals', vinfo_array_t)
)


def psycoobject_get_python_location(po_ptr):
    pr = po_ptr.value.pr
    if not pr.co:
        return None
    inst = pr.next_instr
    co = pr.co.value
    filename = pystringobject_to_str(co.co_filename.value)
    name = pystringobject_to_str(co.co_name.value)
    if inst < 0:
        line = -1
    else:
        lnotab = pystringobject_to_str(co.co_lnotab.value)
        line = co.co_firstlineno
        inst_offset = 0
        for i in range(0, len(lnotab), 2):
            inst_offset += ord(lnotab[i])
            if inst_offset > inst:
                break
            line += ord(lnotab[i+1])
    return (filename, line, name)


REG_NUM_MAP = {
    0: 'rax',
    1: 'rcx',
    2: 'rdx',
    3: 'rbx',
    4: 'rsp',
    5: 'rbp',
    6: 'rsi',
    7: 'rdi',
    8: 'r8',
    9: 'r9',
    10: 'r10',
    11: 'r11',
    12: 'r12',
    13: 'r13',
    14: 'r14',
    15: 'r15',
}


generated_code = namedtuple('generated_code', ['where', 'python_src', 'stack_depth'])


class SimpleExecutionTracer:
    def __init__(self, db):
        self.db = db
        self._trace = intervaltree.IntervalTree()
        self._initialise_breakpoints()
        self.code_gen = intervaltree.IntervalTree()
        self.end_code_callback = None

    def _initialise_breakpoints(self):
        self.db.add_breakpoint('ni_trace_begin_code', self._ni_trace_begin_code)
        self.db.add_breakpoint('ni_trace_end_code', self._ni_trace_end_code)
        self.db.add_breakpoint('ni_trace_jump', self._ni_trace_jump)
        self.db.add_breakpoint('ni_trace_jump_update', self._ni_trace_jump)
        self.db.add_breakpoint('ni_trace_jump_reg', self._ni_trace_jump_reg)
        self.db.add_breakpoint('ni_trace_jump_cond', self._ni_trace_jump_cond)
        self.db.add_breakpoint('ni_trace_jump_cond_update', self._ni_trace_jump_cond)
        self.db.add_breakpoint('ni_trace_jump_cond_reg', self._ni_trace_jump_cond_reg)
        self.db.add_breakpoint('ni_trace_call', self._ni_trace_call)
        self.db.add_breakpoint('ni_trace_call_reg', self._ni_trace_call_reg)
        self.db.add_breakpoint('ni_trace_return', self._ni_trace_return)

    def _ni_trace_begin_code(self, db):
        po_ptr = db.reference(db.registers.rdi, PsycoObject)
        self._begin = po_ptr.value.code
        self._trace_points = {}
        self._begin_stack_depth = po_ptr.value.stack_depth
        return True

    def _ni_trace_end_code(self, db):
        po_ptr = db.reference(db.registers.rdi, PsycoObject)
        end = po_ptr.value.code
        self._trace.chop(self._begin, end)
        for address, function in self._trace_points.items():
            self._trace[address:address+1] = function
        self.code_gen.chop(self._begin, end)
        g = generated_code([i.function_name for i in db.backtrace()], psycoobject_get_python_location(po_ptr), self._begin_stack_depth)
        self.code_gen[self._begin:end] = g
        if self.end_code_callback:
            self.end_code_callback(self._begin, end, g)
        return True

    def _ni_trace_jump(self, db):
        location = db.registers.rdi
        target = db.registers.rsi
        def location_func(db, trace):
            trace.jump('jump', location, target)
            return True
        self._trace_points[location] = location_func
        return True

    def _ni_trace_jump_reg(self, db):
        location = db.registers.rdi
        target_reg = db.registers.rsi
        def location_func(db, trace):
            trace.jump('jumpr', location, getattr(db.registers, REG_NUM_MAP[target_reg]))
            return True
        self._trace_points[location] = location_func
        return True

    def _ni_trace_jump_cond(self, db):
        location = db.registers.rdi
        not_taken = db.registers.rsi
        taken = db.registers.rdx
        def location_func(db, trace):
            def taken_func(db):
                trace.jump('jumpc', location, taken)
                db.remove_breakpoint(taken_func.other_breakpoint)
                return False
            def not_taken_func(db):
                db.remove_breakpoint(not_taken_func.other_breakpoint)
                return False
            taken_func.other_breakpoint = db.add_breakpoint(not_taken, not_taken_func, immediately=True)
            not_taken_func.other_breakpoint = db.add_breakpoint(taken, taken_func, immediately=True)
            return True
        self._trace_points[location] = location_func
        return True

    def _ni_trace_jump_cond_reg(self, db):
        location = db.registers.rdi
        not_taken = db.registers.rsi
        taken_reg = db.registers.rdx
        def location_func(db, trace):
            taken = getattr(db.registers, REG_NUM_MAP[target_reg])
            def taken_func(db):
                trace.jump('jumpcr', location, taken)
                db.remove_breakpoint(taken_func.other_breakpoint)
                return False
            def not_taken_func(db):
                db.remove_breakpoint(not_taken_func.other_breakpoint)
                return False
            taken_func.other_breakpoint = db.add_breakpoint(not_taken, not_taken_func, immediately=True)
            not_taken_func.other_breakpoint = db.add_breakpoint(taken, taken_func, immediately=True)
            return True
        self._trace_points[location] = location_func
        return True

    def _ni_trace_call(self, db):
        location = db.registers.rdi
        call_target = db.registers.rsi
        def location_func(db, trace):
            trace.jump('calli', location, call_target)
            return True
        self._trace_points[location] = location_func
        return True

    def _ni_trace_call_reg(self, db):
        location = db.registers.rdi
        call_target_reg = db.registers.rsi
        def location_func(db, trace):
            trace.jump('callr', location, getattr(db.registers, REG_NUM_MAP[call_target_reg]))
            return True
        self._trace_points[location] = location_func
        return True

    def _ni_trace_return(self, db):
        location = db.registers.rdi
        stack_adjust = db.registers.rsi
        def location_func(db, trace):
            trace.jump('return', location, db.stack[stack_adjust])
            return True
        self._trace_points[location] = location_func
        return True

    def _py_eval_eval_frame_ex(self, db):
        frame = db.reference(db.registers.rdi, PyFrameObject)
        if frame.value.f_code:
            print 'eval_frame', frame.value.f_code.address
        return True

    def _psyco_code_run(self, db):
        code_buffer = db.reference(db.registers.rdi, CodeBufferObject)
        frame = db.reference(db.registers.rsi, PyFrameObject)
        print 'run_code', code_buffer.value.codestart, frame.value.f_code.address
        return True

    def begin_trace(self):
        class Trace:
            def jump(self, desc, _from, to):
                print desc, _from, to
        trace = Trace()
        for interval in self._trace:
            address = interval.begin
            func = interval.data
            def breakpoint(db, trace=trace, func=func):
                return func(db, trace)
            self.db.add_breakpoint(address, breakpoint, immediately=True)
        self.db.add_breakpoint('PyEval_EvalFrameEx', self._py_eval_eval_frame_ex)
        self.db.add_breakpoint('PsycoCode_Run', self._psyco_code_run)


