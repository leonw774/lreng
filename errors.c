#include "errors.h"

void
throw_syntax_error(int line, int col, const char* msg) {
    fprintf(
        stderr,
        "[SyntaxError]: Line %d col %d: %s\n",
        line, col, msg
    );
    exit(2);
}