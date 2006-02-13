import psyco, dis, types, sys, math

def f(filename, mode='r'):
    s = ""
    for line in open(filename, mode).readlines():
        s += "! " + line  # extreme optimization case
    return s

def f2(x, y, z):
    coords = (x, y)
    if type(z) is types.IntType:
        coords += (z,)
    else:
        coords += z
    a, b, c = coords
    return c, b, a

def f3(x, y):
    return x << y, x << 1, x << 31, x >> y, x >> 1, x >> 31

def f4(s):
    return "abc" + s

def double2(x):
    return x * 2

def overflowtest():
    import sys; PY21 = sys.hexversion < 0x02020000
    try:
        n = double2(-1925703681)
        assert not PY21
    except OverflowError:
        n = -3851407362L
        assert PY21
    return `n`

def booltest():
    True,False = 1==1,1==0
    print 'abcdefghijklmnop'=='abcdefghijklMNop'
    print 'abcdefghijklmnop'=='abcdefghijklmnop'
    print str('abcdefghijklmnop'=='abcdefghijklMNop')
    print str('abcdefghijklmnop'=='abcdefghijklmnop')
    print (lambda x: x==1)(0)
    print (lambda x: x==1)(1)
    print [a & b for a in (False,True) for b in (False,True)]
    print [a | b for a in (False,True) for b in (False,True)]
    print [a ^ b for a in (False,True) for b in (False,True)]
    print not (False in ())

def exc_test():
    try:
        failure = [][1]   # opcode 9, line 2 from the start of the function
    except:
        exc, value, tb = sys.exc_info()
        print exc.__name__, str(value)
        print tb.tb_lineno - tb.tb_frame.f_code.co_firstlineno

def seqrepeat():
    for i in [5, 6L, -999]:
        print `i*'abc'`
        print `'abc'*i`
        print i*[3,'z']
        print [6,3]*i
        print `('y'*i) + 'x'`

def inplacerepeat():
    l = l1 = range(100)
    l *= 2
    return l is l1

def g5(x):
    return x+1

def f5(n):
    "Function inlining tests"
    print g5(n)
    print g5(n)
    print g5(3)
    print g5(x=n)

def g6(x):
    if x != 3:
        return x+1
    else:
        return None

def g6bis(n):
    return range(2, 50)[n]

def f6(n):
    "Function inlining tests"
    try:
        print g6(n)
        print g6(n)
        print g6(3)
        print g6(x=n)
        print g6bis(n)
    except:
        print sys.exc_type

def g7(x, *args):
    if not g7len(args):
        last = g5(0)
    else:
        last = args[-1]
    return last * x

def g7bis(tuple):
    return 0.0

def f7(n):
    global g7len
    g7len = len
    "Function inlining tests"
    print [g7(n), 5*g7(3,4,5), g7(*((11,)*5))]
    g7len = g7bis
    print [g7(n), 5*g7(3,4,5), g7(*((11,)*5))]

def g8(x, *args):
    return g7len(args) * x

def f8(n):
    global g7len
    g7len = len
    "Function inlining tests"
    print [g8(n), 5*g8(3,4,5), g8(*((11,)*5))]
    g7len = g7bis
    print [g8(n), 5*g8(3,4,5), g8(*((11,)*5))]

def f9(n):
    return g5(3), g5(n)


# virtual string tests

TEST = ['userhru', 'uiadsfz', '', '1', 'if', '623', 'opad', 'oa8ua',
        '09q34rx093q', '\x00',
        'qw09exdqw0e9dqw9e8d8qw9r8qw', '',
        '\x1d\xd7\xae\xa2\x06\x10\x1a\x00a\xff\xf6\xee\x15\xa2\xea\x89',
        'akjsdfhqweirewru 3498cr 3849rx398du389du389dur398d31623']

def s1():
    s = ''
    for t in TEST:
        s += t
    return s

def s2():
    return "some" + "another"

def s3():
    return TEST[0] + '.'

def s4():
    return '.' + TEST[0]

def s5():
    return TEST[0] + (TEST[1] + TEST[2])

def s6():
    s = ''
    for t in TEST:
        s = t + s
    return s

def s7():
    return int(TEST[-1][5:10] == 'fhqwe')

def s8():
    return TEST[-2][5:10][3]

def s9():
    return TEST[-1][:-1][1:][2:-3]

def s10():
    return int('t' < TEST[1][:1] == 'u')

def s11():
    return int('ia' == TEST[1][1:3] < 'ib' and
               'dsf' == TEST[1][-4:-1] < 'dsf!' and
               'dsfz' == TEST[1][-4:] <= 'dsfz')

def s12():
    return int(TEST[0] + TEST[1] != 'userhruuiadsfz')

def s13():
    return int(TEST[3] + TEST[5] == TEST[-1][-4:])

def teststrings():
    print `s1()`
    print `s2()`
    print `s3()`
    print `s4()`
    print `s5()`
    print `s6()`
    print `s7()`
    print `s8()`
    print `s9()`
    print `s10()`
    print `s11()`
    print `s12()`
    print `s13()`

def testslices(s):
    "Test the various paths the code can take in pstring_slice()."
    print `s[:-9999]`
    print `s[:9999]`
    print `s[:]`
    print `s[-9999:-9999]`
    print `s[-9999:9999]`
    print `s[-9999:]`
    print `s[1:-9999]`
    print `s[1:9999]`
    print `s[1:]`
    print `s[9999:-9999]`
    print `s[9999:9999]`
    print `s[9999:]`

def testovf(x, y):
    import sys; PY21 = sys.hexversion < 0x02020000
    if PY21:
        x = long(x)  # don't really test promotion with Python 2.1
        y = long(y)
    print x+y
    print 2000000000+y
    print x+2000000000
    print x-y
    print 2000000000-y
    print x-2000000000
    print x*y
    print 2000000000*y
    print x*2000000000
    print x
    print -x
    print abs(x)

def rangetypetest(n):
    print type(range(n)).__name__
    print type(range(2, n)).__name__
    print type(range(n, 2, -1)).__name__
    print type(xrange(n)).__name__
    print type(xrange(2, n)).__name__
    print type(xrange(n, 2, -1)).__name__

def rangetest(n):
    for i in range(n):
        print i,
    print
    for i in range(10, n):
        print i,
    print
    for i in range(n, 10, -1):
        print i,
    print

def xrangetest(n):
    for i in xrange(n):
        print i,
    print
    for i in xrange(10, n):
        print i,
    print
    for i in xrange(n, 10, -1):
        print i,
    print

def longrangetest():
    L = 1234567890123456789L
    if sys.hexversion >= 0x02030000:
        print range(L, L+2)
    else:
        print [L, L+1]

def proxy_defargs():
    d = {}
    exec """
def f():
    g()
def g(a=12):
    print a
""" in d
    d['f'] = psyco.proxy(d['f'])
    d['g'] = psyco.proxy(d['g'])
    d['f']()

def setfilter():
    def f1():
        return ('f1', int(__in_psyco__))
    def f2():
        "TAGGY"
        return ('f2', int(__in_psyco__))
    def myfilter(co):
        return co.co_consts[0] != 'TAGGY'
    psyco.setfilter(myfilter)
    try:
        print psyco.proxy(f1)()  # ('f1', 1)
        print psyco.proxy(f2)()  # ('f2', 0)
    finally:
        prev = psyco.setfilter(None)
        assert prev is myfilter

if sys.version_info < (2,3):
    def arraytest():
        print "S"
else:
    def arraytest():
        import array
        class S(array.array): pass
        print type(S('i')).__name__


class CompositeElement:
    def __init__(self):
        self.children = []

    def append(self, child):
        self.children.append(child)
        return self

def makeSelection():
    """Segfault bug (thanks xorAxAx)"""
    result = CompositeElement()
    result.append(CompositeElement().append(42))
    print 'do stuff here'
    assert isinstance(result.children[0], CompositeElement)

class Class1:
    __metaclass__ = type
def class_creation_1(n=400000):
    for i in xrange(n):
        Class1()
    print "ok"

class Class2:
    __metaclass__ = type
    def __init__(self, a):
        self.a = a
def class_creation_2(n=400000):
    for i in xrange(n):
        c = Class2(i)
        assert c.a == i
    print "ok"

class Class3:
    __metaclass__ = type
    def __init__(self):
        return 42
def class_creation_3():
    try:
        Class3()
    except TypeError:
        if (2,2) <= sys.version_info < (2,5):
            print "got a TypeError, but new-style classes don't"
            print "check the __init__() return value before 2.5"
        else:
            print "ok"
    else:
        if (2,2) <= sys.version_info < (2,5):
            print "ok"
        else:
            print "__init__() => 42: should have raised TypeError!"

class Class4:
    __metaclass__ = type
    def __init__(self, a=None):
        self.a = a
def class_creation_4(n=200000):
    for i in xrange(n):
        c = Class4(a=i)
        assert c.a == i
    print "ok"

def power_int(n=2000):
    for j in range(n):
        x = 0
        for i in range(1000):
            x += i ** 2
    print x

def power_int_long(n=200):
    for j in range(n):
        x = 0
        for i in range(1000):
            x += i ** 2L
    print type(x).__name__[:4], int(x)

def power_float(n=500):
    for j in range(n):
        x = 0.0
        for i in range(1000):
            x += i ** 1.409999992
    print type(x).__name__, int(x)


if __name__ == '__main__':
    from test1 import go, print_results
    import time
    print "break!"
    time.sleep(0.5)
    go(class_creation_1)
    go(class_creation_2)
    go(class_creation_3)
    go(class_creation_4)
    #go(power_int)
    #go(power_int_long)
    #go(power_float)
    psyco.dumpcodebuf()
    print_results()
