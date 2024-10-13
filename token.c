#include "token.h"
#include <stdio.h>

const char* const RESERVED_IDS[RESERVED_ID_NUM] = {
    "null", "input", "output"
};

int
print_token(token_t t) {
    const char* token_str;
    const char* type_str;
    if (t.type == TOK_OP || t.type == TOK_LB || t.type == TOK_RB) {
        token_str = OP_STRS[t.name];
    }
    else {
        token_str = t.str;
    }
    switch (t.type) {
        case TOK_ID:
            type_str = "ID";
            break;
        case TOK_NUM:
            type_str = "NUM";
            break;
        case TOK_OP:
            type_str = "OP";
            break;
        case TOK_LB:
            type_str = "LB";
            break;
        case TOK_RB:
            type_str = "RB";
            break;
        default:
            printf("printf_token: bad token type: %d\n", t.type);
            type_str = "";
            break;
    }
    return (t.name == -1)
        ? printf("[%s \"%s\"]", type_str, token_str, t.name)
        : printf("[%s \"%s\" %d]", type_str, token_str, t.name);
}
