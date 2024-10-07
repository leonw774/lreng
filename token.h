#include "operators.h"

#ifndef TOKEN_H
#define TOKEN_H

typedef struct db_info {
    int line;
    int col;
} db_info;

typedef enum TOKEN_TYPE {
    TOK_ID, /*identifier */
    TOK_NUM, /* number */
    TOK_OP, /* operator */
    TOK_LB, /* left bracket */
    TOK_RB /* right bracket */
} TOKEN_TYPE;

typedef union token_payload {
    char* str;
    int op_name;
} token_payload;

typedef struct token {
    token_payload payload;
    enum TOKEN_TYPE type;
    db_info db_info;
} token;

#endif