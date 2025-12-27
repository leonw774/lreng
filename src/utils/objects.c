#include "objects.h"
#include "dynarr.h"
#include "errormsg.h"
#include "frame.h"
#include "reserved.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const object_t ERR_OBJECT = (object_t) {
    .is_error = 1,
    .is_const = 1,
    .type = TYPE_NULL,
};
const object_t* ERR_OBJECT_PTR = &ERR_OBJECT;

/* correspond with object_type_enum in objects.h */
const char* OBJ_TYPE_STR[OBJECT_TYPE_NUM + 1] = {
    "Null", /* TYPE_NULL */
    "Number", /* TYPE_NUMBER */
    "Pair", /* TYPE_PAIR */
    "Callable", /* TYPE_CALL */
    "Any", /* TYPE_ANY */
};

object_t*
object_create(object_type_enum type, object_data_union data)
{
    object_t* objptr = malloc(sizeof(object_t));
    *objptr = (object_t) {
        .is_error = 0,
        .is_const = 0,
        .type = type,
        .ref_count = 1,
        .as = data,
    };
    return objptr;
}

object_t*
object_ref(object_t* obj)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("object_ref: obj_addr=%p ref_count=%d\n", obj, obj->ref_count);
    fflush(stdout);
#endif
    if (!obj->is_const) {
        obj->ref_count++;
    }
    return obj;
}

inline void
object_deref(object_t* obj)
{
#ifdef ENABLE_DEBUG_LOG_MORE
    printf("object_deref: addr=%p, ref_count=%d print: ", obj, obj->ref_count);
    object_print(obj, '\n');
    fflush(stdout);
#endif
    if (obj->is_const) {
        return;
    }
    if (obj->ref_count != 1) {
        obj->ref_count--;
        return;
    }
    if (obj->type == TYPE_NUM) {
        number_free(&obj->as.number);
    } else if (obj->type == TYPE_PAIR) {
        if (obj->as.pair.left != NULL) {
            object_deref(obj->as.pair.left);
        }
        if (obj->as.pair.right != NULL) {
            object_deref(obj->as.pair.right);
        }
    } else if (obj->type == TYPE_CALL) {
        /* if is builtin function, it doesn't have frame */
        if (obj->as.callable.builtin_name != NOT_BUILTIN_FUNC) {
            return;
        }
        if (obj->as.callable.init_frame != NULL) {
            if (obj->as.callable.init_frame->ref_count == 1) {
                frame_free_stack(obj->as.callable.init_frame);
                free(obj->as.callable.init_frame);
            } else {
                obj->as.callable.init_frame->ref_count--;
            }
        }
    }
    free(obj);
}

inline int
object_print(const object_t* obj, char end)
{
    int printed_bytes_count = 0;
    if (obj->is_error) {
        printed_bytes_count = printf("[Error]");
    } else if (obj->type == TYPE_NULL) {
        printed_bytes_count = printf("[Null]");
    } else if (obj->type == TYPE_NUM) {
        printed_bytes_count = number_print_frac(&obj->as.number, '\0');
    } else if (obj->type == TYPE_PAIR) {
        printed_bytes_count = printf("[Pair] (");
        if (obj->as.pair.left == NULL) {
            printed_bytes_count += printf("(empty)");
        } else {
            printed_bytes_count += object_print(obj->as.pair.left, '\0');
        }
        printed_bytes_count += printf(", ");
        if (obj->as.pair.right == NULL) {
            printed_bytes_count += printf("(empty)");
        } else {
            printed_bytes_count += object_print(obj->as.pair.right, '\0');
        }
        printed_bytes_count += printf(")");
    } else if (obj->type == TYPE_CALL) {
        if (obj->as.callable.builtin_name != NOT_BUILTIN_FUNC) {
            printed_bytes_count = printf(
                "[Call] (BUILTIN_FUNC %s)",
                RESERVED_IDS[obj->as.callable.builtin_name]
            );
        } else if (obj->as.callable.is_macro) {
            printed_bytes_count = printf(
                "[Call] (MACRO arg_id=%d, entry_index=%d)",
                obj->as.callable.arg_name, obj->as.callable.index
            );
            printed_bytes_count += frame_print(obj->as.callable.init_frame);
        } else {
            printed_bytes_count = printf(
                "[Call] FUNC arg_id=%d, entry_index=%d, frame=",
                obj->as.callable.arg_name, obj->as.callable.index
            );
            printed_bytes_count += frame_print(obj->as.callable.init_frame);
        }
    } else {
        printf("object_print: bad object type: %d\n", obj->type);
        return 0;
    }
    if (end != '\0') {
        printed_bytes_count += printf("%c", end);
    }
    fflush(stdout);
    return printed_bytes_count;
}

inline int
object_eq(object_t* a, object_t* b)
{
    if (a->type != b->type) {
        return 0;
    } else if (a->type == TYPE_CALL) {
        callable_t *a_func = &a->as.callable, *b_func = &b->as.callable;
        return (
            a_func->is_macro == b_func->is_macro
            && a_func->arg_name == b_func->arg_name
            && a_func->builtin_name == b_func->builtin_name
            && a_func->init_frame == b_func->init_frame
            && a_func->index == b_func->index
        );
    } else if (a->type == TYPE_NULL) {
        return 1;
    } else if (a->type == TYPE_NUM) {
        return number_eq(&a->as.number, &b->as.number);
    } else if (a->type == TYPE_PAIR) {
        return object_eq(a->as.pair.left, b->as.pair.left)
            && object_eq(a->as.pair.right, b->as.pair.right);
    } else {
        printf("object_eq: bad type\n");
        exit(1);
    }
}

inline int
object_to_bool(object_t* obj)
{
    if (obj->type == TYPE_NUM) {
        return obj->as.number.numer.size != 0;
    } else {
        return obj->type != TYPE_NULL;
    }
}
