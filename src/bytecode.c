#include "bytecode.h"
#include "utils/errormsg.h"

void
bytecode_array_extend(
    dynarr_bytecode_t* arr, bytecode_op_code_enum op_code, uint32_t full_arg,
    linecol_t pos
)
{
    if (full_arg <= 0xFFu) {
        bytecode_t bc = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
            .pos = pos,
        };
        dynarr_bytecode_append(arr, &bc);
    } else if (full_arg <= 0xFFFFu) {
        bytecode_t bc1 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 8),
            .pos = pos,
        };
        bytecode_t bc0 = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
            .pos = pos,
        };
        dynarr_bytecode_append(arr, &bc1);
        dynarr_bytecode_append(arr, &bc0);
    } else if (full_arg <= 0xFFFFFFu) {
        bytecode_t bc2 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 16),
            .pos = pos,
        };
        bytecode_t bc1 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 8),
            .pos = pos,
        };
        bytecode_t bc0 = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
            .pos = pos,
        };
        dynarr_bytecode_append(arr, &bc2);
        dynarr_bytecode_append(arr, &bc1);
        dynarr_bytecode_append(arr, &bc0);
    } else if (full_arg <= 0x7FFFFFFFu) {
        bytecode_t bc3 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 24),
            .pos = pos,
        };
        bytecode_t bc2 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 16),
            .pos = pos,
        };
        bytecode_t bc1 = {
            .op = BOP_EXTEND_ARG,
            .arg = (uint8_t)(full_arg >> 8),
            .pos = pos,
        };
        bytecode_t bc0 = {
            .op = op_code,
            .arg = (uint8_t)(full_arg),
            .pos = pos,
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
        bop == BOP_BIND_ARG || bop == BOP_MAKE_FUNCT || bop == BOP_MAKE_MACRO
        || bop == BOP_FRAME_GET || bop == BOP_FRAME_SET || bop == BOP_EXTEND_ARG
        || bop == BOP_PUSH_LITERAL || bop == BOP_JUMP
        || bop == BOP_JUMP_FALSE_OR_POP || bop == BOP_JUMP_TRUE_OR_POP
    );
}

int
bytecode_print(const bytecode_t bytecode)
{
    int printed_bytes_count = 0;
    if (is_arg_bop(bytecode.op)) {
        printed_bytes_count
            += printf("%24s %4u", BYTECODE_OP_NAMES[bytecode.op], bytecode.arg);
    } else {
        printed_bytes_count += printf("%24s", BYTECODE_OP_NAMES[bytecode.op]);
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
