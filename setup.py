#! /usr/bin/env python

"""Setup script for Psyco, the Python specializing compiler"""

import os
from distutils.core import setup
from distutils.extension import Extension

####################################################################
#
#  Customizable debugging flags.
#  Copy the following section in a new file 'preferences.py' to
#  avoiding changing setup.py. Uncomment and change the options there
#  at your will. Setting PsycoDebug to 1 is probably the first thing
#  you want to do to enable all internal checks.

#PSYCO_DEBUG = 1

# level of debugging outputs: 0 = none, 1 = a few, 2 = more,
#   3 = detailled, 4 = full execution trace
#VERBOSE_LEVEL = 0

# write produced blocks of code into a file; see 'xam.py'
#  0 = off, 1 = only manually (from a debugger or with _psyco.dumpcodebuf()),
#  2 = only when returning from Psyco,
#  3 = every time a new code block is built
#CODE_DUMP = 1

# Linux-only *heavy* memory checking: 0 = off, 1 = reasonably heavy,
#                                     2 = unreasonably heavy.
#HEAVY_MEM_CHECK = 0

# If the following is set to 1, Psyco is compiled by #including all .c
# files into psyco.c.
# It provides a version of _psyco.so whose only exported (non-static)
# symbol is init_psyco(). It also seems that the GDB debugger doesn't locate
# too well non-static symbols in shared libraries. Recompiling after minor
# changes is faster if ALL_STATIC=0.
ALL_STATIC = 1

# Be careful with ALL_STATIC=0, because I am not sure the distutils can
# correctly detect all the dependencies. In case of doubt always compile
# with `setup.py build_ext -f'.


####################################################################

# override options with the ones from preferences.py, if the file exists.
try:
    execfile('preferences.py')
except IOError:
    pass


# loads the list of source files from SOURCEDIR/files.py
# and make the appropriate options for the Extension class.
SOURCEDIR = 'c'

data = {}
execfile(os.path.join(SOURCEDIR, 'files.py'), data)

SRC = data['SRC']
MAINFILE = data['MAINFILE']

macros = []
for name in ['PSYCO_DEBUG', 'VERBOSE_LEVEL',
             'CODE_DUMP', 'HEAVY_MEM_CHECK', 'ALL_STATIC']:
    if globals().has_key(name):
        macros.append((name, str(globals()[name])))

if ALL_STATIC:
    sources = [SOURCEDIR + '/' + MAINFILE]
else:
    sources = [SOURCEDIR + '/' + s.filename for s in SRC]


setup (	name="psyco",
      	version="0.3.5",
      	description="Psyco, the Python specializing compiler",
      	author="Armin Rigo",
        author_email="arigo@users.sourceforge.net",
      	url="http://psyco.sourceforge.net/",
        packages=['psyco'],
      	ext_modules=[Extension(name = 'psyco._psyco',
                               sources = sources,
                               define_macros = macros)]
        )
