import os, sys, re, htmlentitydefs, struct
import _psyco

tmpfile = '~tmpfile.tmp'

# the disassembler to use. 'objdump' writes GNU-style instructions.
# 'ndisasm' uses Intel syntax.

objdump = 'objdump -b binary -m i386 --adjust-vma=%(origin)d -D %(file)s'
#objdump = 'ndisasm -o %(origin)d -u %(file)s'

# the files from which symbols are loaded.
# the order and number of files must match
# psyco_dump_code_buffers() in psyco.c.
symbolfiles = [sys.executable, _psyco.__file__]

# the program that lists symbols, and the output it gives
symbollister = 'nm %s'
re_symbolentry = re.compile(r'([0-9a-fA-F]+)\s\w\s(.*)')

re_addr = re.compile(r'[\s,$]0x([0-9a-fA-F]+)')
re_lineaddr = re.compile(r'\s*0?x?([0-9a-fA-F]+)')


symbols = {}
rawtargets = {}

def load_symbol_file(filename, symb1, addr1):
    d = {}
    g = os.popen(symbollister % filename, "r")
    while 1:
        line = g.readline()
        if not line:
            break
        match = re_symbolentry.match(line)
        if match:
            d[match.group(2)] = long(match.group(1), 16)
    g.close()
    if symb1 in d:
        delta = addr1 - d[symb1]
    else:
        delta = 0
        print "Warning: no symbol '%s' in '%s'" % (symb1, filename)
    for key, value in d.items():
        symbols[value + delta] = key


def symtext(sym, addr, inbuf=None):
    if isinstance(sym, CodeBuf):
        if sym is inbuf:
            name = 'top'
        else:
            name = '%s codebuf 0x%x' % (sym.mode, sym.addr)
        if addr > sym.addr:
            name += ' + %d' % (addr-sym.addr)
        return name
    else:
        return sym


revmap = {}
for key, value in htmlentitydefs.entitydefs.items():
    if type(value) is type(' '):
        revmap[value] = '&%s;' % key

def htmlquote(text):
    return ''.join([revmap.get(c,c) for c in text])

def lineaddresses(line):
    result = []
    i = 0
    while 1:
        match = re_addr.search(line, i)
        if not match:
            break
        i = match.end()
        addr = long(match.group(1), 16)
        result.append(addr)
    return result


LOC_LOCALS_PLUS = 2

class CodeBuf:
    
    def __init__(self, mode, co_filename, co_name, nextinstr,
                 addr, data, vlocals):
        self.mode = mode
        self.co_filename = co_filename
        self.co_name = co_name
        self.nextinstr = nextinstr
        self.addr = addr
        self.data = data
        self.vlocals = vlocals
        #self.reverse_lookup = []  # list of (offset, codebuf pointing there)
        self.specdict = []
        for i in range(len(data)):
            symbols[self.addr+i] = self
        sz = struct.calcsize('l')
        for i in range(sz, len(data)+1):
            offset, = struct.unpack('l', data[i-sz:i])
            rawtargets.setdefault(addr+i+offset, {})[self] = 1

    def __getattr__(self, attr):
        if attr == 'cache_text':
            # produce the disassembly listing
            data = self.data
            addr = self.addr
            if data[:4] == '\x66\x66\x66\x66':
                # detected a rt_local_buf_t structure
                next, key = struct.unpack('ll', data[4:12])
                data = data[12:]
                addr += 12
                self.cache_text = [
                    'Created by promotion of the value 0x%x\n' % key,
                    'Next promoted value at buffer 0x%x\n' % next,
                    ]
            else:
                self.cache_text = []
            f = open(tmpfile, 'wb')
            f.write(data)
            f.close()
            try:
                g = os.popen(objdump % {'file': tmpfile, 'origin': addr},
                             'r')
                self.cache_text += g.readlines()
                g.close()
            finally:
                os.unlink(tmpfile)
            return self.cache_text
        if attr == 'disass_text':
            txt = self.cache_text
            if self.specdict:
                txt.append('\n')
                txt.append("'do_promotion' dictionary:\n")
                for key, value in self.specdict:
                    txt.append('.\t%s:\t\t\n' % htmlquote(key))
                    txt.append('.\t\t0x%x\t\t\n' % value)
            self.disass_text = txt
            return txt
        if attr == 'reverse_lookup':
            # 'reverse_lookup' is a list of (offset, codebuf pointing there)
            maybe = {self: 1}
            self.reverse_lookup = []
            start = self.addr
            end = start + len(self.data)
            for addr in range(start, end):
                if addr in rawtargets:
                    for codebuf in rawtargets[addr]:
                        maybe[codebuf] = 1
            for codebuf in maybe.keys():
                for line in codebuf.disass_text:
                    for addr in lineaddresses(line):
                        if start <= addr < end:
                            self.reverse_lookup.append((addr-start, codebuf))
            return self.reverse_lookup
        raise AttributeError, attr

    def get_next_instr(self):
        if self.nextinstr >= 0:
            return self.nextinstr

    def spec_dict(self, key, value):
        self.specdict.append((key, value))
        rawtargets.setdefault(value, {})[self] = 1
        try:
            del self.disass_text
        except:
            pass
        try:
            del self.reverse_lookup
        except:
            pass
    
##    def build_reverse_lookup(self):
##        for line in self.disass_text:
##            for addr in lineaddresses(line):
##                sym = symbols.get(addr)
##                if isinstance(sym, CodeBuf):
##                    sym.reverse_lookup.append((addr-sym.addr, self))
    
    def disassemble(self, symtext=symtext, linetext=None, snapshot=None):
        seen = {}
        data = []
        for line in self.disass_text:
            if line.endswith('\n'):
                line = line[:-1]
            match = re_lineaddr.match(line)
            if match:
                addr = long(match.group(1), 16)
                if addr not in seen:
                    if addr in self.codemap and snapshot:
                        for proxy in self.codemap[addr]:
                            data.append(snapshot(proxy))
                    seen[addr] = 1
                ofs = addr - self.addr
                sources = [c for o, c in self.reverse_lookup if o == ofs]
                if sources and linetext:
                    line = linetext(line, self.addr + ofs)
                if sources != [self]*len(sources):
                    data.append('\n')
            for addr in lineaddresses(line):
                sym = symbols.get(addr)
                if sym:
                    line = '%s\t(%s)' % (line, symtext(sym, addr, self))
                    break
            data.append(line + '\n')
        return ''.join(data)


class VInfo:
    pass

class CompileTimeVInfo(VInfo):
    def __init__(self, flags, value):
        self.flags = flags
        self.value = value
    def gettext(self):
        text = "Compile-time value 0x%x" % self.value
        if self.flags & 1:
            text += ", fixed"
        if self.flags & 2:
            text += ", reference"
        return text

class RunTimeVInfo(VInfo):
    REG_NAMES = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"]
    def __init__(self, source, stackdepth):
        self.source = source
        self.stackdepth = stackdepth
    def gettext(self):
        text = "Run-time source,"
        reg = self.source >> 28
        stack = self.source & 0x07FFFFFC
        if 0 <= reg < 8:
            text += " in register %s" % self.REG_NAMES[reg].upper()
            if stack:
                text += " and"
        if stack:
            text += " in stack [ESP+0x%x] or from top %d" %  \
                    (self.stackdepth - stack, stack)
        if not (self.source & 0x08000000):
            text += " holding a reference"
        return text

class VirtualTimeVInfo(VInfo):
    def __init__(self, vs):
        self.vs = vs
    def gettext(self):
        return "Virtual-time source (%x)" % self.vs

def readdump(filename = 'psyco.dump'):
    re_symb1 = re.compile(r"(\w+?)[:]\s0x([0-9a-fA-F]+)")
    re_codebuf = re.compile(r"CodeBufferObject 0x([0-9a-fA-F]+) (\d+) (\-?\d+) \'(.*?)\' \'(.*?)\' (\-?\d+) \'(.*?)\'$")
    re_specdict = re.compile(r"spec_dict 0x([0-9a-fA-F]+)")
    re_spec1 = re.compile(r"0x([0-9a-fA-F]+)\s(.*)$")
    re_int = re.compile(r"(\-?\d+)$")
    re_ctvinfo = re.compile(r"ct (\d+) (\-?\d+)$")
    re_rtvinfo = re.compile(r"rt (\-?\d+)$")
    re_vtvinfo = re.compile(r"vt 0x([0-9a-fA-F]+)$")

    def load_vi_array(d):
        match = re_int.match(dumpfile.readline())
        assert match
        count = int(match.group(1))
        a = []
        for i in range(count):
            line = dumpfile.readline()
            match = re_int.match(line)
            assert match
            addr = long(match.group(1))
            if addr in d:
                vi = d[addr]
            else:
                line = dumpfile.readline()
                match = re_ctvinfo.match(line)
                if match:
                    vi = CompileTimeVInfo(int(match.group(1)),
                                          long(match.group(2)))
                else:
                    match = re_rtvinfo.match(line)
                    if match:
                        vi = RunTimeVInfo(long(match.group(1)), stackdepth)
                    else:
                        match = re_vtvinfo.match(line)
                        assert match
                        vi = VirtualTimeVInfo(long(match.group(1), 16))
                d[addr] = vi
                vi.addr = addr
                vi.array = load_vi_array(d)
            a.append(vi)
        a.reverse()
        return a
    
    codebufs = []
    dumpfile = open(filename, 'rb')
    dumpfile.seek(0,2)
    filesize = float(dumpfile.tell())
    nextp = 0.1
    dumpfile.seek(0,0)
    for filename in symbolfiles:
        line = dumpfile.readline()
        match = re_symb1.match(line)
        assert match
        load_symbol_file(filename, match.group(1), long(match.group(2), 16))
    while 1:
        line = dumpfile.readline()
        if not line:
            break
        #print line.strip()
        match = re_codebuf.match(line)
        if match:
            percent = dumpfile.tell() / filesize
            if percent >= nextp:
                sys.stderr.write('%d%%...' % int(100*percent))
                nextp += 0.1
            size = int(match.group(2))
            stackdepth = int(match.group(3))
            vlocals = load_vi_array({0: None})
            data = dumpfile.read(size)
            assert len(data) == size
            codebuf = CodeBuf(match.group(7), match.group(4), match.group(5),
                              int(match.group(6)), long(match.group(1), 16),
                              data, vlocals)
            codebuf.complete_list = codebufs
            codebufs.insert(0, codebuf)
        else:
            match = re_specdict.match(line)
            if match:
                addr = long(match.group(1), 16)
                for codebuf in codebufs:
                    if codebuf.addr < addr <= codebuf.addr+len(codebuf.data):
                        break
                else:
                    raise "spec_dict with no matching code buffer", line
                while 1:
                    line = dumpfile.readline()
                    if len(line)<=1:
                        break
                    match = re_spec1.match(line)
                    assert match
                    codebuf.spec_dict(match.group(2), long(match.group(1), 16))
            else:
                raise "invalid line", line
    dumpfile.close()
    codemap = {}
    for codebuf in codebufs:
        #codebuf.build_reverse_lookup()
        codebuf.codemap = codemap
        codemap.setdefault(codebuf.addr, []).insert(0, codebuf)
    sys.stderr.write('100%\n')
    return codebufs

if __name__ == '__main__':
    if len(sys.argv) > 1:
        codebufs = readdump(sys.argv[1])
    else:
        codebufs = readdump()
    for codebuf in codebufs:
        print codebuf.disassemble()
