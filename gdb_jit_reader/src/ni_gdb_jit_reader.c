#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <gdb/jit-reader.h>

#include "../../c/debug_info/debug_info_data.h"


GDB_DECLARE_GPL_COMPATIBLE_READER

void fix_pointers(ni_symbol_header_t *header) {
    long addr_correction;
    ni_symbol_src_file_t *src_file;
    int i;
    addr_correction = (long)header - (long)header->original_address;
    header->src_file = (ni_symbol_src_file_t *)
                       ((char*)header->src_file + addr_correction);
    src_file = header->src_file;
    src_file->filename += addr_correction;
    src_file->line_mappings = (ni_symbol_line_mapping_t *)
                    ((char*)src_file->line_mappings + addr_correction);
    src_file->functions = (ni_symbol_function_t *)
                          ((char*)src_file->functions + addr_correction);
    for(i = 0; i < src_file->num_functions; i++) {
        src_file->functions[i].name += addr_correction;
        src_file->functions[i].stack_depths = 
            (ni_symbol_stack_depth_t *)
            ((char*)src_file->functions[i].stack_depths + 
            addr_correction);
    }
    header->original_address = (char*)header;
}

typedef struct node_s node_t;

struct node_s {
    ni_symbol_header_t *header;
    node_t *next;
};

static node_t *first_node = NULL;

static void reset_nodes(void) {
    node_t *node_next;
    node_t *node;
    node = first_node;
    
    while(node) {
        node_next = node->next;
        free(node->header);
        free(node);
        node = node_next;
    }
    first_node = NULL;
}

static void update_nodes(ni_symbol_header_t *header) {
    node_t *node;
    node = first_node;
    
    while(node) {
        if(strcmp(node->header->src_file->filename, 
                  header->src_file->filename) == 0) 
        {
            free(node->header);
            node->header = header;
            return;
        }
        node = node->next;
    }
    node = (node_t*)malloc(sizeof(node_t));
    node->header = header;
    node->next = first_node;
    first_node = node;
}

enum gdb_status ni_gdb_read_debug_info(struct gdb_reader_funcs *self,
                                       struct gdb_symbol_callbacks *cb,
                                       void *memory, long memory_sz) 
{    
    ni_symbol_header_t *header;
    ni_symbol_src_file_t *src_file;
    
    struct gdb_object * obj;
    struct gdb_symtab *symtab;
    struct gdb_line_mapping *lines;
    int i;
    
    header = (ni_symbol_header_t *)malloc(memory_sz);
    memcpy(header, memory, memory_sz);
    fix_pointers(header);
    
    if((header->command_flags & NI_SYMBOL_CMD_FLAG_RESET) == 
       NI_SYMBOL_CMD_FLAG_RESET) {
        reset_nodes();
    }
    update_nodes(header);
    
    src_file = header->src_file;
    
    obj = cb->object_open(cb);
    symtab = cb->symtab_open(cb, obj, src_file->filename);

    for(i = 0; i < src_file->num_functions; i++) {
        cb->block_open(cb, symtab, NULL, 
                       src_file->functions[i].start_address, 
                       src_file->functions[i].end_address, 
                       src_file->functions[i].name);
    }
    
    lines = (struct gdb_line_mapping *)calloc(
                    src_file->num_line_mappings, 
                    sizeof(struct gdb_line_mapping));
    for(i = 0; i < src_file->num_line_mappings; i++) {
        lines[i].line = src_file->line_mappings[i].line_num;
        lines[i].pc = src_file->line_mappings[i].address;
    }
    cb->line_mapping_add(cb, symtab, src_file->num_line_mappings, lines);
    free(lines);
    
    cb->symtab_close(cb, symtab);
    cb->object_close(cb, obj);
    return GDB_SUCCESS;
}


void ni_gdb_destroy_reader(struct gdb_reader_funcs *self) {
}

static int get_current_stack_depth(ni_symbol_function_t *func, 
                            unsigned long pc) 
{
    int i;
    int current_stack_depth = 0;
    for(i = 0; i < func->num_stack_depths; i++) {
        if(func->stack_depths[i].address > pc) {
            break;
        }
        current_stack_depth = func->stack_depths[i].stack_depth;
    }
    return current_stack_depth;
}

static int get_first_stack_depth(ni_symbol_function_t *func) {
    return func->stack_depths[0].stack_depth;
}

static ni_symbol_function_t * find_function_from_pc(unsigned long pc) 
{
    node_t *node;
    int i;
    node = first_node;
    ni_symbol_function_t *func;
    
    while(node) {
        for(i = 0; i < node->header->src_file->num_functions; i++) {
            func = node->header->src_file->functions + i;
            if(func->start_address <= pc && func->end_address >= pc) {
                return func;
            }
        }
        node = node->next;
    }
    return NULL;
}

static unsigned long get_reg_val(struct gdb_unwind_callbacks *cb, 
                                 int reg) 
{
    unsigned long reg_val;
    int i;
    struct gdb_reg_value *val = cb->reg_get(cb, reg);
    reg_val = 0;
    for(i = val->size - 1; i >=  0; i--) {
        reg_val <<= 8;
        reg_val += val->value[i];
    }
    val->free(val);
    return reg_val;
}

static unsigned long read_unsigned_long(struct gdb_unwind_callbacks *cb, 
                                        unsigned long address)
{
    unsigned long value;
    if(cb->target_read(address, &value, sizeof(unsigned long)) != GDB_SUCCESS) 
    {
        printf("read_unsigend_long: failed at address %lu", address); 
    }
    return value;
}


static void free_value(struct gdb_reg_value *value) {
    free(value);
}

static void set_reg_val(struct gdb_unwind_callbacks *cb, int reg, 
                 unsigned long reg_val) 
{
    size_t size = offsetof(struct gdb_reg_value, value) + 
                  sizeof(unsigned long);
    struct gdb_reg_value *val = (struct gdb_reg_value*)malloc(size);
    val->defined = 1;
    val->size = sizeof(unsigned long);
    val->free = free_value;
    memcpy(val->value, &reg_val, sizeof(unsigned long));
    cb->reg_set(cb, reg, val);
}
    

struct gdb_frame_id ni_gdb_get_frame_id(struct gdb_reader_funcs *self,
                                        struct gdb_unwind_callbacks *cb)
{
    struct gdb_frame_id id;
    unsigned long pc;
    unsigned long sp;
    int current_stack_depth;
    ni_symbol_function_t *func;

    pc = get_reg_val(cb, 8);
    sp = get_reg_val(cb, 4);
    
    func = find_function_from_pc(pc);
    current_stack_depth = get_current_stack_depth(func, pc);
    
    id.code_address = func->start_address;
    id.stack_address = sp + current_stack_depth;
    
    printf("ni_gdb_get_frame_id: id.code_address: %llu  "
           "id.stack_address: %llu in func: %s\n", id.code_address,
           id.stack_address, func->name);

    return id;
}

static unsigned long get_frame_pointer(struct gdb_unwind_callbacks *cb, 
                                unsigned long pc, unsigned long sp)
{
    int current_stack_depth;
    int first_stack_depth;
    ni_symbol_function_t *func;
    unsigned long frame_pointer;
    
    func = find_function_from_pc(pc);
    if(func == NULL) {
        /* if we are not in a psyco function we are in the glue code
         * we just return the EBP register, assuming psyco has
         * been built with EBP_IS_RESERVED = 1
         */
        frame_pointer = get_reg_val(cb, 5);
    }
    else {
        first_stack_depth = get_first_stack_depth(func);
        current_stack_depth = get_current_stack_depth(func, pc);
        frame_pointer = sp + (current_stack_depth - first_stack_depth);
    }
    printf("get_frame_pointer: fp: %lu func: %s\n", frame_pointer, 
           func ? func->name : "[glue code]");
    return frame_pointer;
}

enum gdb_status ni_gdb_unwind_frame(struct gdb_reader_funcs *self,
                                    struct gdb_unwind_callbacks *cb)
{
    unsigned long pc;
    unsigned long sp;
    unsigned long prev_pc;
    unsigned long prev_sp;
    unsigned long prev_fp;
    int current_stack_depth;
    int first_stack_depth;
    ni_symbol_function_t *func;

    pc = get_reg_val(cb, 8);
    sp = get_reg_val(cb, 4);
    
    func = find_function_from_pc(pc);
    
    if(func == NULL || func->num_stack_depths == 0) {
        return GDB_FAIL;
    }
    
    first_stack_depth = get_first_stack_depth(func);
    current_stack_depth = get_current_stack_depth(func, pc);

    prev_pc = read_unsigned_long(cb, sp + (current_stack_depth - 
                                 first_stack_depth));
    prev_sp = sp + (current_stack_depth - first_stack_depth) +
              sizeof(unsigned long);
    prev_fp = get_frame_pointer(cb, prev_pc, prev_sp);
    printf("ni_gdb_unwind_frame: pc: %lu sp: %lu first: %d current: "
           "%d\n", pc, sp, first_stack_depth, current_stack_depth);
    set_reg_val(cb, 8, prev_pc);
    set_reg_val(cb, 4, prev_sp);
    set_reg_val(cb, 5, prev_fp);
    printf("ni_gdb_unwind_frame: prev_pc: %lu prev_sp: %lu prev_fp: "
           "%lu\n", prev_pc, prev_sp, prev_fp);
    return GDB_SUCCESS;
}

static struct gdb_reader_funcs ni_gdb_reader_funcs;


struct gdb_reader_funcs *gdb_init_reader (void) {
    ni_gdb_reader_funcs.reader_version = GDB_READER_INTERFACE_VERSION;
    ni_gdb_reader_funcs.read = &ni_gdb_read_debug_info;
    ni_gdb_reader_funcs.unwind = &ni_gdb_unwind_frame;
    ni_gdb_reader_funcs.get_frame_id = &ni_gdb_get_frame_id;
    ni_gdb_reader_funcs.destroy = &ni_gdb_destroy_reader;
    return &ni_gdb_reader_funcs;
}