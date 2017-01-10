#ifndef DEBUG_INFO_DATA_H
#define DEBUG_INFO_DATA_H


typedef struct {
    unsigned long address;
    int stack_depth;
} ni_symbol_stack_depth_t;


typedef struct {
    char *name;
    unsigned long start_address;
    unsigned long end_address;
    int inlined;
    long return_stack_pos;
    unsigned long from_address;
    int num_stack_depths;
    ni_symbol_stack_depth_t *stack_depths;
} ni_symbol_function_t;


typedef struct {
    unsigned long address;
    int line_num;
} ni_symbol_line_mapping_t;


typedef struct {
    char *filename;
    ni_symbol_function_t *functions;
    int num_functions;
    ni_symbol_line_mapping_t *line_mappings;
    int num_line_mappings;
} ni_symbol_src_file_t;

#define NI_SYMBOL_CMD_FLAG_RESET         0x1

typedef struct {
    char *original_address;
    int command_flags;
    ni_symbol_src_file_t * src_file;
} ni_symbol_header_t;

#endif