import _psyco
import sys

files = {}
ticks = 0
depth = 10

def f(frame, event, arg):
    if event != 'call':  return
    fn = frame.f_code.co_name
    ffn = frame.f_code.co_filename
    if not files.has_key(ffn):
        files[ffn] = {}
    funcs = files[ffn]
    t = frame.f_code.co_firstlineno
    g = frame.f_globals
    if not funcs.has_key(t):
        funcs[t] = 1
    if funcs[t] != None:
        funcs[t] = funcs[t] + 1
        if funcs[t] > ticks and g.has_key(fn):
            g[fn] = _psyco.proxy(g[fn], depth)
            funcs[t] = None
            print 'psyco rebinding function:', fn

sys.setprofile(f)
