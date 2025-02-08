#include <stdio.h>
#include <stdlib.h>
#include "reserved.h"
#include "token.h"

void
free_token_str(token_t* token) {
    if ((token->type == TOK_ID || token->type == TOK_NUM) && token->str) {
        free((char*) token->str);
        token->str = NULL;
    }
}

int
print_token(token_t token) {
    const char* token_str;
    const char* type_str;
    if (token.type == TOK_OP || token.type == TOK_LB || token.type == TOK_RB) {
        token_str = OP_STRS[token.name];
    }
    else {
        token_str = token.str;
    }
    switch (token.type) {
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
            printf("printf_token: bad token type: %d\n", token.type);
            type_str = "";
    }
    return (token.name == -1)
        ? printf("[%s \"%s\"]", type_str, token_str)
        : printf("[%s \"%s\" %d]", type_str, token_str, token.name);
}
