import os, sys
import xam
from ivmdump import insnlist, insntable, stackpushes
from struct import unpack


def dump(data):
    l = len(data)
    p = 0
    result = []
    while p < l:
        mode = insnlist[ord(data[p])]
        if mode.opcode not in insntable:
            break
        p += mode.unpacksize + 1
        if p > l:
            break
        args = mode.getargs(data, p-mode.unpacksize)
        for insn in mode.insns:
            a = len(insn)-1
            if a:
                result.append('%s(%s)' % (insn[0], ','.join(map(str,args[:a]))))
                del args[:a]
            else:
                result.append(insn[0])
    return result


if __name__ == '__main__':
    if len(sys.argv) <= 1:
        print "Usage: python ivmextract.py <directory>"
        print "  psyco.dump is loaded from the <directory>."
        sys.exit(2)
    DIRECTORY = sys.argv[1]
    del sys.argv[1]
    filename = os.path.join(DIRECTORY, 'psyco.dump')
    if not os.path.isfile(filename) and os.path.isfile(DIRECTORY):
        filename = DIRECTORY
        DIRECTORY = os.path.dirname(DIRECTORY)
    outfilename = os.path.join(DIRECTORY, 'psyco.ivmdump')
    if os.path.isfile(filename):
        codebufs = xam.readdump(filename)
        f = open(outfilename, 'w')
        for codebuf in codebufs:
            if codebuf.data:
                lst = dump(codebuf.data)
                if len(lst) > 1:
                    print >> f, 'psycodump([%s]).' % ', '.join(lst)
        f.close()
    elif not os.path.isfile(outfilename):
        print >> sys.stderr, "psyco.dump not found."
        sys.exit(1)
    else:
        print >> sys.stderr, "reusing text dump from", outfilename
    print "'%s'." % outfilename

    #for n in range(256):
    #    p = stackpushes.get(n)
    #    if p is not None:
    #        print 'stackpushes(%d, %d).' % (n, p)
