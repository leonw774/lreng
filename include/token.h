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


#define RESERVED_ID_NUM 3
extern const char* const RESERVED_IDS[RESERVED_ID_NUM];
typedef enum reserved_id_name {
    RESERVED_ID_NAME_NULL,
    RESERVED_ID_NAME_INPUT,
    RESERVED_ID_NAME_OUTPUT
} reserved_id_name_enum;


typedef struct token {
    const char* str;
    int name; /* the name of operator or id */
    token_type_enum type;
    linecol_t pos;
} token_t;

int
print_token(token_t t);

#endif
