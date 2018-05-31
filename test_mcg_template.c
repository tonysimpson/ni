#include "c/x64/iencoding.h"

int main(int argc, char argv[]) {
    code_t codeb[300];
    code_t *code = codeb;
    REPLACE_ME;
    for(code_t *ci = codeb; ci < code; ci++) {
        printf("%02x ", *ci);
    }
    printf("\n");
    return 0;
}
