import sys, re, cStringIO, os, dis, types
import xam, psyco
from SimpleHTTPServer import SimpleHTTPRequestHandler, test

#
# Adapted from SimpleHTTPServer.py.
#

def show_vinfos(array, d, co=None, path=[]):
    text = "<ol>"
    for i in range(len(array)):
        vi = array[i]
        text += "<li>"
        if hasattr(co, 'co_code') and path == []:
            j = i - xam.LOC_LOCALS_PLUS
            if 0 <= j < len(co.co_varnames):
                text += "(%s):\t" % co.co_varnames[j]
        #if name is not None:
        #    text += "%s " % name
        if vi is None:
            text += "[NULL]"
        else:
            text += "[%x] %s" % (vi.addr, vi.gettext())
            if d.has_key(vi.addr):
                text += " (already seen above)"
            else:
                d[vi.addr] = 1
            if vi.array:
                text += show_vinfos(vi.array, d, co, path+[i])
        text += '</li>\n'
    text += '</ol>\n'
    return text

re_codebuf = re.compile(r'[/]0x([0-9A-Fa-f]+)$')
re_proxy = re.compile(r'[/]proxy(\d+)$')

def cache_load(filename, cache={}):
    try:
        return cache[filename]
    except KeyError:
        data = {}
        try:
            f = execfile(filename, data)
        except:
            data = None
        cache[filename] = data
        return data

class CodeBufHTTPHandler(SimpleHTTPRequestHandler):

    def symhtml(self, sym, addr, inbuf=None):
        text = xam.symtext(sym, addr, inbuf)
        if isinstance(sym, xam.CodeBuf):
            if addr == sym.addr:
                name = ''
            else:
                name = '#0x%x' % addr
            text = "<a href='/0x%x%s'>%s</a>" % (sym.addr, name, text)
        return text

    def linehtml(self, line, addr):
        return "<a name='0x%x'><strong>%s</strong></a>" % (addr, line)

    def proxyhtml(self, proxy):
        return "<a href='/proxy%d'>(snapshot)</a>\n" % codebufs.index(proxy)

    def htmlpage(self, title, data):
        return ('<html><head><title>%s</title></head>\n'  % title
                + '<body><h1>%s</h1>\n'               % title
              # + '<hr>\n'
                + data
                + '<hr></body>\n')

    def bufferpage(self, codebuf):
        rev = {}
        for o, c in codebuf.reverse_lookup:
            if c is not codebuf:
                rev[c] = rev.get(c,0) + 1
        if rev:
            data = '<p>Other code buffers pointing to this one:</p><ul>\n'
            for c in codebufs:  # display them in original load order
                if rev.has_key(c):
                    if rev[c] == 1:
                        extra = ''
                    else:
                        extra = '\t(%d times)' % rev[c]
                    data += '<li>%s\t(%d bytes)%s</li>\n' %  \
                            (self.symhtml(c, c.addr),
                             len(c.data),
                             extra)
            data += '</ul>\n'
        else:
            data = '<p>No other code buffer points to this one.</p>\n'
        data += '<hr>\n'
        data += '<pre>%s</pre>\n' % codebuf.disassemble(self.symhtml,
                                                        self.linehtml,
                                                        self.proxyhtml)
        data += "<br><a href='/'>Back to the list of code objects</a>\n"
        if codebuf.co_name:
            data = '<p>Code object %s from file %s, at position %s</p>%s' % (
                codebuf.co_name, codebuf.co_filename, codebuf.get_next_instr(),
                data)
        return data

    def send_head(self):
        if self.path == '/' or self.path == '/all':
            all = self.path == '/all'
            if all:
                title = 'List of ALL code objects'
            else:
                title = 'List of all named code objects'
            data = '<ul>'
            named = 0
            proxies = 0
            for codebuf in codebufs:
                if codebuf.data and codebuf.co_name:
                    named += 1
                else:
                    if not codebuf.data:
                        proxies += 1
                    if not all:
                        continue
                data += '<li>%s:\t%s:\t%s:\t%s\t(%d bytes)</li>\n' % (
                    codebuf.co_filename, codebuf.co_name,
                    codebuf.get_next_instr(),
                    self.symhtml(codebuf, codebuf.addr),
                    len(codebuf.data))
            data += '</ul>\n'
            data += ('<br><a href="/">%d named buffers</a>; ' % named +
                     '<a href="/all">%d buffers in total</a>, ' % len(codebufs) +
                     'including %d proxies' % proxies)
        else:
            match = re_codebuf.match(self.path)
            if match:
                addr = long(match.group(1), 16)
                codebuf = xam.codeat(addr)
                if not codebuf:
                    self.send_error(404, "No code buffer at this address")
                    return None
                title = '%s code buffer at 0x%x' % (codebuf.mode.capitalize(),
                                                    codebuf.addr)
                data = self.bufferpage(codebuf)
            else:
                match = re_proxy.match(self.path)
                if match:
                    title = 'Snapshot'
                    proxy = codebufs[int(match.group(1))]
                    filename = os.path.join(DIRECTORY, proxy.co_filename)
                    moduledata = cache_load(filename)
                    if moduledata is None:
                        co = None
                    else:
                        co = moduledata.get(proxy.co_name)
                        try:
                            co = psyco.unproxy(co)
                        except TypeError:
                            pass
                        if hasattr(co, 'func_code'):
                            co = co.func_code
                    data = '<p>PsycoObject structure at this point:</p>\n'
                    data += show_vinfos(proxy.vlocals, {}, co)
                    data += '<hr><p>Disassembly of %s:%s:%s:</p>\n' % (
                        proxy.co_filename, proxy.co_name, proxy.get_next_instr())
                    if moduledata is None:
                        txt = "(exception while loading the file '%s')\n" % (
                            filename)
                    else:
                        if not hasattr(co, 'co_code'):
                            txt = "(no function object '%s' in file '%s')\n" % (
                                proxy.co_name, filename)
                        else:
                            txt = cStringIO.StringIO()
                            oldstdout = sys.stdout
                            try:
                                sys.stdout = txt
                                dis.disassemble(co, proxy.get_next_instr())
                            finally:
                                sys.stdout = oldstdout
                            txt = txt.getvalue()
                    data += '<pre>%s</pre>\n' % txt
                    data += "<br><a href='/0x%x'>Back</a>\n" % proxy.addr
                else:
                    self.send_error(404, "Invalid path")
                    return None
        f = cStringIO.StringIO(self.htmlpage(title, data))
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        return f


if __name__ == '__main__':
    if len(sys.argv) <= 1:
        print "Usage: python httpxam.py <directory>"
        print "  psyco.dump and any .py files containing code objects"
        print "  are loaded from the <directory>."
        sys.exit(1)
    DIRECTORY = sys.argv[1]
    del sys.argv[1]
    codebufs = xam.readdump(os.path.join(DIRECTORY, 'psyco.dump'))
    test(CodeBufHTTPHandler)
