import _psyco
_psyco.selective(1) # Argument is number of invocations before rebinding

#  import sys

#  ticks = 0
#  depth = 10
#  funcs = {}

#  def f(frame, event, arg):
#      if event != 'call':  return
#      print type(frame.f_globals)
#      c = frame.f_code.co_code
#      fn = frame.f_code.co_name
#      g = frame.f_globals
#      if not funcs.has_key(c):
#          funcs[c] = 1
#      if funcs[c] != None:
#          funcs[c] = funcs[c] + 1
#          if funcs[c] > ticks and g.has_key(fn):
#              g[fn] = _psyco.proxy(g[fn], depth)
#              funcs[c] = None
#              print 'psyco rebinding function:', fn

#  sys.setprofile(f)

