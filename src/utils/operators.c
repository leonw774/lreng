#include "operators.h"
#include "stdlib.h"
#include "string.h"

/* is this charactor appear in any of the operator token */
int
is_op_char(char c)
{
    /* offset by 32 just to save a little space */
    static unsigned char IS_OP_CHAR[256 - 32] = {0};
    static int is_initialized = 0;
    int i, j;
    if (!is_initialized) {
        for (i = 0; i < OPERATOR_COUNT; i++) {
            for (j = 0; OP_STRS[i][j] != '\0'; j++) {
                if (OP_STRS[i][j] > 32) {
                    IS_OP_CHAR[OP_STRS[i][j] - 32] = 1;
                }
            }
        }
        is_initialized = 1;
    }
    return c < 32 ? 0 : IS_OP_CHAR[c - 32];
}

/* if the left and right chars make up to an operator */
int
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
        is_initialized = 1;
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

int
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

int
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

int
is_right_associative_op(op_name_enum op)
{
    unsigned int i;
    for (i = 0; i < sizeof(UNARY_OPS) / sizeof(int); i++) {
        if (UNARY_OPS[i] == (int)op) {
            return 1;
        }
    }
    for (i = 0; i < sizeof(BINARY_RIGHT_ASSOCIATIVE_OPS) / sizeof(int); i++) {
        if (BINARY_RIGHT_ASSOCIATIVE_OPS[i] == (int)op) {
            return 1;
        }
    }
    return 0;
};
