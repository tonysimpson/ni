import _psyco
import sys

funcs = {}
ticks = 0
depth = 10

def f(frame, event, arg):
    if event != 'call':  return
    fn = frame.f_code.co_name
    g = frame.f_globals
    if not funcs.has_key(fn):
        funcs[fn] = 1
    if funcs[fn] != None:
        funcs[fn] = funcs[fn] + 1
        if funcs[fn] > ticks and g.has_key(fn):
            g[fn] = _psyco.proxy(g[fn], depth)
            funcs[fn] = None
            print 'psyco rebinding function:', fn

sys.setprofile(f)
