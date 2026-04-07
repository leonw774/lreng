#include <stdint.h>
#include "operators.h"

#ifndef BYTECODE_H
#define BYTECODE_H

typedef enum eval_stack_op_code {
    ES_OP_PUSH,
    ES_OP_POP,
    ES_OP_PEEK,
} eval_stack_op_code_enum;

typedef union bytecode_op {
    op_code_enum regular_op;
    eval_stack_op_code_enum stack_op;
} bytecode_op_t;

typedef struct bytecode {
    uint8_t op;
    int target;
} bytecode_t;

#endif
