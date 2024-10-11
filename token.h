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

typedef union token_payload {
    char* str;
    int op;
} token_payload_t;

typedef struct token {
    token_payload_t payload;
    token_type_enum type;
    linecol_t pos;
} token_t;

int
print_token(token_t t);

#endif
