from __future__ import print_function
import subprocess

def compare(c_code, asm_code):
    asm_out = subprocess.check_output("./asm '%s' | grep 0: | cut -f 2" % (asm_code,), shell=True).strip()
    c_out = subprocess.check_output("gcc -I/usr/include/python2.7 test_mcg_template.c -DREPLACE_ME='%s' && ./a.out && rm a.out" % (c_code,), shell=True).strip()
    if asm_out != c_out:
        print(c_code, '!=', asm_code)
        print(c_out, '!=', asm_out)
        print()

for l in open('asm_test_cases.txt'):
    if l.strip() and ';' in l:
        compare(*l.strip().split(';'))
