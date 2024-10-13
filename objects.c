#include "objects.h"

const object_t const RESERVED_OBJS[RESERVED_ID_NUM] = {
    (object_t) {.null = {.type = OBJ_NULL}},
    (object_t) {.builtin_func = {
        .type = OBJ_BUILTIN_FUNC,
        .name = RESERVED_NAME_INPUT
    }},
    (object_t) {.builtin_func = {
        .type = OBJ_BUILTIN_FUNC,
        .name = RESERVED_NAME_OUTPUT
    }}
};
