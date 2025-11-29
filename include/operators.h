#ifndef OPERATOR_H
#define OPERATOR_H

typedef enum op_name {
    /* ********** brackets ********** */
    OP_LCURLY, OP_RCURLY, OP_FMAKE,
    OP_LSQUARE, OP_RSQUARE, OP_MMAKE,
    OP_LPAREN, OP_RPAREN, OP_FCALL,
    /* ********** map filter reduce ********** */
    OP_MAP, OP_FILTER, OP_REDUCE,
    /* ********** unary ********** */
    OP_POS, OP_NEG, OP_NOT, OP_CEIL, OP_FLOOR,
    OP_GETL, OP_GETR, OP_CONDCALL, OP_SWAP,
    /* ********** arithmetic ********** */
    OP_EXP, OP_MUL, OP_DIV, OP_MOD, OP_ADD, OP_SUB,
    /* ********** comparison ********** */
    OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE,
    /* ********** logical operators ********** */
    OP_AND, OP_OR,
    /* ********** pair and callables ********** */
    OP_PAIR, OP_FCALLR, OP_ARG,
    /* ********** conditional and assignment ********** */
    OP_CONDAND, OP_CONDOR, OP_ASSIGN, OP_CONDPCALL,
    /* expression separator */
    OP_EXPRSEP,
} op_name_enum;

/* OP_STR must be aligned with op_name_enum */

static const char* const OP_STRS[] = {
    /* ********** brackets ********** */
    "{", "}", "make_func", "[", "]", "make_macro", "(", ")", "call",
    /* ********** map filter reduce ********** */
    "$>", "$|", "$/",
    /* ********** unary ********** */
    "+", "-", "!", "^", "\\",
    "<", ">", "`", "~",
    /* ********** arithmetic ********** */
    "^", "*", "/", "%", "+", "-",
    /* ********** comparison ********** */
    "<", "<=", ">", ">=", "==", "!=",
    /* ********** logical ********** */
    "&", "|",
    /* ********** pair and callables ********** */
    ",", "$", "=>",
    /* ********** conditional and assignment ********** */
    "&&", "||", "=", "?",
    /* expression separator */
    ";"
};

#define OPERATOR_COUNT OP_EXPRSEP + 1

extern unsigned char is_2char_op(char left, char right);


extern const char OP_CHARS[];

extern int get_op_precedence(op_name_enum op);

extern unsigned char is_unary_op(op_name_enum op);

extern unsigned char is_prefixable_op(op_name_enum op);

extern unsigned char is_r_asso_op(op_name_enum op);

extern const char R_ASSOCIATIVE_OPS[];

#endif
