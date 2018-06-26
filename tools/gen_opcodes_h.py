import opcode

f = open('c/opcodes.h', 'w');
f.write('#ifndef _OPCODES_H\n')
f.write('#define _OPCODES_H\n')
f.write('static const char* opcode_names [] = {\n')
for name in opcode.opname:
    f.write('    "%s",\n' % (name,))
f.write('};\n')
f.write('#endif\n')
