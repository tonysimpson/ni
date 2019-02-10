# Ni JIT

Ni aims to provide a module that just makes Python faster.

Ni is based on a fork of Armin Rigo's Psyco, Armin and the other Psyco
developers moved onto PyPy so go check that out if you want a high performance
implementation of Python (http://pypy.org).

## Current state

* 64 bit port working ok but machine code is far from optimal
* Most psyco limitations and bugs are still present

## Milestones

* Improve debugging and execution performance analysis - nianalyser
* Refactor code for maintainability and to CPython coding standards - On-going
* Make Ni work on all Python code
* Improve preformance and implement better optimisation decision making
* Python 3 port


