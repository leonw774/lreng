#include "operators.h"

const char* const OP_STRS[] = {
    "{", "}", "@",
    "(", ")", "(",
    "+", "-", "!", "`", "~",
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

const unsigned char OP_PRECEDENCES[] = {
    0, 0, 0,
    1, 1, 1,
    2, 2, 2, 2, 2,
    3,
    4, 4, 4,
    5, 5,
    6, 6, 6, 6,
    7, 7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15, 16,
    17
};

const char OP_CHARS[] = "{}()!`~^*/%+-<>=&|,$=?;";

const char UNARY_OPS[] = {
    OP_FDEF, OP_POS, OP_NEG, OP_NOT, OP_GETL, OP_GETR, '\0'
};

const char R_ASSOCIATIVE_OPS[] = {
    OP_POS, OP_NEG, OP_NOT, OP_GETL, OP_GETR,
    OP_EXP, OP_PAIR, OP_FCALLR, OP_ARG, OP_CONDFCALL, OP_ASSIGN, '\0'
};

/* if the left and right chars make up to an operator */
unsigned char
is_2char_op(char left, char right) {
    if (right == '!') {
        return left == '+' || left == '-';
    }
    if (right == '=') {
        return left == '<' || left == '=' || left == '>' || left == '!';
    }
    return (left == '=' && right == '>')
        || (left == '&' && right == '&')
        || (left == '|' && right == '|');
}