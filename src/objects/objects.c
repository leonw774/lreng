#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynarr.h"
#include "token.h"
#include "frame.h"
#include "errormsg.h"
#include "objects.h"

const object_t const RESERVED_OBJS[RESERVED_ID_NUM] = {
    NULL_OBJECT,
    (object_t) {.type = TYPE_FUNC, .data = { .func = {
        .create_time_frame = NULL,
        .entry_index = -1,
        .arg_name = -1,
        .builtin_name = RESERVED_ID_NAME_INPUT
    }}},
    (object_t) { .type = TYPE_FUNC, .data = { .func = {
        .create_time_frame = NULL,
        .entry_index = -1,
        .arg_name = -1,
        .builtin_name = RESERVED_ID_NAME_OUTPUT
    }}}
};

const char* OBJECT_TYPE_STR[OBJECT_TYPE_NUM + 1] = {
    "Null", "Number", "Pair", "Function", "Built-in Function",
    "Any"
};

object_t*
alloc_empty_object(object_type_enum type) {
    object_t* o = calloc(1, sizeof(object_t));
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
    if (obj->type == TYPE_NUM) {
        object_t clone = {
            .type = TYPE_NUM,
            .data = {.number = EMPTY_NUMBER()}
        };
        copy_number(&clone.data.number, &obj->data.number);
        return clone;
    }
    else if (obj->type == TYPE_PAIR) {
        object_t clone = {
            .type = TYPE_PAIR,
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
    else if (obj->type == TYPE_FUNC) {
        object_t clone = *obj;
        if (obj->data.func.builtin_name != NOT_BUILTIN_FUNC) {
            return clone;
        }
        /* only copy reference of frame */
        clone.data.func.create_time_frame = obj->data.func.create_time_frame;
        ((frame_t*) clone.data.func.create_time_frame)->refer_count++;
        return clone;
    }
    else {
        object_t clone = *obj;
        return clone;
    }
}

void
free_object(object_t* obj) {
    if (obj->type == TYPE_NUM) {
        free_number(&obj->data.number);
    }
    else if (obj->type == TYPE_PAIR) {
        free_object(obj->data.pair.left);
        free_object(obj->data.pair.right);
        free(obj->data.pair.left);
        free(obj->data.pair.right);
    }
    else if (obj->type = TYPE_FUNC) {
        /* if is builtin function, it doesn't have frame */
        if (obj->data.func.builtin_name != NOT_BUILTIN_FUNC) {
            return;
        }
        frame_t* f = (frame_t*) obj->data.func.create_time_frame;
        if (f != NULL) {
            // printf("free_object: frame %p refer_count=%d\n", f, f->refer_count);
            if (f->refer_count <= 1) {
                free_frame(f, 1);
                free(f);
            }
            else {
                f->refer_count--;
            }
        }
    }
}

int
print_object(object_t* obj) {
    int printed_bytes_count = 0;
    if (obj->type == TYPE_NULL) {
        return printf("[Null]");
    }
    else if (obj->type == TYPE_NUM) {
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
                "[Func] builtin_name=%d", obj->data.func.builtin_name
            );
        }
        int i;
        // for (i = 0; i < obj->data.func.create_time_frame->stack.size; i++) {
        //     printf("frame pairs[%d].data=%p\n", i,
        //         ((dynarr_t*) obj->data.func.create_time_frame->stack.data)[i].data);
        // }
        return printf(
            "[Func] arg_name=%d, entry_index=%d, frame=%p",
            obj->data.func.arg_name,
            obj->data.func.entry_index,
            obj->data.func.create_time_frame
        );
    }
    printf("print_object: bad object type: %d\n", obj->type);
    return 0;
}

int
object_eq(object_t* a, object_t* b) {
    if (a->type != b->type) {
        return 0;
    }
    else if (a->type == TYPE_FUNC) {
        func_t *a_func = &a->data.func, *b_func = &b->data.func;
        return (
            a_func->arg_name == b_func->arg_name
            && a_func->builtin_name == b_func->builtin_name
            && a_func->create_time_frame == b_func->create_time_frame
            && a_func->entry_index == b_func->entry_index
        );
    }
    else if (a->type == TYPE_NULL) {
        return 1;
    }
    else if (a->type == TYPE_NUM) {
        return number_eq(&a->data.number, &b->data.number);
    }
    else if (a->type == TYPE_PAIR) {
        return object_eq(a->data.pair.left, b->data.pair.left)
            && object_eq(a->data.pair.right, b->data.pair.right);
    }
    else {
        printf("object_eq: bad type\n");
        exit(1);
    }
}

inline int
to_bool(object_t* obj) {
    if (obj->type == TYPE_NUM) {
        return obj->data.number.numer.size != 0;
    }
    return obj->type != TYPE_NULL;
}
