import psyco

def engage():
    """Turn on optimisation"""
    psyco.full()

def disengage():
    """Turn off optimisation"""
    psyco.stop()

