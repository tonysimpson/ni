#! /usr/bin/env python

"""
Ni aims to provide a module that just makes Python faster.

Ni is based on a fork of Armin Rigo's Psyco, Armin and the other Psyco
developers moved onto PyPy so go check that out if you want a high performance
implementation of Pythoni (http://pypy.org).

Ni is not currently alpha software and not fit for use. It will be several
months before it is ready to release.
"""
from __future__ import print_function

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
ALL_STATIC = 0

# Be careful with ALL_STATIC=0, because I am not sure the distutils can
# correctly detect all the dependencies. In case of doubt always compile
# with `setup.py build_ext -f'.


####################################################################



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
        raise ProcessorAutodetectError("cannot run 'uname -m'")
    if mach == 'x86_64' and sys.maxint == 2147483647:
        mach = 'x86'     # it's a 64-bit processor but in 32-bits mode, maybe
    try:
        return {'i386': 'i386',
                'i486': 'i386',
                'i586': 'i386',
                'i686': 'i386',
                'i86pc': 'i386',    # Solaris/Intel
                'x86':   'i386',    # Apple
                'x86_64': 'x64',
                }[mach]
    except KeyError:
        raise ProcessorAutodetectError("unsupported processor '%s'" % mach)


macros = []
for name in ['PSYCO_DEBUG', 'VERBOSE_LEVEL',
             'CODE_DUMP', 'HEAVY_MEM_CHECK', 'ALL_STATIC',
             'PSYCO_NO_LINKED_LISTS']:
    if name in globals():
        macros.append((name, str(globals()[name])))

if PROCESSOR is None:
    try:
        PROCESSOR = autodetect()
    except ProcessorAutodetectError:
        PROCESSOR = 'ivm'  # fall back to the generic virtual machine
print("Processor", PROCESSOR)

def find_sources(processor):
    for root, dirs, filenames in os.walk('./c'):
        skip = any(
            [processor_prefix != processor and processor_prefix in root
            for processor_prefix in ['ivm', 'i386', 'x64', 'dummybackend']]
        )
        if skip:
            continue
        for filename in filenames:
            if filename.endswith('.c'):
                yield os.path.join(root, filename)


extra_compile_args = ['-O0', '-g3', '-Wall', '-fno-stack-protector']
extra_link_args = []
sources = list(find_sources(PROCESSOR))
processor_dir = os.path.join('./c', PROCESSOR)


CLASSIFIERS = [
    'Development Status :: 2 - Pre-Alpha',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: MIT License',
    'Operating System :: OS Independent',
    'Programming Language :: Python',
    'Programming Language :: C',
    'Topic :: Software Development :: Compilers',
    'Topic :: Software Development :: Interpreters',
    ]

try:
    import distutils.command.register
except ImportError:
    kwds = {}
else:
    kwds = {'classifiers': CLASSIFIERS}


setup ( name             = "ni",
        version          = "0.1alpha1",
        description      = "Plugin JIT for CPython",
        maintainer       = "Tony Simpson",
        maintainer_email = "agjasimpson@gmail.com",
        url              = "http://github.com/tonysimpson/ni",
        license          = "MIT License",
        long_description = __doc__,
        packages         = ['ni', 'psyco'],
        ext_modules=[Extension(name = '_psyco',
                               sources = sources,
                               extra_compile_args = extra_compile_args,
                               extra_link_args = extra_link_args,
                               define_macros = macros,
                               debug = True,
                               include_dirs = [processor_dir])],
        **kwds )
