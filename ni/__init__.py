import psyco
from psyco import proxy, bind
from time import time as get_time

def engage():
    """Turn on optimisation
    
    """
    psyco.full()

def disengage():
    """Turn off optimisation
    
    """
    psyco.stop()
    
def timeit_iter(func):
    """Yield func call time in seconds _forever_
    
    """
    while True:
        st = get_time()
        func()
        yield get_time() - st