#include "operators.h"
#include "stdlib.h"
#include "string.h"

#define MAX_OPS_IN_TIER 8
const int OP_TIER_LIST[][MAX_OPS_IN_TIER] = {
    /* ********** brackets & callable maker ********** */
    { OP_LCURLY, OP_RCURLY, OP_FMAKE, OP_LSQUARE, OP_RSQUARE, OP_MMAKE, -1 },
    { OP_LPAREN, OP_RPAREN, OP_FCALL, -1 },
    /* ********** map filter reduce ********** */
    { OP_MAP, OP_FILTER, OP_REDUCE, -1 },
    /* ********** unary operators ********** */
    { OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR, -1 },
    /* ********** arithmetic binary operators ********** */
    { OP_EXP, -1 },
    { OP_MUL, OP_DIV, OP_MOD, -1 },
    { OP_ADD, OP_SUB, -1 },
    /* ********** comparison operators ********** */
    { OP_LT, OP_LE, OP_GT, OP_GE, -1 },
    { OP_EQ, OP_NE, -1 },
    /* ********** logical operators ********** */
    { OP_AND, -1 },
    { OP_OR, -1 },
    /* ********** pair and callables ********** */
    { OP_PAIR, -1 },
    { OP_FCALLR, -1 },
    { OP_ARG, -1 },
    /* ********** conditional operators and assignment ********** */
    { OP_CONDAND, -1 },
    { OP_CONDOR, -1 },
    { OP_ASSIGN, OP_CONDFCALL, -1 },
    /* expression separator */
    { OP_EXPRSEP, -1 }
};
#define TIER_COUNT sizeof(OP_TIER_LIST) / MAX_OPS_IN_TIER / sizeof(int)

const char* const OP_STRS[] = {
    /* ********** brackets ********** */
    "{", "}", "make_function", "[", "]", "make_macro", "(", ")", "call",
    /* ********** map filter reduce ********** */
    "$>", "$|", "$/",
    /* ********** unary operators ********** */
    "+", "-", "!", ">>", "<<", "`", "~",
    /* ********** arithmetic binary operators ********** */
    "^", "*", "/", "%", "+", "-",
    /* ********** comparison operators ********** */
    "<", "<=", ">", ">=", "==", "!=",
    /* ********** logical operators ********** */
    "&", "|",
    /* ********** pair and callables ********** */
    ",", "$", "=>",
    /* ********** conditional operators and assignment ********** */
    "&&", "||", "=", "?",
    /* expression separator */
    ";"
};

/* if the left and right chars make up to an operator */
unsigned char
is_2char_op(char left, char right)
{
    if (right == '=') {
        return left == '<' || left == '=' || left == '>' || left == '!';
    }
    if (left == '$') {
        return right == '>' || right == '|' || right == '/';
    }
    return (left == '=' && right == '>') || (left == '&' && right == '&')
        || (left == '|' && right == '|') || (left == '<' && right == '<')
        || (left == '>' && right == '>');
}

const char OP_CHARS[] = "!$%&()*+,-./;<=>?[]^`{|}~";

int
get_op_precedence(op_name_enum op)
{
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
    /* callable makers are also unary operators */
    OP_FMAKE, OP_MMAKE,
    /* other unary operators */
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR
};

unsigned char
is_unary_op(op_name_enum op)
{
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
    /* ********** left brackets ********** */
    OP_LCURLY, OP_LSQUARE, OP_LPAREN,
    /* ********** unary operators ********** */
    OP_FMAKE, OP_MMAKE, OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL,
    OP_GETR
};

unsigned char
is_prefixable_op(op_name_enum op)
{
    unsigned int i;
    for (i = 0; i < sizeof(PREFIXABLE_OPS) / sizeof(int); i++) {
        if (PREFIXABLE_OPS[i] == (int)op) {
            return 1;
        }
    }
    return 0;
};

const int R_ASSO_OPS[] = {
    /* ********** unary operators ********** */
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR,
    /* ********** exponent, pair and callables, assignment ********** */
    OP_EXP, OP_PAIR, OP_FCALLR, OP_ARG, OP_CONDFCALL, OP_ASSIGN
};

unsigned char
is_r_asso_op(op_name_enum op)
{
    unsigned int i;
    for (i = 0; i < sizeof(R_ASSO_OPS) / sizeof(int); i++) {
        if (R_ASSO_OPS[i] == (int)op) {
            return 1;
        }
    }
    return 0;
};
