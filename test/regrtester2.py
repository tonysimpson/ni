#
# A less memory-exploding version of regrtester.py.
# It runs only a fraction of the tests.
#

import sys, re, psyco


assert len(sys.argv) >= 2
match = re.match(r"(\d+)[/](\d+)", sys.argv[1])
assert match, "syntax: regrtester2.py n/m [-nodump]"
n = int(match.group(1))
m = int(match.group(2))
assert 0 <= n < m


import test.regrtest
import regrtester

tests = [s for s in test.regrtest.findtests()
         if hash(s) % m == n and s not in test.regrtest.NOTTESTS]
if __name__ == '__main__':
    try:
        test.regrtest.main(tests, randomize=1)
    finally:
        if len(sys.argv) <= 2 or sys.argv[2] != '-nodump':
            psyco.dumpcodebuf()
