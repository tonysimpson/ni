###########################################################################
# 
#  Psyco top-level file of the Psyco package.
#   Copyright (C) 2001-2002  Armin Rigo et.al.
#
#  All source files of Psyco, including all Python sources in this package,
#   are protected by the GNU General Public License as found in COPYING.txt.

"""Psyco, the Python Specializing Compiler

Compile all global functions:
  import psyco; psyco.jit()

  This enables transparent just-in-time compilation of the most
  used functions of your programs. Just execute 'psyco.jit()' once.
  Note that this only works on global functions.

Compile all class methods:
  from psyco.classes import *

  For Python >= 2.2, the above line to put at the beginning of each
  file defining classes instructs Psyco to compile all their methods.
  Alternatively, you can manually select the classes to optimize by
  having them inherit from 'psyco.classes.psyobj'.

Detailled choice of functions and classes to compile:
  psyco.bind(f)

  For larger programs, the above solutions are too heavy, as Psyco
  currently does not automatically identify the performance
  bottlenecks. You can trigger the compilation of only the most
  algorithmically intensive functions or classes with psyco.bind().
"""
###########################################################################

import _psyco, sys, new, __builtin__
from types import FunctionType, MethodType


__all__ = ['error', 'jit', 'bind', 'proxy', 'dumpcodebuf']
__version__ = (0,5,0)

error = _psyco.error

def jit(tick=5):
    """Enable just-in-time compilation.
Argument is number of invocations before rebinding."""
    _psyco.selective(tick)

def bind(func, rec=_psyco.DEFAULT_RECURSION):
    """Enable compilation of the given function, method or class.
In the latter case all methods defined in the class are rebound.

The optional second argument specifies the number of recursive
compilation levels: all functions called by func are compiled
up to the given depth of indirection."""
    if isinstance(func, FunctionType):
        func.func_code = _psyco.proxycode(func, rec)
    elif isinstance(func, MethodType):
        bind(func.im_func, rec)
    elif hasattr(func, '__dict__'):  # for classes
        for object in func.__dict__.values():
            try:
                bind(object, rec)
            except error:
                pass

def proxy(func, rec=_psyco.DEFAULT_RECURSION):
    """Return a Psyco-enabled copy of the function.

The original function is still available for non-compiled calls.
The optional second argument specifies the number of recursive
compilation levels: all functions called by func are compiled
up to the given depth of indirection."""
    if isinstance(func, FunctionType):
        code = _psyco.proxycode(func, rec)
        return new.function(code, func.func_globals, func.func_name)
    if isinstance(func, MethodType):
        p = proxy(func.im_func, rec)
        return new.instancemethod(p, func.im_self, func.im_class)
    else:
        raise TypeError, 'function or method required'

def unbind(func):
    """Disable compilation of the given function, method or class.
Any future call to 'func' will use the regular Python interpreter."""
    if isinstance(func, FunctionType):
        try:
            f = _psyco.unproxycode(func.func_code)
        except error:
            pass
        else:
            func.func_code = f.func_code
    elif isinstance(func, MethodType):
        unbind(func.im_func)
    elif hasattr(func, '__dict__'):  # for classes
        for object in func.__dict__.values():
            unbind(object)

def unproxy(func):
    """Return a new copy of the original function of a proxy.
The result behaves like the original function in that calling it
does not trigger compilation nor execution of any compiled code."""
    if isinstance(func, FunctionType):
        return _psyco.unproxycode(func.func_code)
    if isinstance(func, MethodType):
        f = unproxy(func.im_func)
        return new.instancemethod(f, func.im_self, func.im_class)
    else:
        raise TypeError, 'function or method required'


def dumpcodebuf():
    """Write in file psyco.dump a copy of the emitted machine code,
provided Psyco was compiled with a non-zero CODE_DUMP.
See py-utils/httpxam.py to examine psyco.dump."""
    if hasattr(_psyco, 'dumpcodebuf'):
        _psyco.dumpcodebuf()


# Psyco mode check
if hasattr(_psyco, 'ALL_CHECKS'):
    print >> sys.stderr, 'psyco: running in debugging mode'


# Reading this variable always return zero, but Psyco special-cases it by
# returning 1 instead. So __in_psyco__ can be used to know in a function if
# the function is being executed by Psyco or not.
__builtin__.__in_psyco__ = 0


###########################################################################
# sys._getframe() gives strange results on a mixed Psyco- and Python-style
# stack frame. Psyco provides a replacement that partially emulates Python
# frames from Psyco frames. Be aware that the f_back fields are not
# correctly set up.

original_sys_getframe = sys._getframe  # old value, if you need it
sys._getframe = _psyco._getframe


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
