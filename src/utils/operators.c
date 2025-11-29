#include "operators.h"
#include "stdlib.h"
#include "string.h"

#define MAX_OPS_IN_TIER 10
const int OP_TIER_LIST[][MAX_OPS_IN_TIER] = {
    /* ********** brackets ********** */
    { OP_LCURLY, OP_RCURLY, OP_FMAKE, OP_LSQUARE, OP_RSQUARE, OP_MMAKE, -1 },
    { OP_LPAREN, OP_RPAREN, OP_FCALL, -1 },
    /* ********** map filter reduce ********** */
    { OP_MAP, OP_FILTER, OP_REDUCE, -1 },
    /* ********** unary ********** */
    { OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR, OP_CONDCALL,
      OP_SWAP, -1 },
    /* ********** arithmetic ********** */
    { OP_EXP, -1 },
    { OP_MUL, OP_DIV, OP_MOD, -1 },
    { OP_ADD, OP_SUB, -1 },
    /* ********** comparison ********** */
    { OP_LT, OP_LE, OP_GT, OP_GE, -1 },
    { OP_EQ, OP_NE, -1 },
    /* ********** logical ********** */
    { OP_AND, -1 },
    { OP_OR, -1 },
    /* ********** pair and callables ********** */
    { OP_PAIR, -1 },
    { OP_FCALLR, -1 },
    { OP_ARG, -1 },
    /* ********** conditional and assignment ********** */
    { OP_CONDAND, -1 },
    { OP_CONDOR, -1 },
    { OP_ASSIGN, OP_CONDPCALL, -1 },
    /* expression separator */
    { OP_EXPRSEP, -1 }
};
#define TIER_COUNT sizeof(OP_TIER_LIST) / MAX_OPS_IN_TIER / sizeof(int)

/* if the left and right chars make up to an operator */
unsigned char
is_2char_op(char left, char right)
{
    static int OP_HASH_ARRAY[OPERATOR_COUNT];
    static int is_initialized = 0;
    unsigned int i;
    if (!is_initialized) {
        for (i = 0; i < OPERATOR_COUNT; i++) {
            if (OP_STRS[i][1] != '\0' && OP_STRS[i][2] == '\0') {
                int h = (((int)OP_STRS[i][0]) << 8) | ((int)OP_STRS[i][1]);
                OP_HASH_ARRAY[i] = h;
            } else {
                OP_HASH_ARRAY[i] = 0;
            }
        }
    }
    for (i = 0; i < OPERATOR_COUNT; i++) {
        if (OP_HASH_ARRAY[i] != 0) {
            int h = (((unsigned int)left) << 8) | ((unsigned int)right);
            if (OP_HASH_ARRAY[i] == h) {
                return 1;
            }
        }
    }
    return 0;
}

const char OP_CHARS[] = "!$%&()*+,-./;<=>?[\\]^`{|}~";

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
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR, OP_CONDCALL,
    OP_SWAP
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

unsigned char
is_prefixable_op(op_name_enum op)
{
    unsigned int i;
    if (op == OP_LCURLY || op == OP_LSQUARE || op == OP_LPAREN) {
        return 1;
    }
    for (i = 0; i < sizeof(UNARY_OPS) / sizeof(int); i++) {
        if (UNARY_OPS[i] == (int)op) {
            return 1;
        }
    }
    return 0;
};

const int BINARY_R_ASSO_OPS[] = {
    /* exponent, pair, special call operations, and assignment */
    OP_EXP, OP_PAIR, OP_FCALLR, OP_ARG, OP_CONDPCALL, OP_ASSIGN
};

unsigned char
is_r_asso_op(op_name_enum op)
{
    unsigned int i;
    for (i = 0; i < sizeof(UNARY_OPS) / sizeof(int); i++) {
        if (UNARY_OPS[i] == (int)op) {
            return 1;
        }
    }
    for (i = 0; i < sizeof(BINARY_R_ASSO_OPS) / sizeof(int); i++) {
        if (BINARY_R_ASSO_OPS[i] == (int)op) {
            return 1;
        }
    }
    return 0;
};
