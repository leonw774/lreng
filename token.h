#include "operators.h"

#ifndef TOKEN_H
#define TOKEN_H

typedef struct {
    unsigned int line;
    unsigned int col;
} db_info;

typedef enum {
    TOK_ID, /*identifier */
    TOK_NUM, /* number */
    TOK_OP, /* operator */
    TOK_LB, /* left bracket */
    TOK_RB /* right bracket */
} token_enum;

typedef union {
    char* str;
    int op;
} token_payload;

typedef struct {
    token_payload payload;
    token_enum type;
    db_info db_info;
} token;

int
print_token(token t);

#endif
