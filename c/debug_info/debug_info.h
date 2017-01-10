#ifndef DEBUG_INFO_H
#define DEBUG_INFO_H

#include "debug_info_data.h"

struct ni_debug_info_t;

struct ni_debug_info_t * ni_debug_info_create(void);
void ni_debug_info_destroy(struct ni_debug_info_t *di);
void ni_debug_info_compiler_begin_inline(struct ni_debug_info_t *di,
                                  char *filename,
                                  char *func_name,
                                  char *start_address,
                                  char *from_address);
void ni_debug_info_compiler_begin_normal(struct ni_debug_info_t *di,
                                  char *filename,
                                  char *func_name,
                                  char *start_address,
                                  long return_stack_pos);                                  
void ni_debug_info_compiler_at_line_number(struct ni_debug_info_t *di, 
                                           char *address, int line_num);
void ni_debug_info_compiler_at_stack_depth(struct ni_debug_info_t *di, 
                                           char *address, 
                                           int stack_depth);                                           
void ni_debug_info_compiler_end(struct ni_debug_info_t *di, 
                                char *end_address);
#endif