from ni import engage, disengage
import docopt


__usage__ = """\
Usage:
    ni FILENAME

"""

def main():
    opts = docopt.docopt(__usage__)
    filename = opts['FILENAME']
    _globals = {
        '__file__': filename,
        '__name__': '__main__',
        '__package__': None,    
    }
    co = compile(open(filename).read(), filename, 'exec')
    engage()
    exec co in _globals
    disengage()

if __name__ == '__main__':
    main()


