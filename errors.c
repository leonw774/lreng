#include "errors.h"

char SYNTAX_ERR_MSG[256];

void
throw_syntax_error(int line, int col, const char* msg) {
    if (line == 0 && col == 0) {
        fprintf(stderr, "[SyntaxError]: %s\n", msg);
    }
    else {
        fprintf(stderr, "[SyntaxError]: Line %d col %d: %s\n", line, col, msg);
    }
    exit(2);
}
