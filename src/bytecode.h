#include "operators.h"
#include "token.h"
#include <stdint.h>
#include <stdio.h>

#ifndef BYTECODE_H
#define BYTECODE_H

typedef enum bytecode_op_code {
    BOP_NOP,
    BOP_EXTEND_ARG,
    /* frame & stack manipulation */
    BOP_PUSH_LITERAL, /* push a literal object to stack */
    BOP_FRAME_GET, /* get object from frame and push to stack */
    BOP_FRAME_SET, /* set object from top of stack to frame */
    BOP_FRAME_SET_LITERAL, /* set literal object to frame and push to stack */
    BOP_POP, /* remove the top of stack */
    BOP_RET, /* return the callable with top of stack and clear the stack */
    /* branching and jumping */
    BOP_JUMP,
    BOP_JUMP_FALSE_OR_POP, /* jump if top of stack is false, otherwise, pop */
    BOP_JUMP_TRUE_OR_POP, /* jump if top of stack is true, otherwise, pop */
    /* normal operator */
    BOP_MAKE_FUNCT,
    BOP_MAKE_MACRO,
    BOP_CALL,
    BOP_MAP,
    BOP_FILTER,
    BOP_REDUCE,
    BOP_NEG,
    BOP_NOT,
    BOP_CEIL,
    BOP_FLOOR,
    BOP_GETL,
    BOP_GETR,
    BOP_COND_CALL,
    BOP_SWAP,
    BOP_EXP,
    BOP_MUL,
    BOP_DIV,
    BOP_MOD,
    BOP_ADD,
    BOP_SUB,
    BOP_LT,
    BOP_LE,
    BOP_GT,
    BOP_GE,
    BOP_EQ,
    BOP_NE,
    BOP_AND,
    BOP_OR,
    BOP_PAIR,
    BOP_BIND_ARG,
    BOP_COND_PAIR_CALL,
    BOP_END_Of_ENUM,
} bytecode_op_code_enum;

static const char* const BYTECODE_OP_NAMES[BOP_END_Of_ENUM] = {
    "NOP",
    "EXTEND_ARG",
    "PUSH_LITERAL",
    "FRAME_GET",
    "FRAME_SET",
    "FRAME_SET_LITERAL",
    "POP",
    "RET",
    "JUMP",
    "JUMP_TRUE_OR_POP",
    "JUMP_FALSE_OR_POP",
    "MAKE_FUNCT",
    "MAKE_MACRO",
    "CALL",
    "MAP",
    "FILTER",
    "REDUCE",
    "NEG",
    "NOT",
    "CEIL",
    "FLOOR",
    "GETL",
    "GETR",
    "COND_CALL",
    "SWAP",
    "EXP",
    "MUL",
    "DIV",
    "MOD",
    "ADD",
    "SUB",
    "LT",
    "LE",
    "GT",
    "GE",
    "EQ",
    "NE",
    "AND",
    "OR",
    "PAIR",
    "BIND_ARG",
    "COND_PAIR_CALL",
};

static const int OP_TO_BOP_MAPPING[][2] = {
    { OP_MAKE_FUNCT, BOP_MAKE_FUNCT },
    { OP_MAKE_MACRO, BOP_MAKE_MACRO },
    { OP_CALL, BOP_CALL },
    { OP_MAP, BOP_MAP },
    { OP_FILTER, BOP_FILTER },
    { OP_REDUCE, BOP_REDUCE },
    { OP_POS, BOP_NOP },
    { OP_NEG, BOP_NEG },
    { OP_NOT, BOP_NOT },
    { OP_CEIL, BOP_CEIL },
    { OP_FLOOR, BOP_FLOOR },
    { OP_GETL, BOP_GETL },
    { OP_GETR, BOP_GETR },
    { OP_COND_CALL, BOP_COND_CALL },
    { OP_SWAP, BOP_SWAP },
    { OP_EXP, BOP_EXP },
    { OP_MUL, BOP_MUL },
    { OP_DIV, BOP_DIV },
    { OP_MOD, BOP_MOD },
    { OP_ADD, BOP_ADD },
    { OP_SUB, BOP_SUB },
    { OP_LT, BOP_LT },
    { OP_LE, BOP_LE },
    { OP_GE, BOP_GE },
    { OP_EQ, BOP_EQ },
    { OP_NE, BOP_NE },
    { OP_AND, BOP_AND },
    { OP_OR, BOP_OR },
    { OP_PAIR, BOP_PAIR },
    { OP_CALLR, BOP_CALL },
    { OP_BIND_ARG, BOP_BIND_ARG },
    { OP_COND_AND, BOP_JUMP_TRUE_OR_POP },
    { OP_COND_OR, BOP_JUMP_FALSE_OR_POP },
    { OP_ASSIGN, BOP_FRAME_SET },
    { OP_COND_PAIR_CALL, BOP_COND_PAIR_CALL },
};

typedef struct bytecode {
    uint8_t op;
    uint8_t arg;
    linecol_t pos;
} bytecode_t;

#define TYPE bytecode_t
#define TYPE_NAME bytecode
#include "utils/dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

void bytecode_array_extend(
    dynarr_bytecode_t* arr, bytecode_op_code_enum op_code, uint32_t full_arg,
    linecol_t pos
);

int bytecode_print(const bytecode_t bytecode);

bytecode_op_code_enum op_to_bop_code(op_code_enum op_code);

#endif
