import sys, test.regrtest

import psyco
psyco.log()
psyco.full()

#import time; print "Break!"; time.sleep(0.5)


# known to fail:
#   test_inspect --- uses sys.exc_traceback.tb_frame.f_back

#################################################################################
SKIP = {'test_gc': "test_gc.test_frame() does not create a cycle with Psyco's limited frames",
        'test_descr': 'seems that it mutates user-defined types and Psyco does not like it at all',
        'test_profilehooks': 'no profiling allowed with Psyco!',
        'test_profile': 'no profiling allowed with Psyco!',
        'test_repr': 'self-nested tuples and lists not supported',
        'test_builtin': 'vars() and locals() not supported',
        'test_trace': 'no line tracing with Psyco',
        'test_threaded_import': 'Python hang-ups',
        'test_hotshot': "PyEval_SetProfile(NULL,NULL) doesn't allow Psyco to take control back",
        'test_coercion': 'uses eval() with locals',
        'test_longexp': 'run it separately if you want, but it takes time and memory',
        }
if sys.version_info[:2] < (2,2):
    SKIP['test_scope'] = 'The jit() uses the profiler, which is buggy with cell and free vars (PyFrame_LocalsToFast() bug)'
#    SKIP['test_operator'] = NO_SYS_EXC
#    SKIP['test_strop'] = NO_SYS_EXC
if sys.version_info[:2] >= (2,3):
    SKIP['test_threadedtempfile'] = 'Python bug: Python test just hangs up'

if hasattr(psyco._psyco, 'VERBOSE_LEVEL'):
    SKIP['test_popen2'] = 'gets confused by Psyco debugging output to stderr'
#################################################################################


# the tests that don't work with Psyco
test.regrtest.NOTTESTS += SKIP.keys()
try:
    test.regrtest.main()
finally:
    psyco.dumpcodebuf()
