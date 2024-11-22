#include <stdio.h>
#include <stdlib.h>
#include "errormsg.h"

/* use this buffer to store error message */
char ERR_MSG_BUF[256];

inline void
throw_syntax_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stdout, "[SyntaxError]: %s\n", msg);
    }
    else {
        fprintf(stdout, "[SyntaxError]: Line %d col %d: %s\n", line, col, msg);
    }
    exit(SYNTAX_ERR_CODE);
}

/* set line col to zero if not availible */
inline void
print_semantic_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stdout, "[SyntaxError]: %s\n", msg);
    }
    else {
        fprintf(stdout, "[SyntaxError]: Line %d col %d: %s\n", line, col, msg);
    }
}

/* set line col to zero if not availible */
inline void
print_runtime_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stdout, "[RuntimError]: %s\n", msg);
    }
    else {
        fprintf(stdout, "[RuntimeError]: Line %d col %d: %s\n", line, col, msg);
    }
}
