#include <stdio.h>
#include <stdlib.h>
#include "errormsg.h"

/* use this buffer to store error message */
char ERR_MSG_BUF[256];

inline void
throw_syntax_error(linecol_t pos, const char* msg) {
    if (pos.line == 0 && pos.col == 0) {
        printf("[SyntaxError]: %s\n", msg);
    }
    else {
        printf("[SyntaxError]: Line %d col %d: %s\n", pos.line, pos.col, msg);
    }
    exit(SYNTAX_ERR_CODE);
}

/* set line col to zero if not availible */
inline void
print_semantic_error(linecol_t pos, const char* msg) {
    if (pos.line == 0 && pos.col == 0) {
        printf("[SyntaxError]: %s\n", msg);
    }
    else {
        printf("[SyntaxError]: Line %d col %d: %s\n", pos.line, pos.col, msg);
    }
}

/* set line col to zero if not availible */
inline void
print_runtime_error(linecol_t pos, const char* msg) {
    if (pos.line == 0 && pos.col == 0) {
        printf("[RuntimError]: %s\n", msg);
    }
    else {
        printf("[RuntimeError]: Line %d col %d: %s\n", pos.line, pos.col, msg);
    }
}
