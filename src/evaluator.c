#include "builtin_funcs.h"
#include "dynarr.h"
#include "errormsg.h"
#include "frame.h"
#include "lreng.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_OPRAND -1

static inline int
is_bad_type(
    token_t op_token, int left_good_type, int right_good_type,
    object_t* left_obj, object_t* right_obj
)
{
    int left_passed, right_passed;
    if (left_good_type == NO_OPRAND) {
        left_passed = left_obj == NULL;
    } else if (left_good_type == TYPE_ANY) {
        left_passed = left_obj != NULL;
    } else {
        left_passed = left_obj != NULL && left_obj->type == left_good_type;
    }
    if (right_good_type == NO_OPRAND) {
        right_passed = right_obj == NULL;
    } else if (right_good_type == TYPE_ANY) {
        right_passed = right_obj != NULL;
    } else {
        right_passed = right_obj != NULL && right_obj->type == right_good_type;
    }
    if (left_passed & right_passed) {
        return 0;
    }
    printf("%d", op_token.name);
    sprintf(
        ERR_MSG_BUF,
        "Bad type for operator \"%s\": expect (%s, %s), get (%s, %s)",
        OP_STRS[op_token.name],
        left_good_type == NO_OPRAND ? "" : OBJ_TYPE_STR[left_good_type],
        right_good_type == NO_OPRAND ? "" : OBJ_TYPE_STR[right_good_type],
        left_obj == NULL ? "" : OBJ_TYPE_STR[left_obj->type],
        right_obj == NULL ? "" : OBJ_TYPE_STR[right_obj->type]
    );
    print_runtime_error(op_token.pos, ERR_MSG_BUF);
    return 1;
}

void
construct_call_frame(
    frame_t* call_frame, const frame_t* cur_frame, const object_t* func
)
{
    /* if is direct recursion, copy the current stack except the last */
    if (cur_frame->stack.size > 0
        && *(int*)back(&cur_frame->indexs) == func->data.callable.index) {
        call_frame->indexs = dynarr_copy(&cur_frame->indexs);
        pop(&call_frame->indexs);
        call_frame->stack = dynarr_copy(&cur_frame->stack);
        pop(&call_frame->stack);
    }
    /* if the i-th stack of the function's init-time frame and current frame is
       the same, call-frame's i-th stack equals current frame's i-th stack. but
       once the entry_index is different, they are in different closure path,
       so only the rest of init-time-frame stack is used
    */
    else {
        int i, cur_frame_index = -1, init_frame_index = -1, is_forked = 0;
        const frame_t* f_init_frame = func->data.callable.init_time_frame;
        stack_new(call_frame);
        for (i = 0; i < f_init_frame->stack.size; i++) {
            dynarr_t* src_pairs = NULL;
            init_frame_index = *(int*)at(&f_init_frame->indexs, i);
            cur_frame_index = -1;
            if (i < cur_frame->stack.size) {
                cur_frame_index = *(int*)at(&cur_frame->indexs, i);
            }
            /* push the entry_index */
            append(&call_frame->indexs, &init_frame_index);

            if (!is_forked && cur_frame_index == init_frame_index) {
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf("exec_call: stack[%d] use current\n", i);
                }
#endif
                src_pairs = (dynarr_t*)at(&cur_frame->stack, i);
            } else {
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf("exec_call: stack[%d] use init-time\n", i);
                }
#endif
                is_forked = 1;
                src_pairs = (dynarr_t*)at(&f_init_frame->stack, i);
            }
            /* push the shallow copy of source pairs */
            append(&call_frame->stack, src_pairs);
        }
    }
    /* push new stack to call_frame */
    stack_push(call_frame, func->data.callable.index);
}

static inline object_t*
exec_call(context_t context, linecol_t pos, const object_t* func, object_t* arg)
{
    object_t* result;
    frame_t* cur_frame = context.cur_frame;
    /* if is builtin */
    if (func->data.callable.builtin_name != -1) {
        object_t* (*func_ptr)(const object_t*)
            = BUILDTIN_FUNC_ARRAY[func->data.callable.builtin_name];
        if (func_ptr == NULL) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        ERR_MSG_BUF[0] = '\0';
        result = func_ptr(arg);
        if (result->is_error && ERR_MSG_BUF[0] != '\0') {
            print_runtime_error(pos, ERR_MSG_BUF);
        }
        return result;
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_call: prepare call frame\n");
    }
#endif
    frame_t* call_frame;
    if (func->data.callable.is_macro) {
        call_frame = cur_frame;
    } else {
        call_frame = calloc(1, sizeof(frame_t));
        call_frame->global_pairs = cur_frame->global_pairs;
        construct_call_frame(call_frame, cur_frame, func);
        /* set argument to call-frame */
        if (func->data.callable.arg_name != -1) {
            frame_set(call_frame, func->data.callable.arg_name, arg);
        }
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_call: call_frame=%p\nfunc_obj=", call_frame);
        object_print(func, '\n');
        printf("arg=");
        object_print(arg, '\n');
    }
#endif
    /* eval from function's entry index */
    context_t new_context = {
        .tree = context.tree,
        .cur_frame = call_frame,
    };
    result = eval(new_context, func->data.callable.index);
    if (!func->data.callable.is_macro) {
        /* free the objects own by this function call */
        stack_pop(call_frame);
        /* free the rest of stack but not free bollowed pairs */
        stack_clear(call_frame, 0);
        free(call_frame);
    }
    return result;
}

enum pair_traverse_code_enum {
    NEW_LEAF,
    VISITED_LEAF,
    NEW_PAIR,
    VISITED_PAIR,
    ERROR,
};

int
map_process_node(
    context_t context, linecol_t pos, const object_t* func, object_t* arg,
    object_t** res
)
{
    if (arg->type == TYPE_PAIR) {
        if (*res == NULL) {
            *res = object_create(
                TYPE_PAIR, (object_data_t) { .pair = (pair_t) { NULL, NULL } }
            );
            return NEW_PAIR;
        }
        return VISITED_PAIR;
    }
    if (*res == NULL) {
        object_t* result = exec_call(context, pos, func, arg);
        if (result->is_error) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_map: a child return with error: ");
                object_print(result, '\n');
            }
#endif
            object_free(result);
            return ERROR;
        }
        *res = result;
        return NEW_LEAF;
    }
    return VISITED_LEAF;
}

/* apply function recursively in postfix order and return a new pair */
object_t*
exec_map(context_t context, linecol_t pos, const object_t* func, object_t* pair)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_map\n");
        printf("func=");
        object_print(func, '\n');
        printf("pair=");
        object_print(pair, '\n');
    }
#endif
    int is_error = 0;
    object_t* result = object_create(
        TYPE_PAIR, (object_data_t) { .pair = (pair_t) { NULL, NULL } }
    );
    dynarr_t arg_pair_stack, res_pair_stack;
    arg_pair_stack = dynarr_new(sizeof(object_t*));
    res_pair_stack = dynarr_new(sizeof(object_t*));
    append(&arg_pair_stack, &pair);
    append(&res_pair_stack, &result);
    while (arg_pair_stack.size > 0) {
        object_t* arg_pair = *(object_t**)back(&arg_pair_stack);
        object_t* res_pair = *(object_t**)back(&res_pair_stack);
        object_t* arg_left = arg_pair->data.pair.left;
        object_t* arg_right = arg_pair->data.pair.right;
        object_t** res_left = &res_pair->data.pair.left;
        object_t** res_right = &res_pair->data.pair.right;
        int process_result_enum;

        /* check */
        if (arg_left == NULL || arg_right == NULL) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_map: bad pair - left or right is empty: ");
                object_print(arg_pair, '\n');
            }
#endif
            is_error = 1;
            break;
        }
        /* left */
        process_result_enum
            = map_process_node(context, pos, func, arg_left, res_left);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_map: a left child return with error: ");
                object_print(*res_left, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_left);
            append(&res_pair_stack, res_left);
            continue;
        }
        /* right */
        process_result_enum
            = map_process_node(context, pos, func, arg_right, res_right);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_map: a right child return with error: ");
                object_print(*res_right, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_right);
            append(&res_pair_stack, res_right);
            continue;
        }

        pop(&arg_pair_stack);
        pop(&res_pair_stack);
    }
    if (is_error) {
        object_free(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }
    dynarr_free(&arg_pair_stack);
    dynarr_free(&res_pair_stack);
    return result;
}

object_t*
filter_merge(object_t* pair_to_merge)
{
    if (pair_to_merge->type != TYPE_PAIR) {
        return pair_to_merge;
    }
    object_t* right = pair_to_merge->data.pair.right;
    object_t* left = pair_to_merge->data.pair.left;
    if (left->is_error && right->is_error) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("filter_merge: pair removed\n");
        }
#endif
        object_free(pair_to_merge);
        return (object_t*)ERR_OBJECT_PTR;
    } else if (left->is_error) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("filter_merge: pair replaced by its right\n");
        }
#endif
        /* we free parent but don't want to free children so increase 1 here to
           counter the decrease in object_free
        */
        object_t* right_copy = object_copy(right);
        object_free(pair_to_merge);
        return right_copy;
    } else if (right->is_error) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("filter_merge: pair replaced by its left\n");
        }
#endif
        object_t* left_copy = object_copy(left);
        object_free(pair_to_merge);
        return left_copy;
    }
    return pair_to_merge;
}

int
filter_process_node(
    context_t context, linecol_t pos, const object_t* func, object_t* arg,
    object_t** res
)
{
    if (arg->type == TYPE_PAIR) {
        if (*res == NULL) {
            *res = object_create(
                TYPE_PAIR, (object_data_t) { .pair = (pair_t) { NULL, NULL } }
            );
            return NEW_PAIR;
        }
        return VISITED_PAIR;
    }
    if (*res == NULL) {
        object_t* result = exec_call(context, pos, func, arg);
        if (result->is_error) {
            object_free(result);
            return ERROR;
        }
        if (object_to_bool(result)) {
            /* keep */
            *res = object_copy(arg);
        } else {
            /* removed */
            *res = (object_t*)ERR_OBJECT_PTR;
        }
        object_free(result);
        return NEW_LEAF;
    }
    return VISITED_LEAF;
}

/* apply function recursively in postfix order to the children. if the return
   value is true, keep the child, otherwise, set it to NULL.
   if a pair's two children's return value is all false, the pair itself becomes
   NULL as well. */
object_t*
exec_filter(
    context_t context, linecol_t pos, const object_t* func, object_t* pair
)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_filter\n");
        printf("func=");
        object_print(func, '\n');
        printf("pair=");
        object_print(pair, '\n');
    }
#endif
    int is_error = 0;
    object_t* result = object_create(
        TYPE_PAIR, (object_data_t) { .pair = (pair_t) { NULL, NULL } }
    );
    dynarr_t arg_pair_stack = dynarr_new(sizeof(object_t*));
    dynarr_t res_pair_stack = dynarr_new(sizeof(object_t*));
    append(&arg_pair_stack, &pair);
    append(&res_pair_stack, &result);

    while (arg_pair_stack.size > 0) {
        object_t* arg_pair = *(object_t**)back(&arg_pair_stack);
        object_t* res_pair = *(object_t**)back(&res_pair_stack);
        object_t* arg_left = arg_pair->data.pair.left;
        object_t* arg_right = arg_pair->data.pair.right;
        object_t** res_left = &res_pair->data.pair.left;
        object_t** res_right = &res_pair->data.pair.right;
        int process_result_enum;

        /* use ERROR_OBJ to indecate a child is removed */
        /* check */
        if (arg_left == NULL || arg_right == NULL) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_filter: bad pair - left or right is empty: ");
                object_print(arg_pair, '\n');
            }
#endif
            is_error = 1;
            break;
        }
        /* left */
        process_result_enum
            = filter_process_node(context, pos, func, arg_left, res_left);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_filter: a left child return with error: ");
                object_print(result, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_left);
            append(&res_pair_stack, res_left);
            continue;
        }
        /* right */
        process_result_enum
            = filter_process_node(context, pos, func, arg_right, res_right);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_filter: a right child return with error: ");
                object_print(result, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_right);
            append(&res_pair_stack, res_right);
            continue;
        }
        /* merge */
#ifdef ENABLE_DEBUG_LOG
        *res_left = filter_merge(*res_left, global_is_enable_debug_log);
        *res_right = filter_merge(*res_right, global_is_enable_debug_log);
        if (global_is_enable_debug_log) {
            printf("exec_filter: pair=");
            object_print(arg_pair, '\n');
            printf("exec_filter: res_obj=");
            object_print(res_pair, '\n');
        }
#else
        *res_left = filter_merge(*res_left);
        *res_right = filter_merge(*res_right);
#endif

        pop(&arg_pair_stack);
        pop(&res_pair_stack);
    }
    dynarr_free(&arg_pair_stack);
    dynarr_free(&res_pair_stack);

    if (is_error) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("exec_filter: error\n");
        }
#endif
        object_free(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }

    /* final merge */
    if (result->type == TYPE_PAIR) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("exec_filter: do final merge on ");
            object_print(result, '\n');
        }
        result = filter_merge(result, global_is_enable_debug_log);
#else
        result = filter_merge(result);
#endif
    }
    /* if root is removed: return null object */
    if (result->is_error) {
        result = (object_t*)NULL_OBJECT_PTR;
    }
    return result;
}

int
reduce_process_node(object_t* arg, object_t** res)
{
    if (arg->type == TYPE_PAIR) {
        if (*res == NULL) {
            *res = object_create(
                TYPE_PAIR, (object_data_t) { .pair = (pair_t) { NULL, NULL } }
            );
            return NEW_PAIR;
        }
        return VISITED_PAIR;
    }
    if (*res == NULL) {
        *res = object_copy(arg);
        return NEW_LEAF;
    }
    return VISITED_LEAF;
}

/* apply function (taking a pair) recursively in postfix order to the pair */
object_t*
exec_reduce(
    context_t context, linecol_t token_pos, const object_t* func, object_t* pair
)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_reduce\n");
        printf("func=");
        object_print(func, '\n');
        printf("pair=");
        object_print(pair, '\n');
    }
#endif
    int is_error = 0;
    object_t* result = object_create(
        TYPE_PAIR, (object_data_t) { .pair = (pair_t) { NULL, NULL } }
    );
    dynarr_t arg_pair_stack, res_pair_stack;
    arg_pair_stack = dynarr_new(sizeof(object_t*));
    res_pair_stack = dynarr_new(sizeof(object_t*));
    append(&arg_pair_stack, &pair);
    append(&res_pair_stack, &result);
    while (arg_pair_stack.size > 0) {
        object_t* arg_pair = *(object_t**)back(&arg_pair_stack);
        object_t* res_pair = *(object_t**)back(&res_pair_stack);
        object_t* arg_left = arg_pair->data.pair.left;
        object_t* arg_right = arg_pair->data.pair.right;
        object_t** res_left = &res_pair->data.pair.left;
        object_t** res_right = &res_pair->data.pair.right;
        int process_result_enum;

        /* check */
        if (arg_left == NULL || arg_right == NULL) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: bad pair - left or right is empty: ");
                object_print(arg_pair, '\n');
            }
#endif
            is_error = 1;
            break;
        }
        /* left */
        process_result_enum = reduce_process_node(arg_left, res_left);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: a left child return with error: ");
                object_print(*res_left, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_left);
            append(&res_pair_stack, res_left);
            continue;
        }
        /* right */
        process_result_enum = reduce_process_node(arg_right, res_right);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: a right child return with error: ");
                object_print(*res_right, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_right);
            append(&res_pair_stack, res_right);
            continue;
        }
        /* reduce */
        object_t* reduce_result = exec_call(context, token_pos, func, res_pair);
        if (reduce_result->is_error) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: a child return with error: ");
                object_print(reduce_result, '\n');
            }
#endif
            is_error = 1;
            object_free(reduce_result);
            object_free(res_pair);
            break;
        }
        /* after reduce:
           1. de-ref res_left and res_right from res_pair because we only want
              keep the result of the reduce function
        */
        object_free(*res_left);
        object_free(*res_right);
        /* 2. replace res_pair with reduce_result so its parent can use it */
        *res_pair = *reduce_result;
        /* 3. free the space of reduce_result */
        object_free(reduce_result);

        pop(&arg_pair_stack);
        pop(&res_pair_stack);
    }
    if (is_error) {
        object_free(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }
    dynarr_free(&arg_pair_stack);
    dynarr_free(&res_pair_stack);
    return result;
}

static inline object_t*
exec_op(
    context_t context, token_t op_token, object_t* left_obj, object_t* right_obj
)
{
    object_t* tmp_obj;
    switch (op_token.name) {
    case OP_FCALL:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        };
        return exec_call(context, op_token.pos, left_obj, right_obj);
    case OP_MAP:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return exec_map(context, op_token.pos, left_obj, right_obj);
    case OP_FILTER:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return exec_filter(context, op_token.pos, left_obj, right_obj);
    case OP_REDUCE:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return exec_reduce(context, op_token.pos, left_obj, right_obj);
        /* case OP_POS: */
        /* OP_POS would be discarded in tree parser */
    case OP_NEG:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        tmp_obj = object_create(left_obj->type, left_obj->data);
        tmp_obj->data.number.numer.sign = !tmp_obj->data.number.numer.sign;
        return tmp_obj;
    case OP_NOT:
        if (is_bad_type(op_token, TYPE_ANY, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)(object_to_bool(left_obj) ? ONE_NUMBER : ZERO_NUMBER)
        );
    case OP_CEIL:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM, (object_data_t)number_ceil(&left_obj->data.number)
        );
    case OP_FLOOR:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM, (object_data_t)number_floor(&left_obj->data.number)
        );
    case OP_GETL:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_copy(left_obj->data.pair.left);
    case OP_GETR:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_copy(left_obj->data.pair.right);
    case OP_EXP:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (right_obj->data.number.denom.size != 1
            || right_obj->data.number.denom.digit[0] != 1) {
            print_runtime_error(op_token.pos, "Exponent must be integer");
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t
            )number_exp(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_MUL:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t
            )number_mul(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_DIV:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (right_obj->data.number.numer.size == 0) {
            print_runtime_error(op_token.pos, "Divided by zero");
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t
            )number_div(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_MOD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t
            )number_mod(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_ADD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t
            )number_add(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_SUB:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t
            )number_sub(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_LT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(
                number_lt(&left_obj->data.number, &right_obj->data.number)
            )
        );
    case OP_LE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(
                !number_lt(&right_obj->data.number, &left_obj->data.number)
            )
        );
    case OP_GT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(
                number_lt(&right_obj->data.number, &left_obj->data.number)
            )
        );
    case OP_GE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(
                !number_lt(&left_obj->data.number, &right_obj->data.number)
            )
        );
    case OP_EQ:
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(object_eq(left_obj, right_obj))
        );
    case OP_NE:
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(!object_eq(left_obj, right_obj))
        );
    case OP_AND:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(
                object_to_bool(left_obj) && object_to_bool(right_obj)
            )
        );
    case OP_OR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)number_from_i32(
                object_to_bool(left_obj) || object_to_bool(right_obj)
            )
        );
    case OP_PAIR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_PAIR,
            (object_data_t)(pair_t) {
                .left = object_copy(left_obj),
                .right = object_copy(right_obj),
            }
        );
    case OP_FCALLR:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return exec_call(context, op_token.pos, left_obj, right_obj);
    case OP_CONDFCALL:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        /* check inside the pair */
        {
            token_t tmp_op_token = (token_t) {
                .name = OP_CONDFCALL,
                .pos = op_token.pos,
                .type = TOK_OP,
                .str = "the members in the conditional function caller",
            };
            object_t* right_left = right_obj->data.pair.left;
            object_t* right_right = right_obj->data.pair.right;
            if (is_bad_type(
                    tmp_op_token, TYPE_CALL, TYPE_CALL, right_left, right_right
                )) {
                return (object_t*)ERR_OBJECT_PTR;
            }
            return exec_call(
                context, op_token.pos,
                (object_to_bool(left_obj) ? right_left : right_right),
                (object_t*)&RESERVED_OBJS[RESERVED_ID_NAME_NULL]
            );
        }
    default:
        print_runtime_error(op_token.pos, "exec_op: bad op name");
        return (object_t*)ERR_OBJECT_PTR;
    }
}

object_t*
eval(context_t context, const int entry_index)
{
    frame_t* cur_frame = context.cur_frame;
    const tree_t* tree = context.tree;
    const token_t* tree_tokens = tree->tokens.data;
    const int tree_size = tree->sizes[entry_index];
    /* obj_table_offset = entry_index - tree_size + 1
       because tree tokens are order in postfix */
    const int obj_table_offset = entry_index - tree_size + 1;

    dynarr_t token_index_stack; /* type: int */
    object_t** obj_table;

    int i, is_error = 0;
    object_t* result;

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval\n");
        printf("entry_index=%d cur_frame=%p ", entry_index, cur_frame);
        printf("obj_table_offset = %d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
    }
#endif

    /* obj_table store shallow copyed intermidiate results */
    obj_table = calloc(tree_size, sizeof(object_t*));
#define OBJ_TABLE(index) (obj_table[index - obj_table_offset])

    /* begin evaluation */
    token_index_stack = dynarr_new(sizeof(int));
    append(&token_index_stack, &entry_index);
    while (token_index_stack.size > 0) {
        int cur_index = *(int*)back(&token_index_stack),
            left_index = tree->lefts[cur_index],
            right_index = tree->rights[cur_index];
        token_t cur_token = tree_tokens[cur_index];
        const token_t* left_token
            = (left_index < 0) ? NULL : &tree_tokens[left_index];
        object_t *left_obj = (left_index < 0) ? NULL : OBJ_TABLE(left_index),
                 *right_obj = (right_index < 0) ? NULL : OBJ_TABLE(right_index);
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("> (node %d) ", cur_index);
            token_print(cur_token);
            printf(
                " (local=%d) left=%d right=%d\n", cur_index - obj_table_offset,
                left_index, right_index
            );
            fflush(stdout);
        }
#endif

        if (cur_token.type == TOK_ID) {
            object_t* o = frame_get(cur_frame, cur_token.name);
            if (o == NULL) {
                const char* err_msg = "Identifier '%s' used uninitialized";
                sprintf(ERR_MSG_BUF, err_msg, cur_token.str);
                print_runtime_error(cur_token.pos, ERR_MSG_BUF);
                is_error = 1;
                break;
            }
            /* copy object from frame */
            OBJ_TABLE(cur_index) = object_copy(o);
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(
                    "  get identifiter '%s' (name=%d) from frame (addr=%p): ",
                    cur_token.str, cur_token.name, o
                );
                object_print(o, '\n');
            }
#endif
        } else if (cur_token.type == TOK_NUM) {
            /* copy object from tree */
            OBJ_TABLE(cur_index) = object_copy(tree->literals[cur_index]);
            /* not id or num or op: error */
        } else if (cur_token.type != TOK_OP) {
            printf("eval: bad token type: %d\n", cur_token.type);
            exit(RUNTIME_ERR_CODE);
            /* function and macro maker */
        } else if (cur_token.name == OP_FMAKE || cur_token.name == OP_MMAKE) {
            int is_macro = cur_token.name == OP_MMAKE;
            /* function will owns a deep copy of the frame it created under */
            frame_t* init_time_frame = is_macro ? NULL : frame_copy(cur_frame);
            OBJ_TABLE(cur_index) = object_create(
                TYPE_CALL,
                (object_data_t)(callable_t) {
                    .is_macro = is_macro,
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_name = -1,
                    .index = left_index,
                    .init_time_frame = init_time_frame,
                }
            );
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(is_macro ? " make function:" : " make macro:");
                object_print(OBJ_TABLE(cur_index), '\n');
            }
#endif
            /* argument binder */
        } else if (cur_token.name == OP_ARG) {
            if (right_obj == NULL) {
                append(&token_index_stack, &tree->rights[cur_index]);
                continue;
            }
            if (right_obj->type != TYPE_CALL
                || right_obj->data.callable.is_macro) {
                const char* err_msg
                    = "Right side of argument binder should be function";
                print_runtime_error(cur_token.pos, err_msg);
                is_error = 1;
                break;
            }
            if (right_obj->data.callable.arg_name != -1) {
                const char* err_msg
                    = "Bind argument to a function that already has one";
                print_runtime_error(cur_token.pos, err_msg);
                is_error = 1;
                break;
            }
            /* move right to current and modify arg_name */
            right_obj->data.callable.arg_name = left_token->name;
            OBJ_TABLE(cur_index) = right_obj;
            OBJ_TABLE(right_index) = NULL;
        } else if (cur_token.name == OP_ASSIGN) {
            /* assignment */
            if (right_obj == NULL) {
                append(&token_index_stack, &right_index);
                continue;
            }
            if (frame_get(cur_frame, left_token->name) != NULL) {
                const char* err_msg
                    = "Repeated initialization of identifier '%s'";
                sprintf(ERR_MSG_BUF, err_msg, left_token->str);
                print_runtime_error(left_token->pos, ERR_MSG_BUF);
                is_error = 1;
                break;
            }
            /* set frame */
            frame_set(cur_frame, left_token->name, right_obj);
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("initialized identifier '%s' to ", left_token->str);
                object_print(right_obj, '\n');
            }
#endif
            /* move right to current */
            OBJ_TABLE(cur_index) = right_obj;
            OBJ_TABLE(right_index) = NULL;
            /* condition-and or condition-or */
        } else if (cur_token.name == OP_CONDAND
                   || cur_token.name == OP_CONDOR) {
            /* eval left first and eval right conditionally */
            if (left_obj == NULL) {
                append(&token_index_stack, &left_index);
                continue;
            }
            /* is_left_true | is_condor | is_eval_right
               F            | F         | F
               F            | T         | T
               T            | F         | T
               T            | T         | F
            */
            if (object_to_bool(left_obj) != (cur_token.name == OP_CONDOR)) {
                if (right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    continue;
                }
                /* move right to current */
                OBJ_TABLE(cur_index) = right_obj;
                OBJ_TABLE(right_index) = NULL;
            } else {
                /* move left to current */
                OBJ_TABLE(cur_index) = left_obj;
                OBJ_TABLE(left_index) = NULL;
            }
        } else if (cur_token.name == OP_EXPRSEP) {
            /* expresion seperator */
            if (left_obj == NULL && right_obj == NULL) {
                append(&token_index_stack, &right_index);
                append(&token_index_stack, &left_index);
                continue;
            }
            /* move right to current */
            OBJ_TABLE(cur_index) = right_obj;
            OBJ_TABLE(right_index) = NULL;
            /* left is out of reach so free it immediately */
            object_free(OBJ_TABLE(left_index));
            OBJ_TABLE(left_index) = NULL;
        } else {
            /* other operator */

            if (is_unary_op(cur_token.name) && left_obj == NULL) {
                append(&token_index_stack, &left_index);
                continue;
            } else if (left_obj == NULL && right_obj == NULL) {
                append(&token_index_stack, &right_index);
                append(&token_index_stack, &left_index);
                continue;
            }
            result = exec_op(context, cur_token, left_obj, right_obj);
            if (result->is_error) {
                const char* err_msg = "operator \"%s\" returns with error";
                sprintf(ERR_MSG_BUF, err_msg, OP_STRS[cur_token.name]);
                print_runtime_error(cur_token.pos, ERR_MSG_BUF);
            }
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(
                    "^ (node %d) exec_op %s returned\n", cur_index,
                    OP_STRS[cur_token.name]
                );
            }
#endif
            /* end eval if error */
            if (result->is_error) {
                is_error = 1;
                break;
            }
            /* don't need to copy cause it is created or newed in exec_op */
            OBJ_TABLE(cur_index) = result;
        }
        /* end if-else */
        if (is_error) {
            break; /* break the while loop */
        }
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("< (node %d) eval result: ", cur_index);
            object_print(OBJ_TABLE(cur_index), '\n');
            fflush(stdout);
        }
#endif
        pop(&token_index_stack);
    } /* end while loop */

    /* prepare return object */
    if (is_error) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("eval return with error\n");
        }
#endif
        result = (object_t*)ERR_OBJECT_PTR;
    } else {
        result = object_copy(OBJ_TABLE(entry_index));
    }

    /* free things */
    dynarr_free(&token_index_stack);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("free obj_tables\n");
        fflush(stdout);
    }
#endif
    for (i = 0; i < tree_size; i++) {
        if (obj_table[i] != NULL) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("free [%d] addr: %p:", i, obj_table[i]);
                object_print(obj_table[i], '\n');
                fflush(stdout);
            }
#endif
            object_free(obj_table[i]);
        }
    }
    free(obj_table);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval returned ");
        object_print(result, '\n');
        fflush(stdout);
    }
#endif
    return result;
}
