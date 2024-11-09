#include <stdio.h>
#include <stdlib.h>
#include "errormsg.h"

/* use this buffer to store error message */
char ERR_MSG_BUF[256];

void
throw_syntax_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stderr, "[SyntaxError]: %s\n", msg);
    }
    else {
        fprintf(stderr, "[SyntaxError]: Line %d col %d: %s\n", line, col, msg);
    }
    exit(SYNTAX_ERR_CODE);
}

void
print_semantic_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stderr, "[SyntaxError]: %s\n", msg);
    }
    else {
        fprintf(stderr, "[SyntaxError]: Line %d col %d: %s\n", line, col, msg);
    }
}

void
throw_runtime_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stderr, "[RuntimError]: %s\n", msg);
    }
    else {
        fprintf(stderr, "[RuntimeError]: Line %d col %d: %s\n", line, col, msg);
    }
    exit(RUNTIME_ERR_CODE);
}