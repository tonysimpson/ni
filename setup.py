#! /usr/bin/env python

"""Setup script for Psyco, the Python specializing compiler"""

from distutils.core import setup
from distutils.extension import Extension


src = [
    "c/Python/pycompiler.c",
    "c/Python/pbltinmodule.c",
    "c/Objects/pabstract.c",
    "c/Objects/pdictobject.c",
    "c/Objects/pfuncobject.c",
    "c/Objects/pintobject.c",
    "c/Objects/piterobject.c",
    "c/Objects/plistobject.c",
    "c/Objects/plongobject.c",
    "c/Objects/pmethodobject.c",
    "c/Objects/pobject.c",
    "c/Objects/psycofuncobject.c",
    "c/Objects/pstringobject.c",
    "c/Objects/ptupleobject.c",
    "c/Objects/pclassobject.c",
    "c/Objects/pdescrobject.c",
    "c/Objects/pstructmember.c",
    "c/linuxmemchk.c",
    "c/codemanager.c",
    "c/dispatcher.c",
    "c/processor.c",
    "c/vcompiler.c",
    "c/mergepoints.c",
    "c/pycencoding.c",
    "c/selective.c",
    "c/psyco.c"
    ]


# Psyco can be compiled and installed using the src list above, but
# we need statics for gdb and to avoid cluttering the namespace
# of Python itself, so for now we just use good old hack.c.
src = ['c/hack.c']


setup (	name="psyco",
      	version="0.3.4",
      	description="Psyco, the Python specializing compiler",
      	author="Armin Rigo",
      	url="http://psyco.sourceforge.net/",
      	ext_modules=[Extension(name="_psyco",
                               sources=src)]
        )
