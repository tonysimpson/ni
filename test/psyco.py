import _psyco
_psyco.selective(1) # Argument is number of invocations before rebinding


###########################################################################
# Support for Python's warnings is not complete, because it
# uses sys._getframe() which will give strange results on a mixed Psyco-
# and Python-style stack frame.
# We work around this by having sys._getframe() always raise ValueError.
# The warning subsystem will then apply the filters globally instead of
# on a per-module basis.

import sys

def disabled_getframe(n=0):
    global _erronce
    if not _erronce:
        # cannot use Warnings, I guess it would cause an endless loop
        print >> sys.stderr, 'psyco: sys._getframe() not supported; support for warnings is only partial'
        _erronce = 1
    raise ValueError, 'sys._getframe() disabled for Psyco'

sys_getframe = sys._getframe  # old value
sys._getframe = disabled_getframe
_erronce = 0


###########################################################################
# The code produced by Psyco is not nice with threads; it does not
# include all the checks the Python interpreter does regularly.



###########################################################################
# Python-based rebinding code, now moved in selective.c. This code
# should probably be partially moved back here, leaving only the detection
# of the most used functions to C code, not the rebinding.

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

