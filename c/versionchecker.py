import re


FILES = {
    'Python/pyver.h': re.compile(r'PSYCO_VERSION_HEX\s+(0x[0-9a-fA-F]+)'),
    '../psyco/support.py': re.compile(r'__version__\s*=\s*(0x[0-9a-fA-F]+)'),
    '../doc/psycoguide.tex': re.compile(r'\\release\{([0-9.]+)\}'),
    '../setup.py': re.compile(r'version\s*=\s*\"([0-9.]+)\"'),
    '../../my/psyco/dist/Makefile': re.compile(r'version\s*=\s*([0-9.]+)'),
    }

versions = {}
for filename, regexp  in FILES.items():
    for line in open(filename, 'r'):
        match = regexp.search(line)
        if match:
            break
    else:
        raise Exception, "No version number found in " + filename
    ver = match.group(1)
    print '%20s  %s' % (ver, filename)
    if ver.startswith('0x'):
        ver = int(ver, 16)
        ver = str(ver >> 24) + '.' + str((ver >> 16) & 0xFF)
    versions[ver] = filename

if len(versions) != 1:
    raise Exception, versions
else:
    print "versionchecker: ok"
