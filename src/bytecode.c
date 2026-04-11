#include "bytecode.h"
#include "utils/errormsg.h"

void
bytecode_array_extend(
    dynarr_bytecode_t* arr, bytecode_op_code_enum op_code, uint32_t full_arg
)
{
    if (full_arg <= 0xFFu) {
        bytecode_t bc = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
        };
        dynarr_bytecode_append(arr, &bc);
    } else if (full_arg <= 0xFFFFu) {
        bytecode_t bc1 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 7),
        };
        bytecode_t bc0 = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
        };
        dynarr_bytecode_append(arr, &bc1);
        dynarr_bytecode_append(arr, &bc0);
    } else if (full_arg <= 0xFFFFFFu) {
        bytecode_t bc2 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 15),
        };
        bytecode_t bc1 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 7),
        };
        bytecode_t bc0 = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
        };
        dynarr_bytecode_append(arr, &bc2);
        dynarr_bytecode_append(arr, &bc1);
        dynarr_bytecode_append(arr, &bc0);
    } else if (full_arg <= 0x7FFFFFFFu) {
        bytecode_t bc3 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 23),
        };
        bytecode_t bc2 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 15),
        };
        bytecode_t bc1 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 7),
        };
        bytecode_t bc0 = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
        };
        dynarr_bytecode_append(arr, &bc3);
        dynarr_bytecode_append(arr, &bc2);
        dynarr_bytecode_append(arr, &bc1);
        dynarr_bytecode_append(arr, &bc0);
    } else {
        print_runtime_error((linecol_t) { 0, 0 }, "bytecode arg too large");
        exit(OTHER_ERR_CODE);
    }
}

int
is_arg_bop(bytecode_op_code_enum bop)
{
    return (
        bop == BOP_BIND_ARG || bop == BOP_FRAME_GET || bop == BOP_FRAME_SET
        || bop == BOP_EXTEND_ARG || bop == BOP_JUMP
        || bop == BOP_JUMP_FALSE_OR_POP || bop == BOP_JUMP_TRUE_OR_POP
    );
}

int
bytecode_array_print(dynarr_bytecode_t* arr, const token_t* context_tokens)
{
    int i, printed_bytes_count = 0;
    for (i = 0; i < arr->size; i++) {
        if (is_arg_bop(arr->data[i].op)) {
            const char* context_str = NULL;
            if (context_tokens && context_tokens[arr->data[i].arg].str) {
                context_str = context_tokens[arr->data[i].arg].str;
            }
            printed_bytes_count += printf(
                "%4u: %24s %4u %8s\n", i, BYTECODE_OP_NAMES[arr->data[i].op],
                arr->data[i].arg, context_str
            );
        } else {
            printed_bytes_count += printf(
                "%4u: %24s    \n", i, BYTECODE_OP_NAMES[arr->data[i].op]
            );
        }
    }
    return printed_bytes_count;
}

bytecode_op_code_enum
op_to_bop_code(op_code_enum op_code)
{
    static bytecode_op_code_enum OP_TO_BOP_TABLE[OPERATOR_COUNT];
    static unsigned char is_initialized = 0;
    unsigned int i;
    if (is_initialized == 0) {
        memset(OP_TO_BOP_TABLE, 0, sizeof(OP_TO_BOP_TABLE));
        for (i = 0; i < sizeof(OP_TO_BOP_MAPPING) / sizeof(int) / 2; i++) {
            OP_TO_BOP_TABLE[OP_TO_BOP_MAPPING[i][0]] = OP_TO_BOP_MAPPING[i][1];
        }
        is_initialized = 1;
    }
    return OP_TO_BOP_TABLE[op_code];
}
