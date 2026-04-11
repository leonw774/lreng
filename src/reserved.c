#include "reserved.h"

const char* RESERVED_IDS[RESERVED_ID_COUNT] = {
    "null",
    "input",
    "output",
    "error",
    "is_number",
    "is_callable",
    "is_pair",
    "debug",
};

const object_t RESERVED_OBJS[RESERVED_ID_COUNT] = {
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_NULL,
        .ref_count = 1,
        .as.null = 0,
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_INPUT
        },
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_OUTPUT,
        },
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_ERROR,
        },
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_IS_NUMBER,
        },
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_IS_CALLABLE,
        },
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_IS_PAIR,
        },
    },
    (object_t) {
        .is_error = 0,
        .is_const = 1,
        .type = TYPE_CALL,
        .ref_count = 1,
        .as.callable = {
            .init_frame = NULL,
            .index = -1,
            .arg_name = -1,
            .builtin_name = RESERVED_ID_CODE_DEBUG,
        },
    }
};

const object_t* NULL_OBJECT_PTR = &RESERVED_OBJS[RESERVED_ID_CODE_NULL];
