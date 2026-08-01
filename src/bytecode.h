#include "operators.h"
#include "token.h"
#include <stdint.h>
#include <stdio.h>

#ifndef BYTECODE_H
#define BYTECODE_H

typedef enum bytecode_op_code {
    BOP_NOP,
    BOP_EXTEND_ARG,

    /**
     * frame & stack manipulation
     */

    /* push a literal object to object stack */
    BOP_PUSH_LIT,
    /* get object from frame and push to object stack */
    BOP_FGET,
    /* set object from top of stack to frame */
    BOP_FSET,
    /* set literal object to frame and push to object stack */
    BOP_FSET_LIT,
    /* use pair to recursively set object to frame and push to object stack */
    BOP_FSET_UNPACK,
    /* remove the top of stack */
    BOP_POP,
    /* pop the frame stack */
    BOP_RET,
    
    /**
     * branching and jumping
     */
    
    /* branch if top of stack is false, otherwise, pop */
    BOP_BF_OR_POP,
    /* branch if top of stack is true, otherwise, pop */
    BOP_BT_OR_POP,
    
    /**
     *  normal operators
     */
    
    BOP_MAKE_FUNCT,
    BOP_MAKE_MACRO,
    BOP_CALL,
#if DEPRECATED
    BOP_MAP,
    BOP_FILTER,
    BOP_REDUCE,
#endif
    BOP_NEG,
    BOP_NOT,
    BOP_CEIL,
    BOP_FLOOR,
    BOP_PGETL,
    BOP_PGETR,
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
    BOP_COND_PGET,
    BOP_COND_PCALL,
    BOP_END_Of_ENUM,
} bytecode_op_code_enum;

static const char* const BYTECODE_OP_NAMES[BOP_END_Of_ENUM] = {
    "NOP",
    "EXTEND_ARG",
    "PUSH_LIT",
    "FGET",
    "FSET",
    "FSET_LIT",
    "FSET_UNPACK",
    "POP",
    "RET",
    "BT_OR_POP",
    "BF_OR_POP",
    "MAKE_FUNCT",
    "MAKE_MACRO",
    "CALL",
#if DEPRECATED
    "MAP",
    "FILTER",
    "REDUCE",
#endif
    "NEG",
    "NOT",
    "CEIL",
    "FLOOR",
    "PGETL",
    "PGETR",
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
    "COND_PGET",
    "COND_PCALL",
};

static const int OP_TO_BOP_MAPPING[][2] = {
    { OP_MAKE_FUNCT, BOP_MAKE_FUNCT },
    { OP_MAKE_MACRO, BOP_MAKE_MACRO },
    { OP_CALL, BOP_CALL },
    { OP_POS, BOP_NOP },
    { OP_NEG, BOP_NEG },
    { OP_NOT, BOP_NOT },
    { OP_CEIL, BOP_CEIL },
    { OP_FLOOR, BOP_FLOOR },
    { OP_PGETL, BOP_PGETL },
    { OP_PGETR, BOP_PGETR },
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
    { OP_GT, BOP_GT },
    { OP_GE, BOP_GE },
    { OP_EQ, BOP_EQ },
    { OP_NE, BOP_NE },
    { OP_AND, BOP_AND },
    { OP_OR, BOP_OR },
    { OP_PAIR, BOP_PAIR },
    { OP_CALLR, BOP_CALL },
    { OP_BIND_ARG, BOP_BIND_ARG },
    { OP_COND_AND, BOP_BT_OR_POP },
    { OP_COND_OR, BOP_BF_OR_POP },
    { OP_COND_PGET, BOP_COND_PGET },
    { OP_COND_PCALL, BOP_COND_PCALL },
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

int bytecode_stack_diff(bytecode_op_code_enum bop);

#endif
