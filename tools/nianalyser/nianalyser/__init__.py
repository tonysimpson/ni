import distorm3
import pointbreak
from pointbreak import types
from itertools import count

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


class CompiledInstructionCode:
    def __init__(self, instruction, start, end, code, code_gen_id, backtrace):
        self.instruction = instruction
        self.code_gen_id = code_gen_id
        self.start = start
        self.end = end
        self.code = code
        self.backtrace = backtrace


class ByteCode:
    def __init__(self, byte_code, lnotab, filename, name):
        self.filename = filename
        self.name = name
        self.lnotab = lnotab
        self.byte_code = byte_code
        self.cics = []

    def add_compiled_instruction_code(self, cic):
        self.cics.append(cic)

 
class NiAnalyser:
    def __init__(self, db):
        self.db = db
        self.db.add_breakpoint('ni_trace_begin_code', self.begin_code)
        self.db.add_breakpoint('ni_trace_end_code', self.end_code)
        #self.db.add_breakpoint('vinfo_new', self.vinfo_new)
        #self.db.add_breakpoint('vinfo_release', self.vinfo_release)
        #self.db.add_breakpoint('compute_vinfo', self.compute_vinfo)
        #self.db.add_breakpoint('PsycoObject_New', self.po_new)
        #self.db.add_breakpoint('PsycoObject_Delete', self.po_delete)
        #self.db.add_breakpoint('PsycoObject_Duplicate', self.po_duplicate)
        self.byte_code_cache = {}
        self.code_gen_id = 0
        self._start_pos = -1
        self.execution_data = []

    def begin_code(self, db):
        po_ptr = db.reference(db.registers.rdi, PsycoObject)
        if po_ptr.value.pr.co:
            self._start_pos = po_ptr.value.code
        return True

    def end_code(self, db):
        po_ptr = db.reference(db.registers.rdi, PsycoObject)
        if po_ptr.value.pr.co:
            byte_code = pystringobject_to_str(po_ptr.value.pr.co.value.co_code.value)
            if byte_code not in self.byte_code_cache:
                lnotab = pystringobject_to_str(po_ptr.value.pr.co.value.co_lnotab.value)
                filename = pystringobject_to_str(po_ptr.value.pr.co.value.co_filename.value)
                name = pystringobject_to_str(po_ptr.value.pr.co.value.co_name.value)
                self.byte_code_cache[byte_code] = ByteCode(byte_code, lnotab, filename, name)
            bc = self.byte_code_cache[byte_code]
            instr = po_ptr.value.pr.next_instr
            start = self._start_pos
            end = po_ptr.value.code
            code = db.read(start, end - start)
            bc.add_compiled_instruction_code(CompiledInstructionCode(instr, start, end, code, self.code_gen_id, [i.function_name for i in db.backtrace()]))
            db.add_breakpoint(self._start_pos, self.execution, immediately=True)
        self.code_gen_id += 1
        return True

    def execution(self, db):
        self.execution_data.append([self.code_gen_id, db.registers.rip, dict((name, getattr(db.registers, name)) for name in dir(db.registers) if name.startswith('r'))])
        return True

    def vinfo_new(self, db):
        return True

    def vinfo_release(self, db):
        return True

    def compute_vinfo(self, db):
        return True

    def po_new(self, db):
        return True

    def po_delete(self, db):
        return True

    def po_duplicate(self, db):
        return True

    def decode(self, name):
        r10 = 0
        db = self.db
        for cic in [i for i in self.byte_code_cache.values() if i.name == name][0].cics:
            index = cic.backtrace.index('call_ceval_hooks')
            print  '.'.join(reversed(cic.backtrace[index+1:-1]))
            for pos, _, decode, _ in distorm3.Decode(cic.start, cic.code, type=distorm3.Decode64Bits):
                if decode.startswith('CALL 0x'):
                    decode = '%s (%s)' % (decode, db.address_to_description(eval(decode.split()[1])))
                elif decode.startswith('MOV R10, 0x'):
                    r10 = eval(decode.split()[2])
                elif decode.startswith('CALL R10'):
                    decode = '%s (R10=%s)' % (decode, db.address_to_description(r10))
                print '  0x%x' % (pos,), decode
            print


    def trace_execution(self, name):
        r10 = 0
        db = self.db
        cics = [i for i in self.byte_code_cache.values() if i.name == name][0].cics
        conc = True
        for gen_id, ip, regs in self.execution_data:
            matches = sorted([i for i in cics if i.start == ip and i.code_gen_id <= gen_id], key=lambda x: x.code_gen_id)
            if matches:
                if not conc:
                    print
                    print '------'
                    print
                    conc = True
                cic = matches[-1]
                index = cic.backtrace.index('call_ceval_hooks')
                print  '.'.join(reversed(cic.backtrace[index+1:-1]))
                print ' '.join('%s=%s' % (k, v) for k, v in sorted(regs.items()))
                for pos, _, decode, _ in distorm3.Decode(cic.start, cic.code, type=distorm3.Decode64Bits):
                    if decode.startswith('CALL 0x'):
                        decode = '%s (%s)' % (decode, db.address_to_description(eval(decode.split()[1])))
                    elif decode.startswith('MOV R10, 0x'):
                        r10 = eval(decode.split()[2])
                    elif decode.startswith('CALL R10'):
                        decode = '%s (R10=%s)' % (decode, db.address_to_description(r10))
                    
                    print '  0x%x' % (pos,), decode
                print
            else:
                conc = False
