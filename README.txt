======================================================================
               Psyco, the Python Specializing Compiler
======================================================================

USER GUIDE
==========

This document is available as 'README.txt' from the Psyco distribution.
Its very latest version from CVS is available at
http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/psyco/psyco/README.txt?rev=HEAD


REQUIREMENTS
------------

Psyco accelerates the execution of Python code. It requires Python 2.1
or 2.2 and a 386-compatible processor. It should be OS-independant,
although it has only be tested on Linux and Windows. It might work on
FreeBSD (at some point it did). Please report any problem you have on
other OSes.

This is relatively stable software, althought it has not been
extensively tested on real-world applications. Note that Psyco should
work a little better with Python 2.2 than 2.1; in particular, using
built-in types as classes is recommended (see comments in
'psyco/classes.py').


INSTALLATION
------------

As usual::

    python setup.py build
    python setup.py install


USAGE
-----

In your project, import the 'psyco' package. The functions you want
Psyco to accelerate must be marked. Psyco only accelerates the
execution of marked functions, and of the functions that these ones
call, and so on recursively (up to a limit).
Example::

    import psyco
    psyco.bind(f)    # 'f' is your algorithmic computation function
    psyco.bind(g)    # 'g' is another one

If you give a class to bind(), all methods within that class are
bound. With Python 2.2, all classes defined after the line::

    from psyco.classes import *

automatically become a new-style class; its methods and the methods
of the inheriting classes are all automatically bound.

For more information see the documentation included in the Psyco
package, in 'psyco/__init__.py'. With Python 2.2 you can read it by
typing 'help("psyco")'.


DOCUMENTATION AND LATEST VERSIONS
---------------------------------

* Home page: http://psyco.sourceforge.net
* 'ISSUES.txt' for known issues, also available as http://psyco.sourceforge.net/bugs.html
* see below for inspecting the emitted machine code


DEBUG BUILD
-----------

To build a version of Psyco that includes debugging checks and/or
debugging output, see comments in 'setup.py'.


INSPECTING THE MACHINE CODE
---------------------------

The directory 'py-utils' contains two scripts I use to inspect the emitted
machine code. Of course, you need to know what is going on under the cover
to fully understand why the code is like this, but this can be a good starting
point. The script 'httpxam.py' reads the files 'psyco.dump' produced in debugging
mode, formats them and presents them as HTML pages to a web browser. 'httpxam.py'
itself is a web server built on Python's standard 'SimpleHTTPServer'. When
it is running, point your browser to http://127.0.0.1:8000.

'httpxam.py' probably only works on Linux. It requires 'objdump' or 'ndisasm' to
disassemble the code and 'nm' to list symbol addresses in the Python and Psyco
executables.

The cross-calling code buffers are presented as cross-linked HTML pages.
Bold lines show the targets of another jump. If preceeded by a blank line,
a bold line shows that another code buffer jumps directly at this position.
The end of the buffer is often garbage; it is not code, but data added there
(typically for the promotion of a value). There are various kind of code
buffers, depending (only) on why Psyco produced it:

:normal:        normal mainstream compilation
:respawn:       execution jumps here when an error occurs, but never did so yet
:respawned:     replaces a respawn buffer when execution has jumped here
:unify:         small buffer whose purpose is to jump back to an existing one
:load_global:   called when a change is detected in a global variable
:coding_pause:  not yet compiled, will be compiled if execution jumps here



----------

Armin Rigo.
