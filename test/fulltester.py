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

PSYCO_MODES = [
    # debugging mode, static compiling
    {'PSYCO_DEBUG': 1, 'VERBOSE_LEVEL': 1, 'CODE_DUMP': 1, 'ALL_STATIC': 1},
    # optimized mode, static compiling
    {'PSYCO_DEBUG': 0, 'VERBOSE_LEVEL': 0, 'CODE_DUMP': 0, 'ALL_STATIC': 1},
    ]

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


def run(path, *argv):
    print '='*10, path, ' '.join(argv)
    sys.stdout.flush()
    err = os.spawnv(os.P_WAIT, path, [path]+list(argv))
    if err:
        print '='*60
        print '*** exited with error code', err
        sys.exit(err)

def do_compile(python_version, psyco_mode):
    print '~'*60
    print psyco_mode
    preffile = os.path.join(os.pardir, 'preferences.py')
    try:
        f = open(preffile, 'r')
        backup = f.read()
        f.close()
    except IOError:
        backup = ''
    cwd = os.getcwd()
    try:
        f = open(preffile, 'w')
        for varvalue in psyco_mode.items():
            f.write('%s = %s\n' % varvalue)
        f.close()
        os.chdir(os.pardir)
        run(python_version, 'setup.py', 'build', '-f')
        run(python_version, 'setup.py', 'install')
    finally:
        os.chdir(cwd)
        f = open(preffile, 'w')
        f.write(backup)
        f.close()

def test_with(python_version, psyco_mode, running_mode):
    #
    # Compile the appropriate Psyco version
    #
    global compiled_version
    if (python_version, psyco_mode) != compiled_version:
        do_compile(python_version, psyco_mode)
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
    print '='*10, 'Mode %.14s: %s' % (psyco_mode, running_mode)
    run(python_version, *script.split())
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
