#Ni JIT

Ni aims to provide a module that just makes Python faster.

Ni is based on a fork of Armin Rigo's Psyco, Armin and the other Psyco
developers moved onto PyPy so go check that out if you want a high performance
implementation of Python (http://pypy.org).

Ni is not yet fit for use. It will be several months before it is ready to release.

##Milestones

* POC 64bit port <- current, IVM is working but buggy, x86_64 backend is planned and being worked on
* Improve debugging and execution performance analysis
* Refactor code for maintainability and to CPython coding standards - On-going
* Make Ni work on all Python code
* Improve preformance and implement better optimisation decision making
* Python 3 port


