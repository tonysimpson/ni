import ni
ni.engage()

import re
def add(a, b):
    return a + b
print(add(6, 9))
print(add("abc", "def"))
print(add([1,2,3,4], [5,6,7,8]))

def test2(a, b, c):
    print a, b, c

def test1(a, b, c):
    test2(a, b, c)

test1(1, 2, 3)
