
all: ni_gdb_jit_reader.so
	
ni_gdb_jit_reader.so: src/ni_gdb_jit_reader.c 
	gcc -g -O2 -Wall -Wstrict-prototypes -fPIC -shared -o ni_gdb_jit_reader.so src/ni_gdb_jit_reader.c 
