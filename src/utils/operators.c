#include "stdlib.h"
#include "string.h"
#include "operators.h"

const char* const OP_STRS[] = {
    "{", "}", "@", "[", "]", "@!",
    "(", ")", "()",
    "$>", "$|", "$/",
    "+", "-", "!", ">>", "<<", "`", "~",
    "^",
    "*", "/", "%",
    "+", "-",
    "<", "<=", ">", ">=",
    "==", "!=",
    "&",
    "|",
    ",",
    "$",
    "=>",
    "&&",
    "||",
    "=", "?",
    ";"
};

/* if the left and right chars make up to an operator */
unsigned char
is_2char_op(char left, char right) {
    if (right == '=') {
        return left == '<' || left == '=' || left == '>' || left == '!';
    }
    if (left == '$') {
        return right == '>' || right == '|' || right == '/';
    }
    return (left == '=' && right == '>')
        || (left == '&' && right == '&')
        || (left == '|' && right == '|')
        || (left == '<' && right == '<')
        || (left == '>' && right == '>');
}

const char OP_CHARS[] = "!$%&()*+,-./;<=>?[]^`{|}~";

#define MAX_OPS_IN_TIER 8
const int OP_TIER_LIST[][MAX_OPS_IN_TIER] = {
    {OP_LCURLY, OP_RCURLY, OP_FMAKE, OP_LSQUARE, OP_RSQUARE, OP_MMAKE, -1},
    {OP_LPAREN, OP_RPAREN, OP_FCALL, -1},
    {OP_MAP, OP_FILTER, OP_REDUCE, -1},
    {OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR, -1},
    {OP_EXP, -1},
    {OP_MUL, OP_DIV, OP_MOD, -1},
    {OP_ADD, OP_SUB, -1},
    {OP_LT, OP_LE, OP_GT, OP_GE, -1},
    {OP_EQ, OP_NE, -1},
    {OP_AND, -1},
    {OP_OR, -1},
    {OP_PAIR, -1},
    {OP_FCALLR, -1},
    {OP_ARG, -1},
    {OP_CONDAND, -1},
    {OP_CONDOR, -1},
    {OP_ASSIGN, OP_CONDFCALL, -1},
    {OP_EXPRSEP, -1}
};
#define TIER_COUNT sizeof(OP_TIER_LIST) / MAX_OPS_IN_TIER / sizeof(int)

int
get_op_precedence(op_name_enum op) {
    static int PRECEDENCES_ARRAY[OPERATOR_COUNT];
    static unsigned char is_initialized = 0;
    unsigned int i;
    /* initialize array if not */
    if (!is_initialized) {
        for (i = 0; i < OPERATOR_COUNT; i++) {
            PRECEDENCES_ARRAY[i] = 0;
        }
        for (i = 0; i < TIER_COUNT; i++) {
            int j = 0;
            while (OP_TIER_LIST[i][j] != -1) {
                PRECEDENCES_ARRAY[OP_TIER_LIST[i][j]] = i;
                j++;
            }
        }
        is_initialized = 1;
    }
    return PRECEDENCES_ARRAY[op];
};

const int UNARY_OPS[] = {
    OP_FMAKE, OP_MMAKE,
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR
};

unsigned char
is_unary_op(op_name_enum op) {
    static unsigned char IS_UNARY_OP_ARRAY[OPERATOR_COUNT];
    static unsigned char is_initialized = 0;
    unsigned int i;
    /* initialize array if not */
    if (!is_initialized) {
        for (i = 0; i < OPERATOR_COUNT; i++) {
            IS_UNARY_OP_ARRAY[i] = 0;
        }
        for (i = 0; i < sizeof(UNARY_OPS) / sizeof(int); i++) {
            IS_UNARY_OP_ARRAY[UNARY_OPS[i]] = 1;
        }
        is_initialized = 1;
    }
    return IS_UNARY_OP_ARRAY[op];
};

const int PREFIXABLE_OPS[] = {
    OP_LCURLY, OP_LSQUARE, OP_LPAREN,
    OP_FMAKE, OP_MMAKE,
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR
};

unsigned char
is_prefixable_op(op_name_enum op) {
    unsigned int i;
    for (i = 0; i < sizeof(PREFIXABLE_OPS) / sizeof(int); i++) {
        if (PREFIXABLE_OPS[i] == (int) op) {
            return 1;
        }
    }
    return 0;
};

const int R_ASSO_OPS[] = {
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR,
    OP_EXP, OP_PAIR, OP_FCALLR, OP_ARG, OP_CONDFCALL, OP_ASSIGN
};

unsigned char
is_r_asso_op(op_name_enum op) {
    unsigned int i;
    for (i = 0; i < sizeof(R_ASSO_OPS) / sizeof(int); i++) {
        if (R_ASSO_OPS[i] == (int) op) {
            return 1;
        }
    }
    return 0;
};
