import py, sys, psyco


def test_index():
    if sys.version_info < (2, 5):
        py.test.skip("for Python 2.5")

    class X(object):
        def __index__(self):
            return -3

    def f(x):
        return (12, 23, 34, 45)[x]

    res = psyco.proxy(f)(X())
    assert res == 23


def test_index_slice():
    if sys.version_info < (2, 5):
        py.test.skip("for Python 2.5")

    class Observer(object):
        def __getslice__(self, i, j):
            return "slice", i, j
        def __getitem__(self, x):
            raise Exception("in __getitem__")
    class X(object):
        def __index__(self):
            return 5
    class Y(object):
        def __index__(self):
            return -2

    def f(o, x, y):
        return o[x:y]

    res = psyco.proxy(f)(Observer(), X(), Y())
    assert res == ("slice", 5, -2)

def test_index_repeat():
    if sys.version_info < (2, 5):
        py.test.skip("for Python 2.5")

    class X(object):
        def __index__(self):
            return 3

    def f(x):
        return (12, 23) * x

    res = psyco.proxy(f)(X())
    assert res == (12, 23, 12, 23, 12, 23)

def test_slice_overflow():
    class X(object):
        def __len__(self):
            return sys.maxint
        def __getslice__(self, i, j):
            return i, j
    def f(X):
        return X()[-(2**100):2**100]

    res = psyco.proxy(f)(X)
    assert res == f(X)

def test_id():
    def f(x):
        return id(x)
    obj = []
    res = psyco.proxy(f)(obj)
    assert res == id(obj)
