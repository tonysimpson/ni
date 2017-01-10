#include "../debug_info.h"

int main(int argc, char *argv[]) 
{
    struct ni_debug_info_t *di;
    di = ni_debug_info_create();
    ni_debug_info_compiler_begin(di, "bob.c", "hello_bob", (char*)10, 1);
    ni_debug_info_compiler_at_line_number(di, (char*)10, 1); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)10, 3);
    ni_debug_info_compiler_at_line_number(di, (char*)12, 2); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)12, 4);
    ni_debug_info_compiler_at_line_number(di, (char*)14, 3); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)14, 3);
    ni_debug_info_compiler_end(di, (char*)16);
    ni_debug_info_destroy(di);
    di = ni_debug_info_create();
    ni_debug_info_compiler_begin(di, "bob.c", "hello_bob", (char*)16, 1);
    ni_debug_info_compiler_at_line_number(di, (char*)16, 1); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)16, 3);
    ni_debug_info_compiler_at_line_number(di, (char*)18, 2); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)18, 4);
    ni_debug_info_compiler_at_line_number(di, (char*)20, 3); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)20, 3);
    ni_debug_info_compiler_end(di, (char*)22);
    ni_debug_info_destroy(di);    
    di = ni_debug_info_create();
    ni_debug_info_compiler_begin(di, "bob.c", "garry", (char*)160, 1);
    ni_debug_info_compiler_at_line_number(di, (char*)160, 1); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)160, 3);
    ni_debug_info_compiler_at_line_number(di, (char*)162, 2); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)162, 4);
    ni_debug_info_compiler_at_line_number(di, (char*)164, 3); 
    ni_debug_info_compiler_at_stack_depth(di, (char*)164, 3);
    ni_debug_info_compiler_end(di, (char*)166);
    ni_debug_info_destroy(di);
    return 0;
}