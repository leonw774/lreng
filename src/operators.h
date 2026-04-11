#ifndef OPERATOR_H
#define OPERATOR_H

typedef enum op_code {
    /* ******** brackets ******** */
    OP_LCURLY,
    OP_RCURLY,
    OP_MAKE_FUNCT,
    OP_LSQUARE,
    OP_RSQUARE,
    OP_MAKE_MACRO,
    OP_LPAREN,
    OP_RPAREN,
    /* ******** call ******** */
    OP_CALL,
    /* ******** map filter reduce ******** */
    OP_MAP,
    OP_FILTER,
    OP_REDUCE,
    /* ******** unary ******** */
    OP_POS,
    OP_NEG,
    OP_NOT,
    OP_CEIL,
    OP_FLOOR,
    OP_GETL,
    OP_GETR,
    OP_COND_CALL,
    OP_SWAP,
    /* ******** arithmetic ******** */
    OP_EXP,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_ADD,
    OP_SUB,
    /* ******** comparison ******** */
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_EQ,
    OP_NE,
    /* ******** logical operators ******** */
    OP_AND,
    OP_OR,
    /* ******** pair and callables ******** */
    OP_PAIR,
    OP_CALLR,
    OP_BIND_ARG,
    /* ******** conditional and assignment ******** */
    OP_COND_AND,
    OP_COND_OR,
    OP_ASSIGN,
    OP_COND_PAIR_CALL,
    /* ******** expression separator ******** */
    OP_EXPRSEP,
    /* ******** end of enum ******** */
    OP_END_OF_ENUM,
} op_code_enum;

#define OPERATOR_COUNT OP_END_OF_ENUM

static const int UNARY_OPS[] = {
    /* callable makers are also unary operators */
    OP_MAKE_FUNCT, OP_MAKE_MACRO,
    /* other unary operators */
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR, OP_COND_CALL,
    OP_SWAP
};

static const int BINARY_RIGHT_ASSOCIATIVE_OPS[] = {
    /* exponent, pair, special call operations, and assignment */
    OP_EXP, OP_PAIR, OP_CALLR, OP_BIND_ARG, OP_COND_PAIR_CALL, OP_ASSIGN
};

/* OP_STR must be aligned with op_code_enum */

static const char* const OP_STRS[OPERATOR_COUNT] = {
    /* ******** brackets ******** */
    "{", "}", "make_func", "[", "]", "make_macro", "(", ")",
    /* ******** function call ******** */
    "call",
    /* ******** map filter reduce ******** */
    "$>", "$|", "$/",
    /* ******** unary ******** */
    "+", "-", "!", "^", "\\", "<", ">", "`", "~",
    /* ******** arithmetic ******** */
    "^", "*", "/", "%", "+", "-",
    /* ******** comparison ******** */
    "<", "<=", ">", ">=", "==", "!=",
    /* ******** logical ******** */
    "&", "|",
    /* ******** pair and callables ******** */
    ",", "$", "=>",
    /* ******** conditional and assignment ******** */
    "&&", "||", "=", "?",
    /* ******** expression separator ******** */
    ";"
};

#define MAX_OPS_IN_TIER 10

static const int OP_TIER_LIST[][MAX_OPS_IN_TIER] = {
    /* ******** brackets ******** */
    { OP_LCURLY, OP_RCURLY, OP_MAKE_FUNCT, OP_LSQUARE, OP_RSQUARE, OP_MAKE_MACRO, -1 },
    { OP_LPAREN, OP_RPAREN, -1 },
    /* ******** function call ******** */
    { OP_CALL, -1 },
    /* ******** map filter reduce ******** */
    { OP_MAP, OP_FILTER, OP_REDUCE, -1 },
    /* ******** unary ******** */
    { OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR, OP_GETL, OP_GETR, OP_COND_CALL,
      OP_SWAP, -1 },
    /* ******** arithmetic ******** */
    { OP_EXP, -1 },
    { OP_MUL, OP_DIV, OP_MOD, -1 },
    { OP_ADD, OP_SUB, -1 },
    /* ******** comparison ******** */
    { OP_LT, OP_LE, OP_GT, OP_GE, -1 },
    { OP_EQ, OP_NE, -1 },
    /* ******** logical ******** */
    { OP_AND, -1 },
    { OP_OR, -1 },
    /* ******** pair and callables ******** */
    { OP_PAIR, -1 },
    { OP_CALLR, -1 },
    { OP_BIND_ARG, -1 },
    /* ******** conditional and assignment ******** */
    { OP_COND_AND, -1 },
    { OP_COND_OR, -1 },
    { OP_ASSIGN, OP_COND_PAIR_CALL, -1 },
    /* ******** expression separator ******** */
    { OP_EXPRSEP, -1 }
};

#define TIER_COUNT sizeof(OP_TIER_LIST) / MAX_OPS_IN_TIER / sizeof(int)

extern int is_op_char(char c);

extern int is_2char_op(char left, char right);

extern int get_op_precedence(op_code_enum op);

extern int is_unary_op(op_code_enum op);

extern int is_prefixable_op(op_code_enum op);

extern int is_right_associative_op(op_code_enum op);

extern const char R_ASSOCIATIVE_OPS[];

#endif
