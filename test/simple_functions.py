import psyco

def test1():
    return 1+1

def test2():
    return "121" + "3333"

def test3():
    return 1.01 * 0.001

psyco.bind(test1)
psyco.bind(test2)
psyco.bind(test3)

if __name__ == "__main__":
    print test1()
    print test2()
    print test3()
