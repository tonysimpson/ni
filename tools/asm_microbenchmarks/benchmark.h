#include "../../c/x64/iencoding.h"
#include <stdio.h>
#include <sys/mman.h>

#define elapsed_seconds(start, end) ((double)(end.tv_sec - start.tv_sec) + ((double)(end.tv_nsec - start.tv_nsec) / 1000000000.0))

#define START\
int main(int argc, char argv[]) {


#define BEGIN\
    int main(int argc, char argv[]) {\
        code_t *code;\
        struct timespec start_t, end_t;\
        void *codeb = (code_t*)mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);\
        if (codeb == MAP_FAILED) {\
            perror("mmap failed");\
            exit(1);\
        }\
        code = (code_t *)codeb;


#define BEGIN_LOOP\
        MOV_R_I(REG_X64_RSI, 0);\
        BEGIN_REVERSE_SHORT_JUMP(999);


#define END_LOOP\
        ADD_R_I8(REG_X64_RSI, 1);\
        CMP_R_R(REG_X64_RDI, REG_X64_RSI);\
        END_REVERSE_SHORT_COND_JUMP(999, CC_NE);


#define END\
        RET();\
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_t);\
        ((void (*) (long)) codeb)(0x1FFFFFFF);\
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_t);\
        printf("Time %s: %f\n", __FILE__, elapsed_seconds(start_t, end_t));\
        munmap(codeb, 4096);\
        return 0;\
    }

