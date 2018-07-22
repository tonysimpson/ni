import pointbreak
from pointbreak import types

Py_ssize_t = types.int64
PyTypeObject_pointer = types.pointer_type(None)
PyObject_HEAD = [
    ('ob_refcnt', Py_ssize_t),
    ('ob_type', PyTypeObject_pointer)
]
PyObject_VAR_HEAD = PyObject_HEAD + [
    ('ob_size', Py_ssize_t)
]
PyTypeObject = types.struct(
    *PyObject_VAR_HEAD + [
        ('tp_name', types.c_string_pointer)
    ]
    # incomplete - but that's ok for our use
)
PyTypeObject_pointer.referenced_type = PyTypeObject
PyObject = types.struct(*PyObject_HEAD)
PyVarObject = types.struct(*PyObject_VAR_HEAD)
PyObject_pointer = types.pointer_type(PyObject)
PyCodeObject = types.struct(*PyObject_HEAD + [
    ('co_argcount', types.int32),
    ('co_nlocals', types.int32),
    ('co_stacksize', types.int32),
    ('co_flags', types.int32),
    ('co_code', PyObject_pointer),
    ('co_consts', PyObject_pointer),
    ('co_names', PyObject_pointer),
    ('co_varnames', PyObject_pointer),
    ('co_freevars', PyObject_pointer),
    ('co_cellvars', PyObject_pointer),
    ('co_filename', PyObject_pointer),
    ('co_name', PyObject_pointer),
    ('co_firstlineno', types.int32),
    ('co_lnotab', PyObject_pointer),
    ('co_zombieframe', types.uint64),
    ('co_weakreflist', PyObject_pointer)
])
vinfo_t_pointer = types.pointer_type(None)
vinfo_array_t = types.struct_type(
    ('count', types.int32),
    ('items', types.array_type(7, ptr_vinfo_t, checked=False))
)
vinfo_t = types.struct_type(
    ('refcount', types.int32),
    ('source', types.uint64),
    ('array', type.pointer_type(vinfo_array_t)),
    ('tmp', ptr_vinfo_t),
)
vinfo_t_pointer.referenced_type = vinfo_t
PyTryBlock = types.struct_type(
    ('b_type', types.int32),
    ('b_handler', types.int32),
    ('b_level', types.int32),
)
CO_MAXBLOCKS = 20
pyc_data_t = types.struct_type(
    ('co', type.pointer_type(PyCodeObject)),
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
    ('reg_array', types.array_type(16, ptr_vinfo_t),
    ('ccregs', types.array_type(2, ptr_vinfo_t),
    ('last_used_reg', reg_t),
    ('respawn_cnt', types.int32),
    ('respawn_proxy', types.uint64),
    ('pr', pyc_data_t),
    ('vlocals', vinfo_array_t)
)

 
class NiAnalyser:
    def __init__(self, db):
        self.db = db
        self.db.add_breakpoint('ni_trace_begin_code', self.begin_code)
        self.db.add_breakpoint('ni_trace_end_code', self.end_code)
        self.db.add_breakpoint('vinfo_new', self.vinfo_new)
        self.db.add_breakpoint('vinfo_release', self.vinfo_release)
        self.db.add_breakpoint('compute_vinfo', self.compute_vinfo)
        self.db.add_breakpoint('PsycoObject_New', self.po_new)
        self.db.add_breakpoint('PsycoObject_Delete', self.po_delete)
        self.db.add_breakpoint('PsycoObject_Duplicate', self.po_duplicate)

    def begin_code(self, db):
        print db.read_fmt(db.registers.rdi, 'Q')[0]
        return True

    def end_code(self, db):
        print db.read_fmt(db.registers.rdi, 'Q')[0]
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


