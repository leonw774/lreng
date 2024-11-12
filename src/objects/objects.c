#include <stdio.h>
#include <stdlib.h>
#include "token.h"
#include "errormsg.h"
#include "objects.h"

const object_t const RESERVED_OBJS[RESERVED_ID_NUM] = {
    NULL_OBJECT,
    (object_t) {.type = TYPE_FUNC, .data = { .func = {
        .builtin_name = RESERVED_ID_NAME_INPUT,
        .arg_name = -1,
        .entry_index = -1,
        .tree = NULL,
        .frame = NULL
    }}},
    (object_t) { .type = TYPE_FUNC, .data = { .func = {
        .builtin_name = RESERVED_ID_NAME_OUTPUT,
        .arg_name = -1,
        .entry_index = -1,
        .tree = NULL,
        .frame = NULL
    }}}
};

const char* OBJECT_TYPE_STR[OBJECT_TYPE_NUM] = {
    "Null", "Number", "Pair", "Function", "Built-in Function"
};

object_t*
alloc_empty_object(object_type_enum type) {
    object_t* o = calloc(1, sizeof(object_t));
    printf("alloc_empty_object %p\n", o);
    fflush(stdout);
    if (o == NULL) {
        printf("alloc_empty_object: allocation failed\n");
        exit(OTHER_ERR_CODE);
    }
    o->type = type;
    return o;
}

object_t
copy_object(const object_t* obj) {
    if (obj->type == TYPE_NUMBER) {
        object_t clone = {
            .type = obj->type,
            .data = {.number = EMPTY_NUMBER()}
        };
        copy_number(&clone.data.number, &obj->data.number);
        return clone;
    }
    else if (obj->type == TYPE_PAIR) {
        object_t clone = {
            .type = obj->type,
            .data = {
                .pair = {.left = NULL, .right = NULL}
            }
        };
        clone.data.pair.left = (object_t*) malloc(sizeof(object_t));
        clone.data.pair.right = (object_t*) malloc(sizeof(object_t));
        if (clone.data.pair.left == NULL || clone.data.pair.right == NULL) {
            exit(OTHER_ERR_CODE);
        }
        *(clone.data.pair.left) = copy_object(obj->data.pair.left);
        *(clone.data.pair.right) = copy_object(obj->data.pair.right);
        return clone;
    }
    else {
        object_t clone = *obj;
        return clone;
    }
}

void
free_object(object_t* obj) {
    if (obj->type == TYPE_NUMBER) {
        free_number(&obj->data.number);
    }
    else if (obj->type == TYPE_PAIR) {
        free_object(obj->data.pair.left);
        free_object(obj->data.pair.right);
    }
}

int
print_object(object_t* obj) {
    int printed_bytes_count = 0;
    if (obj->type == TYPE_NULL) {
        return printf("[Null]");
    }
    else if (obj->type == TYPE_NUMBER) {
        return print_number_frac(&obj->data.number);
    }
    else if (obj->type == TYPE_PAIR) {
        printed_bytes_count = printf("[Pair] (");
        printed_bytes_count += print_object(obj->data.pair.left);
        printed_bytes_count += printf(", ");
        printed_bytes_count += print_object(obj->data.pair.right);
        putchar(')');
        return printed_bytes_count + 1;
    }
    else if (obj->type == TYPE_FUNC) {
        if (obj->data.func.builtin_name != NOT_BUILTIN_FUNC) {
            return printf(
                "[Func] builtin_name=%d",
                RESERVED_IDS[obj->data.func.builtin_name]
            );
        }
        return printf(
            "[Func] arg_name=%d, entry_index=%d",
            obj->data.func.arg_name,
            obj->data.func.entry_index
        );
    }
    printf("print_object: bad object type: %d\n", obj->type);
    return 0;
}

int
to_boolean(object_t* obj) {
    if (obj->type == TYPE_NUMBER) {
        return !(obj->data.number.zero);
    }
    return (obj->type == TYPE_NULL);
}