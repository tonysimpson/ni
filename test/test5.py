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
