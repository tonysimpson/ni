#
# The Ultimate Test Suite (tm)
#
import sys, os, time

#
# This script should do the following:
#
#  for interpreter-executable-path in file('fulltester.local'):
#      for mode in (debug, optimized):
#          compile Psyco for 'mode' and 'interpreter'
#          for testset in testset_list:
#              start 'interpreter' running 'testset'
#
# The testset_list is called RUNNING_MODES below, and has the 
# following entries:

# basetests.py        -- run elementary tests
# regrtester2.py 0/5  -- 1st fifth of all regr tests with psyco.full()
# regrtester2.py 1/5  -- 2nd fifth of all regr tests with psyco.full()
# regrtester2.py 2/5  -- 3rd fifth of all regr tests with psyco.full()
# regrtester2.py 3/5  -- 4th fifth of all regr tests with psyco.full()
# regrtester2.py 4/5  -- 5th fifth of all regr tests with psyco.full()
# (idem)              -- the same, with psyco.profile()
#

try:
    f = open('fulltester.local')
except IOError:
    print >> sys.stderr, "You must create a file 'fulltester.local' containing a"
    print >> sys.stderr, "list of Python executables of various versions."
    sys.exit(2)

PYTHON_VERSIONS = [s.strip() for s in f.readlines()]
f.close()

PSYCO_MODES = ['G', 'O']

RUNNING_MODES = [
    ("basetests.py",  {}),
    ("regrtester2.py 0/5 -nodump", {"regrtester.local": "psyco.full()"}),
    ("regrtester2.py 1/5 -nodump", {"regrtester.local": "psyco.full()"}),
    ("regrtester2.py 2/5 -nodump", {"regrtester.local": "psyco.full()"}),
    ("regrtester2.py 3/5 -nodump", {"regrtester.local": "psyco.full()"}),
    ("regrtester2.py 4/5 -nodump", {"regrtester.local": "psyco.full()"}),
    ("regrtester2.py 0/5 -nodump", {"regrtester.local": "psyco.profile()"}),
    ("regrtester2.py 1/5 -nodump", {"regrtester.local": "psyco.profile()"}),
    ("regrtester2.py 2/5 -nodump", {"regrtester.local": "psyco.profile()"}),
    ("regrtester2.py 3/5 -nodump", {"regrtester.local": "psyco.profile()"}),
    ("regrtester2.py 4/5 -nodump", {"regrtester.local": "psyco.profile()"}),
    ]

compiled_version = None


def run(cmd):
    print '='*10, cmd
    err = os.system(cmd)
    if err:
        print '='*60
        print '*** exited with error code', err
        sys.exit(err>>256)

def test_with(python_version, psyco_mode, running_mode):
    #
    # Compile the appropriate Psyco version
    #
    global compiled_version
    if (python_version, psyco_mode) != compiled_version:
        run('cd ../c; make python=%s mode=%s' % (python_version, psyco_mode))
        compiled_version = (python_version, psyco_mode)
    #
    # Prepare the running mode
    #
    script, prefiles = running_mode
    for filename, content in prefiles.items():
        f = open(filename, 'w')
        print >> f, content
        f.close()
    #
    # Run it
    #
    print '='*10, 'Mode %s: %s' % (psyco_mode, running_mode)
    run('%s %s' % (python_version, script))
    #
    # Passed
    #
    tests_passed.append((python_version, psyco_mode, running_mode))
    f = open(passed_filename, 'w')
    f.write(repr(tests_passed))
    f.close()
    #
    # Make a little pause to catch our breath again
    #
    time.sleep(1)


def test():
    global tests_passed, passed_filename
    passed_filename = "tmp_fulltests_passed"
    try:
        f = open(passed_filename)
        tests_passed = eval(f.read())
        f.close()
    except IOError:
        tests_passed = []

    tests = [(python_version, psyco_mode, running_mode)
             for python_version in PYTHON_VERSIONS
             for psyco_mode in PSYCO_MODES
             for running_mode in RUNNING_MODES]
    tests.sort()
    later_tests = []
    for mode in tests:
        if mode in tests_passed:
            later_tests.append(mode)
        else:
            test_with(*mode)
    for mode in later_tests:
        test_with(*mode)
    try:
        os.unlink(passed_filename)
    except:
        pass
    try:
        os.unlink("regrtester.local")
    except:
        pass
    print "="*60
    print
    print "The Ultimate Test Suite (tm) passed !"
    print
    print "="*60


if __name__ == '__main__':
    test()
