#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "debug_info.h"

/* 
 * 
 * There are 3 sections here:
 * 
 * 1) The interface as specified by gdb to allow us to inform it of 
 * where to find debug symbols for the JIT compiled code.
 * 
 * 2) Functions for packing symbol data so it can be sent to gdb
 * 
 * 3) Functions and structures unique to Ni for recording and sharing
 * debug information with gdb. These are called during compilation.
 * 
 */


struct ni_debug_info_t {
    char* filename;
    ni_symbol_function_t function;
    int last_line_num;
    long last_stack_depth;
    ni_symbol_line_mapping_t *line_mappings;
    int num_line_mappings;
};


void *mempcpy(void *_dest, const void *src, size_t size) {
    memcpy(_dest, src, size);
    return (void *)(((char *)_dest) + size);
}

char *strpcpy(char *_dest, const char *src) {
    size_t len = strlen(src) + 1;
    return mempcpy(_dest, src, len);
}

/**** Start GDB JIT Hook Specific Bits ****/

typedef enum
{
  JIT_NOACTION = 0,
  JIT_REGISTER_FN,
  JIT_UNREGISTER_FN
} jit_actions_t;

struct jit_code_entry
{
  struct jit_code_entry *next_entry;
  struct jit_code_entry *prev_entry;
  const char *symfile_addr;
  uint64_t symfile_size;
};

struct jit_descriptor
{
  uint32_t version;
  /* This type should be jit_actions_t, but we use uint32_t
     to be explicit about the bitwidth.  */
  uint32_t action_flag;
  struct jit_code_entry *relevant_entry;
  struct jit_code_entry *first_entry;
};

void __attribute__((noinline)) __jit_debug_register_code() { };


struct jit_descriptor __jit_debug_descriptor = { 1, 0, 0, 0 };



/**** End GDB JIT Hook Specific Bits ****/



static void pack_into_entry(struct jit_code_entry *entry, 
                            struct ni_debug_info_t *di) 
{
    static int firstTime = 1;
    ni_symbol_header_t *header;
    ni_symbol_src_file_t *src_file;
    ni_symbol_function_t *function;
    char *data;
    char *cur_pos;
    size_t size = 0;
    
    /* calc required size */
    size += sizeof(ni_symbol_header_t);
    size += sizeof(ni_symbol_src_file_t);
    size += sizeof(ni_symbol_line_mapping_t) * di->num_line_mappings;
    size += sizeof(ni_symbol_function_t);
    size += sizeof(ni_symbol_stack_depth_t) * 
            di->function.num_stack_depths;
    size += strlen(di->filename) + 1;
    size += strlen(di->function.name) + 1;
    
    /* pack data */
    cur_pos = data = malloc(size);
    
    header = (ni_symbol_header_t*)cur_pos;
    header->command_flags = 0;
    if(firstTime) { 
        header->command_flags = NI_SYMBOL_CMD_FLAG_RESET;
        firstTime = 0;
    }
    header->original_address = cur_pos;
    cur_pos += sizeof(ni_symbol_header_t);
    
    
    src_file = (ni_symbol_src_file_t *)cur_pos;
    header->src_file = src_file;
    cur_pos += sizeof(ni_symbol_src_file_t);
    
    /* line_mappings */
    if(di->num_line_mappings > 0) {
        src_file->num_line_mappings = di->num_line_mappings;
        src_file->line_mappings = (ni_symbol_line_mapping_t *)cur_pos;
        cur_pos = mempcpy(cur_pos, di->line_mappings, 
                          sizeof(ni_symbol_line_mapping_t) * 
                          di->num_line_mappings);
    }
    
    /* function */
    src_file->num_functions = 1;
    function = (ni_symbol_function_t *)cur_pos;
    src_file->functions = function;
    cur_pos = mempcpy(cur_pos, &di->function, 
                     sizeof(ni_symbol_function_t));
    
    /* function stack_depths */
    if(di->function.num_stack_depths > 0) {
        src_file->functions[0].stack_depths = 
                                     (ni_symbol_stack_depth_t *)cur_pos;
        cur_pos = mempcpy(cur_pos, di->function.stack_depths, 
                          sizeof(ni_symbol_stack_depth_t) * 
                          di->function.num_stack_depths);
    }
    
    /* pack strings */
    src_file->filename = cur_pos;
    strcpy(cur_pos, di->filename);
    cur_pos += strlen(di->filename) +1;
    
    function->name = cur_pos;
    strcpy(cur_pos, di->function.name);
    cur_pos += strlen(function->name) +1;
    /* set entry fields */
    entry->symfile_addr = data;
    entry->symfile_size = size;
}


static int line_mapping_cmp(const void *s1, const void *s2) 
{
    unsigned long s1_address;
    unsigned long s2_address;
    s1_address = ((ni_symbol_line_mapping_t *)s1)->address;
    s2_address = ((ni_symbol_line_mapping_t *)s2)->address;
    if(s1_address < s2_address) return -1;
    if(s1_address > s2_address) return 1;
    else return 0;
}

/* merges both sets of line mappings and returns the new total number */
static int merge_line_mappings(ni_symbol_src_file_t *old, 
                        struct ni_debug_info_t *di, 
                        ni_symbol_line_mapping_t *target) 
{
    ni_symbol_line_mapping_t *temp;
    int i;
    int j;
    int total_num = old->num_line_mappings + di->num_line_mappings;
    
    temp = mempcpy(target, old->line_mappings, 
                  sizeof(ni_symbol_line_mapping_t) * 
                  old->num_line_mappings);
    memcpy(temp, di->line_mappings, sizeof(ni_symbol_line_mapping_t) * 
                  di->num_line_mappings);
    qsort(target, total_num, sizeof(ni_symbol_line_mapping_t), 
          line_mapping_cmp);
    for(j = 0, i = 1; i < total_num; i++) {
        if(target[j].address == target[i].address) {
            continue;
        }
        else {
            j++;
            target[j].address = target[i].address;
            target[j].line_num = target[i].line_num;
        }
    }
    total_num = j + 1;
    return total_num;
}


#define OVERLAP_MARGIN  16

#define MIN(A, B)    A < B ? A : B
#define MAX(A, B)    A > B ? A : B

static int functions_overlap(ni_symbol_function_t *f1, 
                             ni_symbol_function_t *f2)
{
    int result  = 0;
    result = f1->end_address < (f2->start_address - OVERLAP_MARGIN);
    result = result || f1->start_address > (f2->end_address + OVERLAP_MARGIN);
    result = ! result;
    result = result && strcmp(f1->name, f2->name) == 0;
    result = result && (f1->inlined == f2->inlined);
    return result;
}

static int stack_depth_cmp(const void *s1, const void *s2) 
{
    unsigned long s1_address;
    unsigned long s2_address;
    s1_address = ((ni_symbol_stack_depth_t *)s1)->address;
    s2_address = ((ni_symbol_stack_depth_t *)s2)->address;
    if(s1_address < s2_address) return -1;
    if(s1_address > s2_address) return 1;
    else return 0;
}

static int merge_function(ni_symbol_src_file_t *old, 
                          struct ni_debug_info_t *di, 
                          ni_symbol_function_t *func_target,
                          ni_symbol_stack_depth_t *stack_depth_target)
{
    int i, j, k;
    ni_symbol_stack_depth_t *stack_depth_target_pos;
    ni_symbol_function_t *function_target_pos;
    ni_symbol_function_t *function_temp;
    int num_function;
    num_function = 0;
    /* place di->function in the list in order */
    i = 0;
    j = 0;
    stack_depth_target_pos = stack_depth_target;
    function_target_pos = func_target;
    /* copy non overlaping before the new function */
    for(; i < old->num_functions; i++) {
        if(di->function.end_address >=  old->functions[i].start_address - OVERLAP_MARGIN) {
            break;
        }
        else {
            function_temp = old->functions + i;
            memcpy(function_target_pos, function_temp, 
                   sizeof(ni_symbol_function_t));
            if(function_temp->num_stack_depths > 0) {
                function_target_pos->stack_depths = 
                                                 stack_depth_target_pos;
                memcpy(stack_depth_target_pos, 
                       function_temp->stack_depths, 
                       sizeof(ni_symbol_stack_depth_t) * 
                       function_temp->num_stack_depths);
                stack_depth_target_pos += function_temp->num_stack_depths;
            }
            function_target_pos++;
            num_function++;
        }
    }
    
    /* merge overlapping */
    memcpy(function_target_pos, &di->function, 
           sizeof(ni_symbol_function_t));
    function_target_pos->stack_depths = stack_depth_target_pos;        
    if(di->function.num_stack_depths > 0) {
        memcpy(stack_depth_target_pos, 
                       di->function.stack_depths, 
                       sizeof(ni_symbol_stack_depth_t) * 
                       di->function.num_stack_depths);
        stack_depth_target_pos += di->function.num_stack_depths;
    }
    for(; i < old->num_functions; i++) {
        if(!functions_overlap(function_target_pos, old->functions + i)) 
        {
            break;
        }
        function_target_pos->start_address = MIN(
                                     function_target_pos->start_address, 
                                     old->functions[i].start_address);
        function_target_pos->end_address = MAX(
                                     function_target_pos->end_address, 
                                     old->functions[i].end_address);
        function_target_pos->num_stack_depths += 
                                     old->functions[i].num_stack_depths;
        if(old->functions[i].num_stack_depths > 0) {
            memcpy(stack_depth_target_pos,
                       old->functions[i].stack_depths, 
                       sizeof(ni_symbol_stack_depth_t) * 
                       old->functions[i].num_stack_depths);
            stack_depth_target_pos += 
                                     old->functions[i].num_stack_depths;
        }
    }
    /* sort and de-dup stack_depths */
    qsort(function_target_pos->stack_depths, 
          function_target_pos->num_stack_depths, 
          sizeof(ni_symbol_stack_depth_t), 
          stack_depth_cmp);
    stack_depth_target_pos = function_target_pos->stack_depths;
    for(j = 1, k = 0 ; j < function_target_pos->num_stack_depths; j++) {
        if(stack_depth_target_pos[k].address == 
           stack_depth_target_pos[j].address) {
            continue;
        }
        else {
            k++;
            stack_depth_target_pos[k].address = 
                                      stack_depth_target_pos[j].address;
            stack_depth_target_pos[k].stack_depth = 
                                  stack_depth_target_pos[j].stack_depth;
        }
    }
    stack_depth_target_pos += k;
    function_target_pos->num_stack_depths = k + 1;
    num_function++;
    function_target_pos++;
    
    /* copy any remaining none overlapping */
    for(; i < old->num_functions; i++) {    
        function_temp = old->functions + i;
        memcpy(function_target_pos, function_temp, 
               sizeof(ni_symbol_function_t));
        if(function_temp->num_stack_depths > 0) {
            function_target_pos->stack_depths = 
                                             stack_depth_target_pos;
            memcpy(stack_depth_target_pos, 
                   function_temp->stack_depths, 
                   sizeof(ni_symbol_stack_depth_t) * 
                   function_temp->num_stack_depths);
            stack_depth_target_pos += function_temp->num_stack_depths;
        }
        function_target_pos++;
        num_function++;
    }
    return num_function;
}

static void update_entry(struct jit_code_entry *entry, 
                  struct ni_debug_info_t *di)
{
    ni_symbol_header_t * old_header;
    ni_symbol_header_t * new_header;
    ni_symbol_src_file_t * new_src_file;
    ni_symbol_line_mapping_t *line_mappings;
    ni_symbol_function_t *functions;
    ni_symbol_stack_depth_t *stack_depths;
    int num_line_mappings;
    int num_functions;
    
    char *data;
    char *cur_pos;
    char *temp;
    size_t size;
    int i;
    size = entry->symfile_size;
    old_header = (ni_symbol_header_t *)entry->symfile_addr;
    /* 
     * to keep things simple we allocate a new size based on 
     * nothings overlapping or being merged 
     */
    size += sizeof(ni_symbol_line_mapping_t) * di->num_line_mappings;
    size += sizeof(ni_symbol_function_t);
    size += strlen(di->function.name) + 1;
    size += sizeof(ni_symbol_stack_depth_t) * 
            di->function.num_stack_depths;

    cur_pos = data = (char*)malloc(size);

    new_header = (ni_symbol_header_t*)cur_pos;
    new_header->command_flags = 0;
    new_header->original_address = cur_pos;
    cur_pos += sizeof(ni_symbol_header_t);
    new_src_file = (ni_symbol_src_file_t *)cur_pos;
    new_header->src_file = new_src_file;
    new_src_file->filename = old_header->src_file->filename;
    cur_pos += sizeof(ni_symbol_src_file_t);
    line_mappings = (ni_symbol_line_mapping_t*)cur_pos;
    num_line_mappings = merge_line_mappings(old_header->src_file, di, 
                                            line_mappings);
    new_src_file->line_mappings = line_mappings;
    new_src_file->num_line_mappings = num_line_mappings;
    cur_pos += num_line_mappings * sizeof(ni_symbol_line_mapping_t);
    functions = (ni_symbol_function_t *)cur_pos;
    stack_depths = (ni_symbol_stack_depth_t *)(cur_pos + 
                            ((old_header->src_file->num_functions + 1) * 
                            sizeof(ni_symbol_function_t)));
    num_functions = merge_function(old_header->src_file, di, functions, 
                                   stack_depths);
    new_src_file->num_functions = num_functions;
    new_src_file->functions = functions;
    cur_pos = (char*)stack_depths;
    for(i = 0; i < new_src_file->num_functions; i++) {
        cur_pos += sizeof(ni_symbol_stack_depth_t) * 
                   new_src_file->functions[i].num_stack_depths;
    }
    temp = cur_pos;
    cur_pos = strpcpy(cur_pos, new_src_file->filename);
    new_src_file->filename = temp;
    for(i = 0; i < new_src_file->num_functions; i++) {
        temp = cur_pos;
        cur_pos = strpcpy(cur_pos, new_src_file->functions[i].name);
        new_src_file->functions[i].name = temp;
    }
    free(old_header);
    entry->symfile_addr = data;
    entry->symfile_size = size;
}

static void ni_debug_info_gdb_update(struct ni_debug_info_t *di) {
    struct jit_code_entry *entry;
    for(entry = __jit_debug_descriptor.first_entry; entry != NULL;
        entry = entry->next_entry) {
        ni_symbol_header_t *header = 
            (ni_symbol_header_t *)entry->symfile_addr;
        if(strcmp(header->src_file->filename, di->filename) == 0) {
            __jit_debug_descriptor.relevant_entry = entry;
            __jit_debug_descriptor.action_flag = JIT_UNREGISTER_FN;
            __jit_debug_register_code();
            update_entry(entry, di);
            __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
            __jit_debug_register_code();
            return;
        }
    }
    entry = malloc(sizeof(struct jit_code_entry));
    pack_into_entry(entry, di);
    entry->next_entry = __jit_debug_descriptor.first_entry;
    if(entry->next_entry != NULL) {
        entry->next_entry->prev_entry = entry;
    }
    __jit_debug_descriptor.first_entry = entry;
    __jit_debug_descriptor.relevant_entry = entry;
    __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
    __jit_debug_register_code();
}

/*************************************/
/**** Collect and Pack Debug Info ****/
/*************************************/




struct ni_debug_info_t * ni_debug_info_create() 
{
    struct ni_debug_info_t * new = malloc(sizeof(
                                               struct ni_debug_info_t));
    new->last_line_num = -1;
    new->last_stack_depth = 0;
    new->line_mappings = NULL;
    new->num_line_mappings = 0;
    new->function.num_stack_depths = 0;
    new->function.stack_depths = NULL;
    return new;
}


void ni_debug_info_destroy(struct ni_debug_info_t *di) 
{
    if(di->num_line_mappings > 0)
        free(di->line_mappings);
    if(di->function.num_stack_depths > 0)
        free(di->function.stack_depths);
    free(di);
}


void ni_debug_info_compiler_begin(struct ni_debug_info_t *di,
                                  char *filename,
                                  char *func_name,
                                  char *start_address,
                                  int inlined,
                                  long return_stack_pos, 
                                  char *from_address) 
{
    di->filename = filename;
    di->function.name = func_name;
    di->function.start_address = (unsigned long)start_address;
    di->function.return_stack_pos = return_stack_pos;
    di->function.from_address = (unsigned long)from_address;
    di->function.inlined = inlined;
}

void ni_debug_info_compiler_begin_inline(struct ni_debug_info_t *di,
                                  char *filename,
                                  char *func_name,
                                  char *start_address,
                                  char *from_address) 
{
    ni_debug_info_compiler_begin(di, filename, func_name, start_address, 
                                 1, 0, from_address);
}


void ni_debug_info_compiler_begin_normal(struct ni_debug_info_t *di,
                                  char *filename,
                                  char *func_name,
                                  char *start_address,
                                  long return_stack_pos) 
{
    ni_debug_info_compiler_begin(di, filename, func_name, start_address, 
                                 0, return_stack_pos, 0);
}

void ni_debug_info_compiler_end(struct ni_debug_info_t *di, 
                                char *end_address) 
{
    di->function.end_address = (unsigned long)end_address;
    ni_debug_info_gdb_update(di);
}


void ni_debug_info_compiler_at_line_number(struct ni_debug_info_t *di, 
                                           char *address, 
                                           int line_num)
{
    ni_symbol_line_mapping_t *line_mapping;
    if(line_num == di->last_line_num) 
        return;
    if(di->line_mappings == NULL) 
        di->line_mappings = malloc(sizeof(ni_symbol_line_mapping_t));
    else
        di->line_mappings = realloc(di->line_mappings, 
                                   sizeof(ni_symbol_line_mapping_t) * 
                                   (di->num_line_mappings + 1));
    line_mapping = di->line_mappings + di->num_line_mappings;
    line_mapping->address = (unsigned long)address;
    line_mapping->line_num = line_num;
    di->num_line_mappings += 1;
    di->last_line_num = line_num;
}


void ni_debug_info_compiler_at_stack_depth(struct ni_debug_info_t *di, 
                                           char *address, 
                                           int stack_depth)
{
    ni_symbol_stack_depth_t *target;
    if(stack_depth == di->last_stack_depth) {
        return;
    }
    if(di->function.num_stack_depths == 0) {
        di->function.stack_depths = malloc(
                                       sizeof(ni_symbol_stack_depth_t));
    }
    else {
        di->function.stack_depths = realloc(di->function.stack_depths, 
                                      sizeof(ni_symbol_stack_depth_t) * 
                                   (di->function.num_stack_depths + 1));
    }
    target = di->function.stack_depths + di->function.num_stack_depths;
    target->address = (unsigned long)address;
    target->stack_depth = stack_depth;
    di->function.num_stack_depths += 1;
    di->last_stack_depth = stack_depth;
}

ni_symbol_function_t * ni_symbol_function_at(unsigned long pc) {
    ni_symbol_header_t *header;
    int i;
    struct jit_code_entry *entry = __jit_debug_descriptor.first_entry;
    while(entry) {
        header = (ni_symbol_header_t *)entry->symfile_addr;
        for(i = 0; i < header->src_file->num_functions; i++) {
            if(header->src_file->functions[i].start_address <= pc &&
               header->src_file->functions[i].end_address >= pc) 
            {
                return header->src_file->functions + i;
            }
        }
        entry = entry->next_entry;
    }
    return NULL;
}

ni_symbol_src_file_t * ni_symbol_src_file_at(unsigned long pc) {
    ni_symbol_header_t *header;
    int i;
    struct jit_code_entry *entry = __jit_debug_descriptor.first_entry;
    while(entry) {
        header = (ni_symbol_header_t *)entry->symfile_addr;
        for(i = 0; i < header->src_file->num_functions; i++) {
            if(header->src_file->functions[i].start_address <= pc &&
               header->src_file->functions[i].end_address >= pc) 
            {
                return header->src_file;
            }
        }
        entry = entry->next_entry;
    }
    return NULL;
}

void ni_symbol_dump(void) {
    FILE *file = fopen("ni_symbol.dump", "w");
    struct jit_code_entry *entry = __jit_debug_descriptor.first_entry;
    while(entry) {
        fwrite(&entry->symfile_size, sizeof(uint64_t), 1, file);
        fwrite(entry->symfile_addr, entry->symfile_size, 1, file);
        entry = entry->next_entry;
    }
    fclose(file);
}
    

