#include "builtin_funcs.h"
#include "dynarr.h"
#include "errormsg.h"
#include "eval_tree.h"
#include "frame.h"
#include "lreng.h"
#include "token_tree.h"
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
    if (left_passed && right_passed) {
        return 0;
    }
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



static inline object_t*
exec_call(context_t context, linecol_t pos, const object_t* func, object_t* arg)
{
    object_t* result;
    frame_t* caller_frame = context.cur_frame;
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
    frame_t* callee_frame;
    callable_t callable = func->data.callable;
    if (callable.is_macro) {
        callee_frame = caller_frame;
    } else {
        int arg_name = callable.arg_name;
        callee_frame = frame_call_with_closure(caller_frame, func);
        /* set argument to call-frame */
        if (arg_name != -1 && frame_set(callee_frame, arg_name, arg) == NULL) {
            int arg_token_index = context.tree->lefts[callable.index];
            const char* arg_str
                = ((token_t*)at(&context.tree->tokens, arg_token_index))->str;
            const char* err_msg
                = "Failed initialization of function argument '%s'";
            sprintf(ERR_MSG_BUF, err_msg, arg_str);
            print_runtime_error(pos, ERR_MSG_BUF);
        }
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_call: callee_frame=%p\nfunc_obj=", callee_frame);
        object_print(func, '\n');
        printf("arg=");
        object_print(arg, '\n');
    }
#endif
    /* eval from function's entry index */
    context_t new_context = {
        .tree = context.tree,
        .cur_frame = callee_frame,
        .call_depth = context.call_depth + 1,
    };
    result = eval(new_context, callable.index);
    if (!callable.is_macro) {
        /* free the objects own by this function call */
        frame_return(callee_frame);
        /* free the rest of stack */
        frame_free_stack(callee_frame);
        free(callee_frame);
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
            object_deref(result);
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
        object_deref(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }
    dynarr_free(&arg_pair_stack);
    dynarr_free(&res_pair_stack);
    return result;
}

const object_t FILTER_EMPTY_OBJECT = ((object_t) {
    .is_error = 0,
    .is_const = 1,
    .type = TYPE_NULL,
    .ref_count = 0,
    .data = (object_data_t)(0x00454d505459ULL),
    /* this magic value is just the string "EMPTY" */
});

object_t*
filter_merge(object_t* pair_to_merge)
{
    if (pair_to_merge->type != TYPE_PAIR) {
        return pair_to_merge;
    }
    object_t* right = pair_to_merge->data.pair.right;
    object_t* left = pair_to_merge->data.pair.left;
    if (left == &FILTER_EMPTY_OBJECT && right == &FILTER_EMPTY_OBJECT) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("filter_merge: pair removed\n");
        }
#endif
        object_deref(pair_to_merge);
        return (object_t*)&FILTER_EMPTY_OBJECT;
    } else if (left == &FILTER_EMPTY_OBJECT) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("filter_merge: pair replaced by its right\n");
        }
#endif
        /* free parent but not a child */
        object_t* right_copy = object_ref(right);
        object_deref(pair_to_merge);
        return right_copy;
    } else if (right == &FILTER_EMPTY_OBJECT) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("filter_merge: pair replaced by its left\n");
        }
#endif
        /* free parent but not a child */
        object_t* left_copy = object_ref(left);
        object_deref(pair_to_merge);
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
            object_deref(result);
            return ERROR;
        }
        if (object_to_bool(result)) {
            /* keep */
            *res = object_ref(arg);
        } else {
            /* removed */
            *res = (object_t*)&FILTER_EMPTY_OBJECT;
        }
        object_deref(result);
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
        object_t* arg_left = arg_pair->data.pair.left;
        object_t* arg_right = arg_pair->data.pair.right;

        object_t* res_pair = *(object_t**)back(&res_pair_stack);
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
        *res_left = filter_merge(*res_left);
        *res_right = filter_merge(*res_right);
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("exec_filter: pair=");
            object_print(arg_pair, '\n');
            printf("exec_filter: res_obj=");
            object_print(res_pair, '\n');
        }
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
        object_deref(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }

    /* final merge */
    if (result->type == TYPE_PAIR) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("exec_filter: do final merge on ");
            object_print(result, '\n');
        }
#endif
        result = filter_merge(result);
    }
    /* if root is removed: return null object */
    if (result == &FILTER_EMPTY_OBJECT) {
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
        *res = object_ref(arg);
        return NEW_LEAF;
    }
    return VISITED_LEAF;
}

/* apply function (taking a pair) recursively in postfix order to the pair
 */
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
    object_t** input_pair_ptr = &pair;
    object_t** result_ptr = &result;
    dynarr_t arg_pair_stack, res_pair_stack;
    arg_pair_stack = dynarr_new(sizeof(object_t**));
    res_pair_stack = dynarr_new(sizeof(object_t**));
    append(&arg_pair_stack, &input_pair_ptr);
    append(&res_pair_stack, &result_ptr);
    while (arg_pair_stack.size > 0) {
        object_t** arg_pair_ptr = *(object_t***)back(&arg_pair_stack);
        object_t** arg_left_ptr = &((*arg_pair_ptr)->data.pair.left);
        object_t** arg_right_ptr = &((*arg_pair_ptr)->data.pair.right);
        object_t* arg_left = *arg_left_ptr;
        object_t* arg_right = *arg_right_ptr;

        object_t** res_pair_ptr = *(object_t***)back(&res_pair_stack);
        object_t** res_left_ptr = &((*res_pair_ptr)->data.pair.left);
        object_t** res_right_ptr = &((*res_pair_ptr)->data.pair.right);

        int process_result_enum;

        /* check */
        if (arg_left == NULL || arg_right == NULL) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: bad pair - left or right is empty: ");
                object_print(*arg_pair_ptr, '\n');
            }
#endif
            is_error = 1;
            break;
        }
        /* left */
        process_result_enum = reduce_process_node(arg_left, res_left_ptr);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: a left child return with error: ");
                object_print(*res_left_ptr, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_left_ptr);
            append(&res_pair_stack, &res_left_ptr);
            continue;
        }
        /* right */
        process_result_enum = reduce_process_node(arg_right, res_right_ptr);
        if (process_result_enum == ERROR) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: a right child return with error: ");
                object_print(*res_right_ptr, '\n');
            }
#endif
            is_error = 1;
            break;
        } else if (process_result_enum == NEW_PAIR) {
            append(&arg_pair_stack, &arg_right_ptr);
            append(&res_pair_stack, &res_right_ptr);
            continue;
        }
        /* reduce */
        object_t* reduce_result
            = exec_call(context, token_pos, func, *res_pair_ptr);
        if (reduce_result->is_error) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("exec_reduce: a child return with error: ");
                object_print(reduce_result, '\n');
            }
#endif
            is_error = 1;
            object_deref(reduce_result);
            break;
        }
        /* after reduce, we want to use res_pair_ptr's space to store result so
           that its parent can use it:
           1. de-ref the original result pair
        */
        object_deref(*res_pair_ptr);
        /* 2. point the res_pair_ptr to reduce_result
           don't need to increase ref because reduce_result is temp pointer
        */
        *res_pair_ptr = reduce_result;

        pop(&arg_pair_stack);
        pop(&res_pair_stack);
    }
    if (is_error) {
        object_deref(result);
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
        {
            number_t neg_number = EMPTY_NUMBER;
            number_copy(&neg_number, &left_obj->data.number);
            neg_number.numer.sign = !neg_number.numer.sign;
            tmp_obj = object_create(left_obj->type, (object_data_t)neg_number);
        }
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
        return object_ref(left_obj->data.pair.left);
    case OP_GETR:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_ref(left_obj->data.pair.right);
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
            (object_data_t)
                number_exp(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_MUL:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)
                number_mul(&left_obj->data.number, &right_obj->data.number)
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
            (object_data_t)
                number_div(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_MOD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)
                number_mod(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_ADD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)
                number_add(&left_obj->data.number, &right_obj->data.number)
        );
    case OP_SUB:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_t)
                number_sub(&left_obj->data.number, &right_obj->data.number)
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
                .left = object_ref(left_obj),
                .right = object_ref(right_obj),
            }
        );
    case OP_FCALLR:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return exec_call(context, op_token.pos, left_obj, right_obj);
    case OP_CONDPCALL:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        /* check inside the pair */
        {
            token_t tmp_op_token = (token_t) {
                .name = OP_CONDPCALL,
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
        sprintf(ERR_MSG_BUF, "exec_op: bad op name: %d", op_token.name);
        print_runtime_error(op_token.pos, ERR_MSG_BUF);
        return (object_t*)ERR_OBJECT_PTR;
    }
}

static inline eval_tree_t*
eval_tree_node_create(int token_index)
{
    eval_tree_t* node = calloc(1, sizeof(eval_tree_t));
    node->token_index = token_index;
    return node;
}

/* recursively delete a node and its children, deref the objects they hold */
void
eval_tree_node_free(eval_tree_t* node)
{
#ifdef ENABLE_DEBUG_MORE_LOG
    printf("eval_tree_node_free: freeing node %p and object ", node);
    if (node->object) {
        object_print(node->object, '\n');
    } else {
        printf("nullptr\n");
    }
    fflush(stdout);
#endif
    if (node->left) {
        eval_tree_node_free(node->left);
    }
    if (node->right) {
        eval_tree_node_free(node->right);
    }
    if (node->object) {
        object_deref(node->object);
    }
    free(node);
}

object_t*
eval(context_t context, const int entry_index)
{
    frame_t* cur_frame = context.cur_frame;
    const token_tree_t* token_tree = context.tree;
    const token_t* tokens = token_tree->tokens.data;
    if (context.call_depth > 1000) {
        print_runtime_error(
            tokens[entry_index].pos, "Call stack too deep (> 1000)"
        );
        exit(RUNTIME_ERR_CODE);
    }

#ifdef ENABLE_DEBUG_LOG
    const int tree_size = token_tree->sizes[entry_index];
    const int obj_table_offset = entry_index - tree_size + 1;
#endif

    int is_error = 0;
    object_t* result = NULL;

    /* type: eval_tree_node* */
    dynarr_t eval_node_stack = dynarr_new(sizeof(eval_tree_t*));
    eval_tree_t* root_eval_node = eval_tree_node_create(entry_index);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval\n");
        printf("entry_index=%d caller_frame=%p ", entry_index, cur_frame);
        printf("stack depth=%d ", context.call_depth);
        printf("obj_table_offset=%d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
    }
#endif

    /* begin evaluation */
    append(&eval_node_stack, &root_eval_node);
    while (eval_node_stack.size > 0) {
        eval_tree_t* cur_eval_node = *(eval_tree_t**)back(&eval_node_stack);
        int cur_index = cur_eval_node->token_index,
            left_index = token_tree->lefts[cur_index],
            right_index = token_tree->rights[cur_index];
        const token_t cur_token = tokens[cur_eval_node->token_index];

        object_t* left_obj
            = (cur_eval_node->left) ? cur_eval_node->left->object : NULL;
        object_t* right_obj
            = (cur_eval_node->right) ? cur_eval_node->right->object : NULL;
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("> (node %d) ", cur_index);
            token_print(&cur_token);
            printf(
                " (local=%d) left=%d right=%d\n", cur_index - obj_table_offset,
                left_index, right_index
            );
            fflush(stdout);
        }
#endif

        if (token_tree->literals[cur_index]) {
            /* if it is a literal, use it */
            cur_eval_node->object = object_ref(token_tree->literals[cur_index]);
        } else if (cur_token.type == TOK_ID) {
            object_t* o = frame_get(cur_frame, cur_token.name);
            if (o == NULL) {
                const char* err_msg = "Identifier '%s' used uninitialized";
                sprintf(ERR_MSG_BUF, err_msg, cur_token.str);
                print_runtime_error(cur_token.pos, ERR_MSG_BUF);
                is_error = 1;
                break;
            }
            /* copy object from frame */
            cur_eval_node->object = object_ref(o);
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
            /* we are sure numbers are all pre-evaled when creating token tree
               so this line will almost never run, but just keep it in case
            */
            cur_eval_node->object = object_ref(token_tree->literals[cur_index]);
        } else if (cur_token.type != TOK_OP) {
            /* not id or num or op: error */
            sprintf(ERR_MSG_BUF, "eval: bad token type: %d\n", cur_token.type);
            print_runtime_error(tokens[cur_index].pos, ERR_MSG_BUF);
            exit(RUNTIME_ERR_CODE);
        } else if (cur_token.name == OP_FMAKE || cur_token.name == OP_MMAKE) {
            /* function and macro maker */
            int is_macro = cur_token.name == OP_MMAKE;
            /* function will owns a deep copy of the frame it created under */
            frame_t* init_time_frame = is_macro ? NULL : frame_copy(cur_frame);
            cur_eval_node->object = object_create(
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
                object_print(cur_eval_node->object, '\n');
            }
#endif
        } else if (cur_token.name == OP_ARG) {
            /* argument binder */
            if (right_obj == NULL) {
                cur_eval_node->right = eval_tree_node_create(right_index);
                append(&eval_node_stack, &cur_eval_node->right);
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
            /* move right object to current and free right sub-tree */
            cur_eval_node->object = right_obj;
            cur_eval_node->right->object = NULL;
            eval_tree_node_free(cur_eval_node->right);
            cur_eval_node->right = NULL;
            /* set arg_name */
            cur_eval_node->object->data.callable.arg_name
                = tokens[left_index].name;
        } else if (cur_token.name == OP_ASSIGN) {
            /* assignment */
            if (right_obj == NULL) {
                cur_eval_node->right = eval_tree_node_create(right_index);
                append(&eval_node_stack, &cur_eval_node->right);
                continue;
            }
            /* set frame */
            {
                const token_t left_token = tokens[left_index];
                if (frame_set(cur_frame, left_token.name, right_obj) == NULL) {
                    const char* err_msg
                        = "Repeated initialization of identifier '%s'";
                    sprintf(ERR_MSG_BUF, err_msg, left_token.str);
                    print_runtime_error(left_token.pos, ERR_MSG_BUF);
                    is_error = 1;
                    break;
                }
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf("initialized identifier '%s' to ", left_token.str);
                    object_print(right_obj, '\n');
                }
#endif
            }
            /* move right object to current and free the right sub-tree */
            cur_eval_node->object = right_obj;
            cur_eval_node->right->object = NULL;
            eval_tree_node_free(cur_eval_node->right);
            cur_eval_node->right = NULL;
        } else if (cur_token.name == OP_CONDAND
                   || cur_token.name == OP_CONDOR) {
            /* condition-and or condition-or */
            /* eval left first and eval right conditionally */
            if (left_obj == NULL) {
                cur_eval_node->left = eval_tree_node_create(left_index);
                append(&eval_node_stack, &cur_eval_node->left);
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
                    cur_eval_node->right = eval_tree_node_create(right_index);
                    append(&eval_node_stack, &cur_eval_node->right);
                    continue;
                }
                /* move right object to current */
                cur_eval_node->object = right_obj;
                cur_eval_node->right->object = NULL;
                /* free the both sub-trees */
                eval_tree_node_free(cur_eval_node->left);
                cur_eval_node->left = NULL;
                eval_tree_node_free(cur_eval_node->right);
                cur_eval_node->right = NULL;
            } else {
                /* move left object to current and free the left sub-tree */
                cur_eval_node->object = left_obj;
                cur_eval_node->left->object = NULL;
                eval_tree_node_free(cur_eval_node->left);
                cur_eval_node->left = NULL;
            }
        } else if (cur_token.name == OP_EXPRSEP) {
            /* expresion seperator */
            if (left_obj == NULL && right_obj == NULL) {
                cur_eval_node->right = eval_tree_node_create(right_index);
                append(&eval_node_stack, &cur_eval_node->right);
                cur_eval_node->left = eval_tree_node_create(left_index);
                append(&eval_node_stack, &cur_eval_node->left);
                continue;
            }
            /* move right object to current */
            cur_eval_node->object = right_obj;
            cur_eval_node->right->object = NULL;
            /* free the both sub-tree */
            eval_tree_node_free(cur_eval_node->right);
            cur_eval_node->right = NULL;
            eval_tree_node_free(cur_eval_node->left);
            cur_eval_node->left = NULL;
        } else {
            /* other operator */
            if (is_unary_op(cur_token.name) && left_obj == NULL) {
                cur_eval_node->left = eval_tree_node_create(left_index);
                append(&eval_node_stack, &cur_eval_node->left);
                continue;
            } else if (left_obj == NULL && right_obj == NULL) {
                cur_eval_node->right = eval_tree_node_create(right_index);
                append(&eval_node_stack, &cur_eval_node->right);
                cur_eval_node->left = eval_tree_node_create(left_index);
                append(&eval_node_stack, &cur_eval_node->left);
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
            /* don't need to copy cause it is created or refed in exec_op */
            cur_eval_node->object = result;
            result = NULL;
        }
        /* end if-else */
        if (is_error) {
            break; /* break the while loop */
        }
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("< (node %d) eval result: ", cur_index);
            object_print(cur_eval_node->object, '\n');
            fflush(stdout);
        }
#endif
        pop(&eval_node_stack);
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
        result = object_ref(root_eval_node->object);
    }

    /* free things */
    dynarr_free(&eval_node_stack);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("free eval tree\n");
        fflush(stdout);
    }
#endif
    /* free the eval_tree */
    eval_tree_node_free(root_eval_node);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval returned ");
        object_print(result, '\n');
        fflush(stdout);
    }
#endif
    return result;
}
