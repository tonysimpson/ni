#! /usr/bin/env python

"""Setup script for Psyco, the Python specializing compiler"""

import os, sys
from distutils.core import setup
from distutils.extension import Extension

PROCESSOR = None  # autodetect

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


# processor auto-detection
class ProcessorAutodetectError(Exception):
    pass
def autodetect():
    platform = sys.platform.lower()
    if platform.startswith('win'):   # assume an Intel Windows
        return 'i386'
    # assume we have 'uname'
    mach = os.popen('uname -m', 'r').read().strip()
    if not mach:
        raise ProcessorAutodetectError, "cannot run 'uname -m'"
    try:
        return {'i386': 'i386',
                'i486': 'i386',
                'i586': 'i386',
                'i686': 'i386',
                }[mach]
    except KeyError:
        raise ProcessorAutodetectError, "unsupported processor '%s'" % mach


# loads the list of source files from SOURCEDIR/files.py
# and make the appropriate options for the Extension class.
SOURCEDIR = 'c'

data = {}
execfile(os.path.join(SOURCEDIR, 'files.py'), data)

SRC = data['SRC']
MAINFILE = data['MAINFILE']

macros = []
for name in ['PSYCO_DEBUG', 'VERBOSE_LEVEL',
             'CODE_DUMP', 'HEAVY_MEM_CHECK', 'ALL_STATIC',
             'PSYCO_NO_LINKED_LISTS']:
    if globals().has_key(name):
        macros.append((name, str(globals()[name])))

if PROCESSOR is None:
    try:
        PROCESSOR = autodetect()
    except ProcessorAutodetectError, e:
        print '%s: %s' % (e.__class__.__name__, e)
        print 'Set PROCESSOR to one of the supported processor names in setup.py, line 9.'
        sys.exit(2)
    print "PROCESSOR = %r" % PROCESSOR
processor_dir = 'c/' + PROCESSOR

if ALL_STATIC:
    sources = [SOURCEDIR + '/' + MAINFILE]
else:
    sources = [SOURCEDIR + '/' + s.filename for s in SRC]

extra_compile_args = []
extra_link_args = []
if sys.platform == 'win32':
    if globals().get('PSYCO_DEBUG'):
        # how do we know if distutils will use the MS compilers ???
        # these are just hacks that I really need to compile psyco debug versions
        # on Windows
        extra_compile_args.append('/Od')   # no optimizations, override the default /Ox
        extra_compile_args.append('/ZI')   # debugging info
        extra_link_args.append('/debug')   # debugging info
    macros.insert(0, ('NDEBUG', '1'))  # prevents from being linked against python2x_d.lib


setup (	name="psyco",
      	version="1.1.1",
      	description="Psyco, the Python specializing compiler",
      	author="Armin Rigo",
        author_email="arigo@users.sourceforge.net",
      	url="http://psyco.sourceforge.net/",
        packages=['psyco'],
      	ext_modules=[Extension(name = 'psyco._psyco',
                               sources = sources,
                               extra_compile_args = extra_compile_args,
                               extra_link_args = extra_link_args,
                               define_macros = macros,
                               include_dirs = [processor_dir])]
        )
