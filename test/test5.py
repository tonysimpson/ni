import psyco, dis, types, sys

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

def exc_test():
    try:
        failure = [][1]   # opcode 9, line 2 from the start of the function
    except:
        exc, value, tb = sys.exc_info()
        print exc.__name__, str(value)
        print tb.tb_lineno - tb.tb_frame.f_code.co_firstlineno

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


if __name__ == '__main__':
    import time
    print "break!"
    time.sleep(0.5)
    #print f('test5.py')
    #print f2(11,22,33)
    #print f2(11,22,(5,))
    #print f3(-15, 32)
    #psyco.bind(f3)
    #print f3(-15, 32)
    #psyco.bind(f4)
    #print f4("some-string")
    psyco.full()
    #print overflowtest()
    exc_test()
    psyco.dumpcodebuf()
