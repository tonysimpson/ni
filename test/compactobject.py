import os, sys, random
import psyco
from psyco import _psyco


def do_test_1(objects):
    d = [{} for i in range(20)]
    s = [_psyco.compact() for i in range(20)]
    attrnames = list('abcdefghijklmnopqrstuvwxyz')
    for j in range(5000):
        i = random.randrange(0, 20)
        attr = random.choice(attrnames)
        if random.randrange(0, 2):
            if attr in d[i]:
                if random.randrange(0,5) == 3:
                    delattr(s[i], attr)
                    del d[i][attr]
                else:
                    assert d[i][attr] == getattr(s[i], attr)
            else:
                try:
                    getattr(s[i], attr)
                except AttributeError:
                    pass
                else:
                    raise AssertionError, attr
        else:
            obj = random.choice(objects)
            setattr(s[i], attr, obj)
            d[i][attr] = obj
    for i in range(20):
        d1 = {}
        for attr in attrnames:
            try:
                d1[attr] = getattr(s[i], attr)
            except AttributeError:
                pass
        assert d[i] == d1

def do_test(n):
    random.jumpahead(n*111222333444555666777L)
    objects = [None, -1, 0, 1, 123455+1, -99-1,
               'hel'+'lo', [1,2], {(5,): do_test}, 5.43+0.01, xrange(5)]
    do_test_1(objects)
    for o in objects[4:]:
        #print '%5d  -> %r' % (sys.getrefcount(o), o)
        assert sys.getrefcount(o) == 4


def test_all(n):
    sys.stdout.flush()
    childpid = os.fork()
    if not childpid:
        do_test(n)
        sys.exit(0)
    childpid, status = os.wait()
    return os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0

def stress_test(repeat=20):
    for i in range(repeat):
        print i
        if not test_all(i):
            print
            print "Test failed"
            break
    else:
        print
        print "OK"

# ____________________________________________________________

def read_x(k):
    return k.x + 1

def read_y(k):
    return k.y

def read_z(k):
    return k.z

psyco.bind(read_x)
psyco.bind(read_y)

def pcompact_test():
    k = _psyco.compact()
    k.x = 12
    k.z = None
    k.y = 'hello'
    print read_x(k)
    print read_y(k)
    print read_z(k)
    psyco.dumpcodebuf()

def pcompact_creat(obj):
    for i in range(11):
        k = _psyco.compact()
        k.x = (0,i,i*2)
        k.y = i+1
        k.z = None
        k.t = obj
        k.y = i+2
        k.y = i+3
        print k.x, k.y, k.z, k.t
    print sys.getrefcount(obj)

psyco.bind(pcompact_creat)

def pcompact_modif(obj):
    for i in range(21):
        k = _psyco.compact()
        #k.x = i+1
        #k.y = (i*2,i*3,i*4,i*5,i*6,i*7)
        k.x = obj
        print k.x,
        k.x = i+1
        print k.x
        #k.x = len
        #k.x = i+2
        #print k.x, k.y
    print sys.getrefcount(obj)

psyco.bind(pcompact_modif)

# ____________________________________________________________

if __name__ == '__main__':
    import time; print "break!"; time.sleep(1)
    #stress_test()
    #pcompact_test()
    pcompact_creat('hello')
    pcompact_modif('hello')
    psyco.dumpcodebuf()
