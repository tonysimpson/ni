import py, sys, psyco


def test_index():
    if sys.version_info < (2, 5):
        py.test.skip("for Python 2.5")

    def f():
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
        return Observer()[X():Y()]

    assert f() == ("slice", 5, -2)
    res = psyco.proxy(f)()
    assert res == ("slice", 5, -2)
