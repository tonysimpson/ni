###########################################################################
# 
#  Psyco top-level file of the Psyco package.
#   Copyright (C) 2001-2002  Armin Rigo et.al.
#
#  All source files of Psyco, including all Python sources in this package,
#   are protected by the GNU General Public License as found in COPYING.txt.

"""Psyco, the Python Specializing Compiler

Step 1.  import psyco; psyco.jit()

This enables transparent just-in-time compilation of the most
used functions of your programs. Just execute 'psyco.jit()' once.
Currently only works on global functions, but will be fixed.

Step 2.  from psyco.classes import *

If you use classes (but not multiple inheritance), the above line
to put at the beginning of each file defining classes instructs
Psyco to compile all the methods. Alternatively, you can manually
select the classes to optimize by having them inherit from
'psyco.classes.psyobj'. (Only works on Python >= 2.2; incompatible
with multiple inheritance.)
"""
###########################################################################

import _psyco, sys, __builtin__


__all__ = ['jit']


def jit(tick=5):
    """Enable just-in-time compilation.
Argument is number of invocations before rebinding."""
    _psyco.selective(tick)

def jit2(tick=5):
    """Enable just-in-time compilation.
Argument is number of invocations before rebinding."""
    print '** Warning: jit2 is not working yet'
    _psyco.selective2(tick)

# Psyco mode check
if hasattr(_psyco, 'ALL_CHECKS'):
    print >> sys.stderr, 'psyco: running in debugging mode'


# Reading this variable always return zero, but Psyco special-cases it by
# returning 1 instead. So __in_psyco__ can be used to know in a function if
# the function is being executed by Psyco or not.
__builtin__.__in_psyco__ = 0


###########################################################################
# Support for Python's warnings is not complete, because it
# uses sys._getframe() which will give strange results on a mixed Psyco-
# and Python-style stack frame.
# We work around this by having sys._getframe() always raise ValueError.
# The warning subsystem will then apply the filters globally instead of
# on a per-module basis.

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
