#
# Trivial script that searches for identical lines in stdin.
# stdout report is formatted according to sys.argv[1].
#
import sys

def samelines(infile, outfile, format, verbose=1):
    lines = {}
    total = 0
    try:
        for line in infile:
            verbose -= 1
            if not verbose:
                if total:
                    print >> sys.stderr, '%d lines, %d without duplicates...' % (
                        total, len(lines))
                total += 10000
                verbose = 10000
            if line.endswith('\n'):
                line = line[:-1]
            lines[line] = lines.get(line, 0) + 1
    finally:
        for line_count in lines.iteritems():
            print >> outfile, format % line_count

if __name__ == '__main__':
    format = sys.argv[1]
    samelines(sys.stdin, sys.stdout, format)
