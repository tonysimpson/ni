###########################################################################
# 
#  Psyco class support module.
#   Copyright (C) 2001-2002  Armin Rigo et.al.
#
#  All source files of Psyco, including all Python sources in this package,
#   are protected by the GNU General Public License as found in COPYING.txt.

"""Psyco class support module.

'psyco.classes.psyobj' is an alternate Psyco-optimized root for classes.
Any class inheriting from it or using the metaclass '__metaclass__' might
get optimized specifically for Psyco.

Note that this is not compatible with multiple inheritance and it does
not work on Python version 2.1 or earlier.

Importing everything from psyco.classes in a module will import the
'__metaclass__' name, so all classes defined after a

       from psyco.classes import *

will automatically use the Psyco-optimized metaclass.
"""
###########################################################################

import _psyco

__all__ = ['psyobj', 'psymetaclass', '__metaclass__']


# Python version check
try:
    object
except NameError:
    class psyobj:        # compatilibity
        pass
    psymetaclass = None
else:
    # version >= 2.2 only
    
    FunctionType = type(lambda x: None)
    recursivity = 5
    
    class psymetaclass(type):
        "Psyco-optimized meta-class. Turns all methods into Psyco proxies."
    
        def __init__(self, name, bases, dict):
            type.__init__(self, name, bases, dict)
            for key, value in self.__dict__.items():
                if isinstance(value, FunctionType):
                    setattr(self, key, _psyco.proxy(value, recursivity))
    
    psyobj = psymetaclass("psyobj", (), {})
    __metaclass__ = psymetaclass
