#include "operators.h"

#ifndef TOKEN_H
#define TOKEN_H

typedef struct linecol {
    unsigned int line;
    unsigned int col;
} linecol_t;

typedef enum token_type {
    TOK_ID, /*identifier */
    TOK_NUM, /* number */
    TOK_OP, /* operator */
    TOK_LB, /* left bracket */
    TOK_RB /* right bracket */
} token_type_enum;

#define MAX_TOKEN_STR_LEN 128

typedef struct token {
    const char* str;
    int name; /* the name of operator or id */
    token_type_enum type;
    linecol_t pos;
} token_t;

extern int token_print(const token_t* token);

#endif
