#include "token.h"
#include <stdio.h>

int
print_token(token_t t) {
    const char* token_str;
    const char* type_str;
    int op_enum_int = -1;
    if (t.type == TOK_OP || t.type == TOK_LB || t.type == TOK_RB) {
        token_str = OP_STRS[t.payload.op];
        op_enum_int = t.payload.op;
    }
    else {
        token_str = t.payload.str;
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
    return (op_enum_int == -1)
        ? printf("(%s, \"%s\")", type_str, token_str)
        : printf("(%s, \"%s\", %d)", type_str, token_str, op_enum_int);
}
