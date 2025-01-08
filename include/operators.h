#ifndef OPERATOR_H
#define OPERATOR_H

typedef enum op_name {
    OP_LCURLY, OP_RCURLY, OP_FMAKE, OP_LSQUARE, OP_RSQUARE, OP_MMAKE,
    OP_LPAREN, OP_RPAREN, OP_FCALL,
    OP_POS, OP_NEG, OP_NOT, OP_GETL, OP_GETR,
    OP_EXP,
    OP_MUL, OP_DIV, OP_MOD,
    OP_ADD, OP_SUB,
    OP_LT, OP_LE, OP_GT, OP_GE,
    OP_EQ, OP_NE,
    OP_AND,
    OP_OR,
    OP_PAIR,
    OP_FCALLR,
    OP_ARG,
    OP_CONDAND,
    OP_CONDOR,
    OP_ASSIGN, OP_CONDFCALL,
    OP_EXPRSEP
} op_name_enum;

#define OPERATOR_COUNT OP_EXPRSEP + 1

extern const char* const OP_STRS[];

extern const unsigned char OP_PRECEDENCES[];

extern const char OP_CHARS[];

extern const unsigned char IS_UNARY_OP[];

extern const char R_ASSOCIATIVE_OPS[];

extern unsigned char is_2char_op(char left, char right);

#endif
