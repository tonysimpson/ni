#! /usr/bin/env python

# $Id$

"""Setup script for Psyco, the Python specializing compiler"""

from distutils.core import setup
from distutils.extension import Extension

# This one is sufficient right now
src = ["c/hack.c"]

###############################################################################

setup (	name="psyco",
      	version="0.3.4",
      	description="Psyco, the Python specializing compiler",
      	author="Armin Rigo",
      	url="http://psyco.sourceforge.net/",
      	ext_modules=[Extension(name="_psyco",
                               sources=src,
                               include_dirs=['c'])]
        )
