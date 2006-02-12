#! /usr/bin/env python

"""List of source files of Psyco, the Python specializing compiler

This script can be used to rebuild various headers and the MANIFEST."""


class Source:
    def __init__(self, filename, initname=None):
        self.filename = filename
        self.initname = initname

class Object(Source):
    def __init__(self, name, has_init=1):
        if has_init:
            initname = 'psy_%s_init' % name
        else:
            initname = None
        Source.__init__(self, 'Objects/p%s.c' % name, initname)

class Module(Source):
    def __init__(self, name):
        Source.__init__(self, 'Modules/p%s.c' % name, 'psyco_init%s' % name)


SRC = [
    Source('dispatcher.c'),
    Source('vcompiler.c',	'psyco_compiler_init'),
    Source('psyco.c'),
    Source('psyfunc.c'),
    Source('stats.c',		'psyco_stats_init'),
    Source('profile.c',         'psyco_profile_init'),
    Source('cstruct.c',		'psyco_cstruct_init'),
    Source('alarm.c',		'psyco_alarm_init'),
    Source('codemanager.c'),
    Source('codegen.c',		'psyco_codegen_init'),
    Source('mergepoints.c'),
    Source('linuxmemchk.c'),
    Source('Python/pycompiler.c',	'psyco_pycompiler_init'),
    Source('Python/frames.c',		'psyco_frames_init'),
    Source('Python/pbltinmodule.c',	'psyco_bltinmodule_init'),

    Object('object'),
    Object('abstract', 0),
    Object('boolobject'),
    Object('classobject'),
    Object('descrobject'),
    Object('dictobject'),
    Object('floatobject'),
    Object('funcobject'),
    Object('intobject'),
    Object('iterobject'),
    Object('listobject'),
    Object('longobject'),
    Object('methodobject'),
    Object('rangeobject'),
    Object('stringobject'),
    Object('structmember', 0),
    Object('tupleobject'),
    Object('typeobject'),
    Source('Objects/compactobject.c',  'psyco_compact_init'),
    Object('compactobject'),

    Module('array'),
    Module('math'),
    Module('psyco'),
    ]

PROCESSOR_SRC = {
    'i386': [
        Source('iprocessor.c',	'psyco_processor_init'),
        Source('idispatcher.c'),
        Source('iencoding.c'),
        Source('ipyencoding.c'),
        ],
    'ivm': [
        Source('iprocessor.c'),
        Source('idispatcher.c'),
        Source('iencoding.c'),
        Source('ipyencoding.c'),
        Source('ivm-insns.c'),
        ],
    }

MAINFILE = 'psyco.c'


def generate(processor=None):
    if processor:
        filename = '%s/iinitialize.h' % processor
        src = PROCESSOR_SRC[processor]
    else:
        filename = 'initialize.h'
        src = SRC
    header = """\
 /***************************************************************/
/***          Automatically generated support file             ***/
 /***************************************************************/

 /* This file is automatically generated by 'files.py'.
    DO NOT MODIFY. Changes will be overwritten ! */

"""
    print 'Rebuilding %s...' % filename
    f = open(filename, 'w')
    print >> f, header
    if not processor:
        print >> f, ' /* Including this file results in all headers Objects/xxx.h'
        print >> f, '    being included, so that it has roughly the same result'
        print >> f, '    for Psyco as a "#include <Python.h>" has for Python:'
        print >> f, '    including all headers extension modules generally need.'
        print >> f
        print >> f, '    This file is moreover used internally by psyco.c. */'
        print >> f
        print >> f
        print >> f, '#ifndef PSYCO_INITIALIZATION'
        print >> f
        for s in src:
            if isinstance(s, Object):
                assert s.filename.endswith(".c")
                print >> f, '# include "%s"' % (s.filename[:-2] + ".h")
        print >> f
        print >> f, '#else /* if PSYCO_INITIALIZATION */'
        print >> f, '# undef PSYCO_INITIALIZATION'
        print >> f
        print >> f, '#include <iinitialize.h>  /* processor-specific initialization */'
        print >> f
    print >> f, '  /* internal part for psyco.c */'
    print >> f, '#if ALL_STATIC'
    for s in src:
        if processor or s.filename != MAINFILE:
            print >> f, '# include "%s"' % s.filename
    print >> f, '#else /* if !ALL_STATIC */'
    for s in src:
        if s.initname:
            print >> f, '  EXTERNFN void %s(void);\t/* %s */' % (s.initname, s.filename)
    print >> f, '#endif /* !ALL_STATIC */'
    print >> f
    if processor:
        print >> f, 'PSY_INLINE void initialize_processor_files(void) {'
    else:
        print >> f, 'PSY_INLINE void initialize_all_files(void) {'
        print >> f, '  initialize_processor_files();'
    for s in src:
        if s.initname:
            print >> f, '  %s();\t/* %s */' % (s.initname, s.filename)
    print >> f, '}'
    if not processor:
        print >> f
        print >> f, '#endif /* PSYCO_INITIALIZATION */'
    f.close()

def main():
    generate()
    for processor in PROCESSOR_SRC.keys():
        generate(processor)
    
    import os, sys; sys.path.insert(0, os.path.join(os.pardir, 'py-utils'))
    import manifest
    manifest.generate()

if __name__ == '__main__':
    main()
