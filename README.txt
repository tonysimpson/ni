======================================================================
               Psyco, the Python Specializing Compiler
======================================================================


REQUIREMENTS
------------

Psyco accelerates the execution of Python code. It requires Python 2.1
or 2.2 and a 386-compatible processor. It should be OS-independant,
although it has only be tested on Linux and Windows. Problems have
been reported on FreeBSD.

This is alpha/beta software. Note that Psyco should work a little
better with Python 2.2 than 2.1; in particular, using built-in types
as classes is recommended (see comments in 'psyco/classes.py').


QUICK INTRODUCTION
------------------

To install Psyco, put the 'psyco' package in your Python search path,
e.g. by copying the subdirectory 'psyco' into the directory
'../python2.x/lib/site-packages'.

If you downloaded the source, you first need to build it; this should
be as easy as running 'python setup.py build' in the Psyco root.

Usage is very simple; see the documentation included in the Psyco
package, in 'psyco/__init__.py'. With Python 2.2 you can read it by
typing 'help("psyco")'.


DOCUMENTATION AND LATEST VERSIONS
---------------------------------

See:
  *  http://sourceforge.net/projects/psyco
  *  ISSUES.txt for known issues
  *  below for inspecting the emitted machine code

An up-to-date technical documentation ('how does it work?') is yet
to come.


DEBUG BUILD
-----------

To build a version of Psyco that includes debugging checks and/or
debugging output, see comments in setup.py.


INSPECTING THE MACHINE CODE
---------------------------

The directory py-utils contains two scripts I use to inspect the emitted
machine code. Of course, you need to know what is going on under the cover
to fully understand why the code is like this, but this can be a good starting
point. The script httpxam.py reads the files psyco.dump produced in debugging
mode, formats them and presents them as HTML pages to a web browser. httpxam.py
itself is a web server built on Python's standard SimpleHTTPServer. When
it is running, point your browser to http://127.0.0.1:8000.

httpxam.py probably only works on Linux. It requires 'objdump' or 'ndisasm' to
disassemble the code and 'nm' to list symbol addresses in the Python and Psyco
executables.

The cross-calling code buffers are presented as cross-linked HTML pages.
Bold lines show the targets of another jump. If preceeded by a blank line,
a bold line shows that another code buffer jumps directly at this position.
The end of the buffer is often garbage; it is not code, but data added there
(typically for the promotion of a value). There are various kind of code
buffers, depending (only) on why Psyco produced it:

 * normal:        normal mainstream compilation
 * respawn:       execution jumps here when an error occurs, but never did so yet
 * respawned:     replaces a respawn buffer when execution has jumped here
 * unify:         small buffer whose purpose is to jump back to an existing one
 * load_global:   called when a change is detected in a global variable
 * coding_pause:  not yet compiled, will be compiled if execution jumps here



----------
Armin Rigo.
