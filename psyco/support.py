###########################################################################
# 
#  Psyco general support module.
#   Copyright (C) 2001-2002  Armin Rigo et.al.

"""Psyco general support module.

For internal use.
"""
###########################################################################

import sys, _psyco, __builtin__

error = _psyco.error
class warning(Warning):
    pass

_psyco.NoLocalsWarning = warning

def warn(msg):
    from warnings import warn
    warn(msg, warning, stacklevel=2)

#
# Version checks
#
__version__ = 0x010000b1
if _psyco.PSYVER != __version__:
    raise error, "version mismatch between Psyco parts, reinstall it"


VERSION_LIMITS = [0x02010000,   # 2.1
                  0x02020000,   # 2.2
                  0x02020200,   # 2.2.2
                  0x02030000]   # 2.3

if ([v for v in VERSION_LIMITS if v <= sys.hexversion] !=
    [v for v in VERSION_LIMITS if v <= _psyco.PYVER  ]):
    if sys.hexversion < VERSION_LIMITS[0]:
        warn("Psyco requires Python version 2.1 or later")
    else:
        warn("Psyco version does not match Python version. "
             "Psyco must be updated or recompiled")

PYTHON_SUPPORT = hasattr(_psyco, 'turbo_code')


if hasattr(_psyco, 'ALL_CHECKS') and hasattr(_psyco, 'VERBOSE_LEVEL'):
    print >> sys.stderr, 'psyco: running in debugging mode'


###########################################################################
# sys._getframe() gives strange results on a mixed Psyco- and Python-style
# stack frame. Psyco provides a replacement that partially emulates Python
# frames from Psyco frames. Be aware that the f_back fields are not
# correctly set up.
#
# The same problems require some other built-in functions to be replaced
# as well. Note that the local variables are not available in any
# dictionary with Psyco.

def patch(name, module=__builtin__):
    f = getattr(_psyco, name)
    org = getattr(module, name)
    if org is not f:
        setattr(module, name, f)
        setattr(_psyco, 'original_' + name, org)


patch('_getframe', sys)
patch('globals')
patch('eval')
patch('execfile')
patch('locals')
patch('vars')
patch('dir')
patch('input')
_psyco.original_raw_input = raw_input
__builtin__.__in_psyco__ = 0
