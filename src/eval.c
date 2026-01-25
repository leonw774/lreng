#include "builtin_funcs.h"
#include "errormsg.h"
#include "eval_tree.h"
#include "frame.h"
#include "main.h"
#include "token_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TYPE eval_tree_t*
#define TYPE_NAME eval_tree_ptr
#include "dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

#define TYPE object_t*
#define TYPE_NAME object_ptr
#include "dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

#define TYPE object_t**
#define TYPE_NAME object_ptr_ptr
#include "dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

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
exec_call(context_t context, linecol_t pos, const object_t* call, object_t* arg)
{
    object_t* result;
    frame_t* caller_frame = context.cur_frame;
    /* if is builtin */
    if (call->as.callable.builtin_name != -1) {
        object_t* (*func_ptr)(const object_t*)
            = BUILDTIN_FUNC_ARRAY[call->as.callable.builtin_name];
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
    callable_t callable = call->as.callable;
    if (callable.is_macro) {
        callee_frame = caller_frame;
    } else {
        int arg_name = callable.arg_name;
        callee_frame = frame_call_with_closure(caller_frame, call);
        /* set argument to call-frame */
        if (arg_name != -1 && frame_set(callee_frame, arg_name, arg) == NULL) {
            int arg_token_index = context.tree->lefts[callable.index];
            const char* arg_str
                = dynarr_token_at(&context.tree->tokens, arg_token_index)->str;
            const char* err_msg
                = "Failed initialization of function argument '%s'";
            sprintf(ERR_MSG_BUF, err_msg, arg_str);
            print_runtime_error(pos, ERR_MSG_BUF);
        }
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_call: callee_frame=%p\nfunc_obj=", callee_frame);
        object_print(call, '\n');
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
        /* free the stacks */
        frame_free(callee_frame);
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
    context_t context, linecol_t pos, const object_t* call, object_t* arg,
    object_t** res
)
{
    if (arg->type == TYPE_PAIR) {
        if (*res == NULL) {
            *res = object_create(
                TYPE_PAIR,
                (object_data_union) { .pair = (pair_t) { NULL, NULL } }
            );
            return NEW_PAIR;
        }
        return VISITED_PAIR;
    }
    if (*res == NULL) {
        object_t* result = exec_call(context, pos, call, arg);
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

/* apply callable recursively in postfix order and return a new pair */
object_t*
exec_map(context_t context, linecol_t pos, const object_t* call, object_t* pair)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_map\n");
        printf("call=");
        object_print(call, '\n');
        printf("pair=");
        object_print(pair, '\n');
    }
#endif
    int is_error = 0;
    object_t* result = object_create(
        TYPE_PAIR, (object_data_union) { .pair = (pair_t) { NULL, NULL } }
    );
    dynarr_object_ptr_t arg_pair_stack = dynarr_object_ptr_new();
    dynarr_object_ptr_t res_pair_stack = dynarr_object_ptr_new();
    dynarr_object_ptr_append(&arg_pair_stack, &pair);
    dynarr_object_ptr_append(&res_pair_stack, &result);
    while (arg_pair_stack.size > 0) {
        object_t* arg_pair = *dynarr_object_ptr_back(&arg_pair_stack);
        object_t* arg_left = arg_pair->as.pair.left;
        object_t* arg_right = arg_pair->as.pair.right;

        object_t* res_pair = *dynarr_object_ptr_back(&res_pair_stack);
        object_t** res_left = &res_pair->as.pair.left;
        object_t** res_right = &res_pair->as.pair.right;
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
            = map_process_node(context, pos, call, arg_left, res_left);
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
            dynarr_object_ptr_append(&arg_pair_stack, &arg_left);
            dynarr_object_ptr_append(&res_pair_stack, res_left);
            continue;
        }
        /* right */
        process_result_enum
            = map_process_node(context, pos, call, arg_right, res_right);
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
            dynarr_object_ptr_append(&arg_pair_stack, &arg_right);
            dynarr_object_ptr_append(&res_pair_stack, res_right);
            continue;
        }

        dynarr_object_ptr_pop(&arg_pair_stack);
        dynarr_object_ptr_pop(&res_pair_stack);
    }
    if (is_error) {
        object_deref(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }
    dynarr_object_ptr_free(&arg_pair_stack);
    dynarr_object_ptr_free(&res_pair_stack);
    return result;
}

const object_t FILTER_EMPTY_OBJECT = ((object_t) {
    .is_error = 0,
    .is_const = 1,
    .type = TYPE_NULL,
    .ref_count = 0,
    .as = (object_data_union)(0x00454d505459ULL),
    /* this magic value is just the string "EMPTY" */
});

object_t*
filter_merge(object_t* pair_to_merge)
{
    if (pair_to_merge->type != TYPE_PAIR) {
        return pair_to_merge;
    }
    object_t* right = pair_to_merge->as.pair.right;
    object_t* left = pair_to_merge->as.pair.left;
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
                TYPE_PAIR,
                (object_data_union) { .pair = (pair_t) { NULL, NULL } }
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

/* apply callable recursively in postfix order to the children. if the return
   value is true, keep the child, otherwise, set it to NULL.
   if a pair's two children's return value is all false, the pair itself becomes
   NULL as well. */
object_t*
exec_filter(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_filter\n");
        printf("call=");
        object_print(call, '\n');
        printf("pair=");
        object_print(pair, '\n');
    }
#endif
    int is_error = 0;
    object_t* result = object_create(
        TYPE_PAIR, (object_data_union) { .pair = (pair_t) { NULL, NULL } }
    );
    dynarr_object_ptr_t arg_pair_stack = dynarr_object_ptr_new();
    dynarr_object_ptr_t res_pair_stack = dynarr_object_ptr_new();
    dynarr_object_ptr_append(&arg_pair_stack, &pair);
    dynarr_object_ptr_append(&res_pair_stack, &result);

    while (arg_pair_stack.size > 0) {
        object_t* arg_pair = *dynarr_object_ptr_back(&arg_pair_stack);
        object_t* arg_left = arg_pair->as.pair.left;
        object_t* arg_right = arg_pair->as.pair.right;

        object_t* res_pair = *dynarr_object_ptr_back(&res_pair_stack);
        object_t** res_left = &res_pair->as.pair.left;
        object_t** res_right = &res_pair->as.pair.right;

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
            = filter_process_node(context, pos, call, arg_left, res_left);
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
            dynarr_object_ptr_append(&arg_pair_stack, &arg_left);
            dynarr_object_ptr_append(&res_pair_stack, res_left);
            continue;
        }
        /* right */
        process_result_enum
            = filter_process_node(context, pos, call, arg_right, res_right);
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
            dynarr_object_ptr_append(&arg_pair_stack, &arg_right);
            dynarr_object_ptr_append(&res_pair_stack, res_right);
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

        dynarr_object_ptr_pop(&arg_pair_stack);
        dynarr_object_ptr_pop(&res_pair_stack);
    }
    dynarr_object_ptr_free(&arg_pair_stack);
    dynarr_object_ptr_free(&res_pair_stack);

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
                TYPE_PAIR,
                (object_data_union) { .pair = (pair_t) { NULL, NULL } }
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

/* apply callable (taking a pair) recursively in postfix order to the pair
 */
object_t*
exec_reduce(
    context_t context, linecol_t token_pos, const object_t* call, object_t* pair
)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("exec_reduce\n");
        printf("call=");
        object_print(call, '\n');
        printf("pair=");
        object_print(pair, '\n');
    }
#endif
    int is_error = 0;
    object_t* result = object_create(
        TYPE_PAIR, (object_data_union) { .pair = (pair_t) { NULL, NULL } }
    );
    object_t** input_pair_ptr = &pair;
    object_t** result_ptr = &result;
    dynarr_object_ptr_ptr_t arg_pair_stack = dynarr_object_ptr_ptr_new();
    dynarr_object_ptr_ptr_t res_pair_stack = dynarr_object_ptr_ptr_new();
    dynarr_object_ptr_ptr_append(&arg_pair_stack, &input_pair_ptr);
    dynarr_object_ptr_ptr_append(&res_pair_stack, &result_ptr);
    while (arg_pair_stack.size > 0) {
        object_t** arg_pair_ptr = *dynarr_object_ptr_ptr_back(&arg_pair_stack);
        object_t** arg_left_ptr = &((*arg_pair_ptr)->as.pair.left);
        object_t** arg_right_ptr = &((*arg_pair_ptr)->as.pair.right);
        object_t* arg_left = *arg_left_ptr;
        object_t* arg_right = *arg_right_ptr;

        object_t** res_pair_ptr = *dynarr_object_ptr_ptr_back(&res_pair_stack);
        object_t** res_left_ptr = &((*res_pair_ptr)->as.pair.left);
        object_t** res_right_ptr = &((*res_pair_ptr)->as.pair.right);

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
            dynarr_object_ptr_ptr_append(&arg_pair_stack, &arg_left_ptr);
            dynarr_object_ptr_ptr_append(&res_pair_stack, &res_left_ptr);
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
            dynarr_object_ptr_ptr_append(&arg_pair_stack, &arg_right_ptr);
            dynarr_object_ptr_ptr_append(&res_pair_stack, &res_right_ptr);
            continue;
        }
        /* reduce */
        object_t* reduce_result
            = exec_call(context, token_pos, call, *res_pair_ptr);
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

        dynarr_object_ptr_ptr_pop(&arg_pair_stack);
        dynarr_object_ptr_ptr_pop(&res_pair_stack);
    }
    if (is_error) {
        object_deref(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }
    dynarr_object_ptr_ptr_free(&arg_pair_stack);
    dynarr_object_ptr_ptr_free(&res_pair_stack);
    return result;
}

static inline object_t*
exec_op(
    const context_t context, const eval_tree_t* cur_eval_node,
    const token_t op_token, object_t* left_obj, object_t* right_obj
)
{
    switch (op_token.name) {
    case OP_FMAKE:
        return object_create(
            TYPE_CALL,
            (object_data_union)(callable_t) {
                .is_macro = 0,
                .builtin_name = NOT_BUILTIN_FUNC,
                .arg_name = -1,
                .index = context.tree->lefts[cur_eval_node->token_index],
                /* function will own a copy of the frame it created under */
                .init_frame = frame_copy(context.cur_frame),
            }
        );
    case OP_MMAKE:
        return object_create(
            TYPE_CALL,
            (object_data_union)(callable_t) {
                .is_macro = 1,
                .builtin_name = NOT_BUILTIN_FUNC,
                .arg_name = -1,
                .index = context.tree->lefts[cur_eval_node->token_index],
                /* function will own a copy of the frame it created under */
                .init_frame = NULL,
            }
        );
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
        return object_create(
            left_obj->type, (object_data_union)number_neg(&left_obj->as.number)
        );
    case OP_NOT:
        if (is_bad_type(op_token, TYPE_ANY, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)(object_to_bool(left_obj) ? ONE_NUMBER
                                                         : ZERO_NUMBER)
        );
    case OP_CEIL:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM, (object_data_union)number_ceil(&left_obj->as.number)
        );
    case OP_FLOOR:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM, (object_data_union)number_floor(&left_obj->as.number)
        );
    case OP_GETL:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_ref(left_obj->as.pair.left);
    case OP_GETR:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_ref(left_obj->as.pair.right);
    case OP_CONDCALL:
        if (left_obj->type == TYPE_CALL) {
            return exec_call(
                context, op_token.pos, left_obj,
                (object_t*)&RESERVED_OBJS[RESERVED_ID_NAME_NULL]
            );
        }
        return object_ref(left_obj);
    case OP_SWAP:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_PAIR,
            (object_data_union)(pair_t) {
                .left = object_ref(left_obj->as.pair.right),
                .right = object_ref(left_obj->as.pair.left),
            }
        );
    case OP_EXP:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (right_obj->as.number.denom.size != 1
            || right_obj->as.number.denom.digit[0] != 1) {
            print_runtime_error(op_token.pos, "Exponent must be integer");
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)
                number_exp(&left_obj->as.number, &right_obj->as.number)
        );
    case OP_MUL:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)
                number_mul(&left_obj->as.number, &right_obj->as.number)
        );
    case OP_DIV:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (right_obj->as.number.numer.size == 0) {
            print_runtime_error(op_token.pos, "Divided by zero");
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)
                number_div(&left_obj->as.number, &right_obj->as.number)
        );
    case OP_MOD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)
                number_mod(&left_obj->as.number, &right_obj->as.number)
        );
    case OP_ADD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)
                number_add(&left_obj->as.number, &right_obj->as.number)
        );
    case OP_SUB:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)
                number_sub(&left_obj->as.number, &right_obj->as.number)
        );
    case OP_LT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(
                number_lt(&left_obj->as.number, &right_obj->as.number)
            )
        );
    case OP_LE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(
                !number_lt(&right_obj->as.number, &left_obj->as.number)
            )
        );
    case OP_GT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(
                number_lt(&right_obj->as.number, &left_obj->as.number)
            )
        );
    case OP_GE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(
                !number_lt(&left_obj->as.number, &right_obj->as.number)
            )
        );
    case OP_EQ:
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(object_eq(left_obj, right_obj))
        );
    case OP_NE:
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(!object_eq(left_obj, right_obj))
        );
    case OP_AND:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(
                object_to_bool(left_obj) && object_to_bool(right_obj)
            )
        );
    case OP_OR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(
                object_to_bool(left_obj) || object_to_bool(right_obj)
            )
        );
    case OP_PAIR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_create(
            TYPE_PAIR,
            (object_data_union)(pair_t) {
                .left = object_ref(left_obj),
                .right = object_ref(right_obj),
            }
        );
    case OP_FCALLR:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return exec_call(context, op_token.pos, left_obj, right_obj);
    case OP_ARG:
        if (is_bad_type(op_token, NO_OPRAND, TYPE_CALL, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (right_obj->as.callable.is_macro) {
            const char* err_msg
                = "Right side of argument binder should be function";
            print_runtime_error(op_token.pos, err_msg);
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (right_obj->as.callable.arg_name != -1) {
            const char* err_msg
                = "Bind argument to a function that already has one";
            print_runtime_error(op_token.pos, err_msg);
            return (object_t*)ERR_OBJECT_PTR;
        }
        {
            token_t* left_token = dynarr_token_at(
                &context.tree->tokens,
                context.tree->lefts[cur_eval_node->token_index]
            );
            right_obj->as.callable.arg_name = left_token->name;
        }
        return object_ref(right_obj);
    case OP_CONDAND:
    case OP_CONDOR:
        /* if the right_obj is NULL, returns left_obj, otherwise right_obj */
        if (right_obj == NULL) {
            if (is_bad_type(
                    op_token, TYPE_ANY, NO_OPRAND, left_obj, right_obj
                )) {
                return (object_t*)ERR_OBJECT_PTR;
            }
        }
        return right_obj == NULL ? object_ref(left_obj) : object_ref(right_obj);
    case OP_ASSIGN: {
        token_t* left_token = dynarr_token_at(
            &context.tree->tokens,
            context.tree->lefts[cur_eval_node->token_index]
        );
        if (is_bad_type(op_token, NO_OPRAND, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        if (frame_set(context.cur_frame, left_token->name, right_obj) == NULL) {
            const char* err_msg = "Repeated initialization of "
                                  "identifier '%s' (var_id=%d)";
            sprintf(ERR_MSG_BUF, err_msg, left_token->str, left_token->name);
            print_runtime_error(left_token->pos, ERR_MSG_BUF);
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_ref(right_obj);
    }
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
            object_t* right_left = right_obj->as.pair.left;
            object_t* right_right = right_obj->as.pair.right;
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
    case OP_EXPRSEP:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*)ERR_OBJECT_PTR;
        }
        return object_ref(right_obj);
    default:
        sprintf(ERR_MSG_BUF, "exec_op: bad op name: %d", op_token.name);
        print_runtime_error(op_token.pos, ERR_MSG_BUF);
        return (object_t*)ERR_OBJECT_PTR;
    }
}

object_t*
eval(context_t context, const int entry_index)
{
    object_t* result = NULL;
    frame_t* cur_frame = context.cur_frame;
    const token_tree_t* token_tree = context.tree;
    const token_t* tokens = token_tree->tokens.data;
    dynarr_eval_tree_ptr_t eval_node_stack;
    eval_tree_t* root_eval_node = NULL;

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        const int tree_size = token_tree->sizes[entry_index];
        printf("eval\n");
        printf(
            "entry_index=%d tree_size=%d stack_depth=%d cur_frame=",
            entry_index, tree_size, context.call_depth
        );
        frame_print(cur_frame);
        printf("\n");
    }
#endif

    if (context.call_depth > 1000) {
        print_runtime_error(
            tokens[entry_index].pos, "Call stack too deep (> 1000)"
        );
        return (object_t*)ERR_OBJECT_PTR;
    }

    /* begin evaluation */
    eval_node_stack = dynarr_eval_tree_ptr_new();
    root_eval_node = eval_tree_node_create(entry_index);
    dynarr_eval_tree_ptr_append(&eval_node_stack, &root_eval_node);
    while (eval_node_stack.size > 0) {
        eval_tree_t* cur_eval_node
            = *dynarr_eval_tree_ptr_back(&eval_node_stack);
        const int cur_index = cur_eval_node->token_index,
                  left_index = token_tree->lefts[cur_index],
                  right_index = token_tree->rights[cur_index];
        const token_t cur_token = tokens[cur_index];
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("> (node %d) ", cur_index);
            token_print(&cur_token);
            printf(" left=%d right=%d\n", left_index, right_index);
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
                o = (object_t*)ERR_OBJECT_PTR;
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
        } else if (cur_token.type == TOK_OP) {
            object_t* left_obj
                = (cur_eval_node->left) ? cur_eval_node->left->object : NULL;
            object_t* right_obj
                = (cur_eval_node->right) ? cur_eval_node->right->object : NULL;
            /* bypass right evaluation if:
               - op is unary operator, or
               - op is cond-or and left is not evaled, or
               - op is cond-or and left is true, or
               - op is cond-and and left is not evaled, or
               - op is cond-and and left is false
            */
            int is_condor_and_left_true = (cur_token.name == OP_CONDOR)
                && ((left_obj == NULL) || object_to_bool(left_obj));
            int is_condand_and_left_false = (cur_token.name == OP_CONDAND)
                && ((left_obj == NULL) || !object_to_bool(left_obj));
            int is_bypass_eval_right = is_unary_op(cur_token.name)
                || is_condor_and_left_true || is_condand_and_left_false;
            /* bypass left evaluation if:
               - op is OP_FMAKE or OP_MMAKE or OP_ARG or OP_ASSIGN
            */
            int is_bypass_eval_left = cur_token.name == OP_FMAKE
                || cur_token.name == OP_MMAKE || cur_token.name == OP_ARG
                || cur_token.name == OP_ASSIGN;

            /* if left and right both need eval push them and continue */
            if (!is_bypass_eval_left && left_obj == NULL
                && !is_bypass_eval_right && right_obj == NULL) {
                cur_eval_node->right = eval_tree_node_create(right_index);
                dynarr_eval_tree_ptr_append(
                    &eval_node_stack, &cur_eval_node->right
                );
                cur_eval_node->left = eval_tree_node_create(left_index);
                dynarr_eval_tree_ptr_append(
                    &eval_node_stack, &cur_eval_node->left
                );
                continue;
            } else {
                /* if only one need eval */
                if (!is_bypass_eval_left && left_obj == NULL) {
                    cur_eval_node->left = eval_tree_node_create(left_index);
                    dynarr_eval_tree_ptr_append(
                        &eval_node_stack, &cur_eval_node->left
                    );
                    continue;
                }
                if (!is_bypass_eval_right && right_obj == NULL) {
                    cur_eval_node->right = eval_tree_node_create(right_index);
                    dynarr_eval_tree_ptr_append(
                        &eval_node_stack, &cur_eval_node->right
                    );
                    continue;
                }
            }
            cur_eval_node->object = exec_op(
                context, cur_eval_node, cur_token, left_obj, right_obj
            );
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(
                    "^ (node %d) exec_op %s returned\n", cur_index,
                    OP_STRS[cur_token.name]
                );
                if (cur_token.name == OP_FMAKE) {
                    printf("  Made func:");
                    object_print(cur_eval_node->object, '\n');
                }
                if (cur_token.name == OP_MMAKE) {
                    printf("  Made macro:");
                    object_print(cur_eval_node->object, '\n');
                }
                fflush(stdout);
            }
#endif
            if (cur_eval_node->object->is_error) {
                const char* err_msg = "operator \"%s\" returns with error";
                sprintf(ERR_MSG_BUF, err_msg, OP_STRS[cur_token.name]);
                print_runtime_error(cur_token.pos, ERR_MSG_BUF);
            }
            /* free the both sub-tree */
            if (!is_bypass_eval_left) {
                eval_tree_node_free(cur_eval_node->left);
                cur_eval_node->left = NULL;
            }
            if (!is_bypass_eval_right) {
                eval_tree_node_free(cur_eval_node->right);
                cur_eval_node->right = NULL;
            }
        } else {
            /* not id or num or op: error */
            sprintf(ERR_MSG_BUF, "eval: bad token type: %d\n", cur_token.type);
            print_runtime_error(tokens[cur_index].pos, ERR_MSG_BUF);
            exit(RUNTIME_ERR_CODE);
        }

#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("< (node %d) eval result: ", cur_index);
            object_print(cur_eval_node->object, '\n');
            fflush(stdout);
        }
#endif

        /* early end eval if error */
        if (cur_eval_node->object->is_error) {
            result = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        dynarr_eval_tree_ptr_pop(&eval_node_stack);
    } /* end while loop */

    /* prepare return object */
#ifdef ENABLE_DEBUG_LOG
    if (result == ERR_OBJECT_PTR) {
        if (global_is_enable_debug_log) {
            printf("eval return with error\n");
        }
    }
#endif
    if (result != ERR_OBJECT_PTR) {
        if (root_eval_node->object == NULL) {
            print_runtime_error(
                tokens[entry_index].pos,
                "Unexpected error: root_eval_node is NULL"
            );
            result = (object_t*)ERR_OBJECT_PTR;
        } else {
            result = object_ref(root_eval_node->object);
        }
    }

    /* free the stack */
    dynarr_eval_tree_ptr_free(&eval_node_stack);
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
