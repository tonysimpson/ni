from __future__ import generators
import os, cvs

def psycofiles():
    root = cvs.Directory(os.pardir)
    for dir in root.alldirs():
        for name, info in dir.fileinfo.items():
            if name != '.cvsignore':
                yield os.path.join(dir.relpath, name)

def generate():
    filename = os.path.join('..', 'MANIFEST')
    print 'Rebuilding %s...' % filename
    f = open(filename, 'w')
    for filename in psycofiles():
        print >> f, filename
    f.close()

if __name__ == '__main__':
    generate()
