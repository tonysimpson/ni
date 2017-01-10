from gdb.unwinder import Unwinder, register_unwinder

class MyUnwinder(Unwinder):
    def __init__(self, *args, **kwargs):
        super(MyUnwinder, self).__init__(*args, **kwargs)

    def __call__(self, pending_frame):        
        sp = pending_frame.read_register("esp")
        pc = pending_frame.read_register("eip")
        print("eip: %d, esp: %d" % (int(pc.cast(gdb.lookup_type('unsigned long'))), int(sp.cast(gdb.lookup_type('unsigned long')))))
        return None

register_unwinder(None, MyUnwinder("whatever"), replace=False)
