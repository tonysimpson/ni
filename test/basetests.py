#! /usr/bin/env python

"""  Run base Psyco tests.
"""

import sys, os, random, doctest, cStringIO


SEPARATOR = """
========== %r ==========
"""
LASTLINE = "Tests completed."
BUFFERFILE = "buffer-basetests.txt"
EXPECTEDFILE = "expected-basetests.txt"

TESTS = open('btrun.py', 'r').read()


tests = doctest._extract_examples(TESTS)
PRELUDE = ''
for inp, outp, line in tests[:]:
    if not outp:
        PRELUDE += inp + '\n'
        tests.remove((inp, outp, line))
random.shuffle(tests)             # first run all tests in any order
tests_again = tests[:]
random.shuffle(tests_again)
all_tests = tests + tests_again   # then run them all again in any other order

# run in a child process
childin = os.popen('%s > %s' % (sys.executable, BUFFERFILE), 'w')
expected = open(EXPECTEDFILE, 'w')

print >> childin, 'import sys'
print >> childin, PRELUDE

for inp, outp, line in all_tests:
    sep = SEPARATOR % inp
    print >> childin, 'print %r' % sep
    print >> childin, 'print >> sys.stderr, %r' % inp
    print >> expected, sep
    print >> childin, inp
    expected.write(outp)

print >> childin, 'print %r' % LASTLINE
print >> expected, LASTLINE

expected.close()
err = childin.close()
if err:
    print >> sys.stderr, 'child process returned %d, %d' % (err/256, err%256)
else:
    os.system('diff -c %s %s' % (EXPECTEDFILE, BUFFERFILE))
