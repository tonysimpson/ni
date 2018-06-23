#!/usr/bin/env python
from __future__ import print_function

def gen_blocks():
    begin = None
    end = None
    for line in open('codegen.log'):
        if 'BEGIN_CODE' in line:
            begin = line.split()[-1]
        elif 'END_CODE' in line:
            end = line.split()[-1]
            if begin != end:
                yield begin, end

def hex_to_int(h):
    return int(h, 16)

        
