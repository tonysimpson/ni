======================================================================
               Psyco, the Python Specializing Compiler
======================================================================


                              VERSION 1.2
                              -----------

Psyco transparently accelerates the execution of Python code.


REQUIREMENTS
------------

Psyco works on any recent version of Python (currently 2.1 to 2.3).
At present it *requires* a *PC* (i.e. a 386-compatible processor),
but it is OS-independant.

This program is still incomplete, but still it seems to have been
quite stable for some time and can give good results.

Note that Psyco works a bit better with Python versions starting
from 2.2.2.


QUICK INTRODUCTION
------------------

To install Psyco, do the usual

   python setup.py install

Manually, you can also put the 'psyco' package in your Python search
path, e.g. by copying the subdirectory 'psyco' into the directory
'/usr/lib/python2.x/site-packages' (default path on Linux).

Basic usage is very simple: add

  import psyco
  psyco.full()

to the beginning of your main script. For basic introduction see:

  import psyco
  help(psyco)


DOCUMENTATION AND LATEST VERSIONS
---------------------------------

Home page:

  *  http://psyco.sourceforge.net

The current up-to-date documentation is the Ultimate Psyco Guide.
If it was not included in this distribution ("psycoguide.ps" or
"psycoguide/index.html"), see the doc page:

  *  http://psyco.sourceforge.net/doc.html


DEBUG BUILD
-----------

To build a version of Psyco that includes debugging checks and/or
debugging output, see comments in setup.py.


----------
Armin Rigo.
