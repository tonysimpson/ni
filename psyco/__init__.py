###########################################################################
# 
#  Psyco top-level file of the Psyco package.
#   Copyright (C) 2001-2002  Armin Rigo et.al.

"""Psyco -- the Python Specializing Compiler.

Typical usage: add the following lines to your application's main module:

try:
    import psyco
    psyco.profile()
except:
    print 'Psyco not found, ignoring it'
"""
###########################################################################


#
# This module is present to make 'psyco' a package and to
# publish the main functions and variables.
#
# More documentation can be found in core.py.
#

from support import __version__, error, warning
from core import full, profile, background, runonly, stop, cannotcompile
from core import log, bind, unbind, proxy, unproxy, dumpcodebuf
