#include "eval.h"
#include "frame.h"
#include "builtin_funcs.h"
#include "utils/debug_flag.h"

object_t*
exec_call(context_t context, linecol_t pos, const object_t* call, object_t* arg)
{
    object_t* result;
    frame_t* caller_frame = context.cur_frame;
    frame_t* callee_frame;
    callable_t callable = call->as.callable;

    /* if is builtin */
    if (callable.builtin_name != -1) {
        object_t* (*func_ptr)(const object_t*)
            = BUILDTIN_FUNC_ARRAY[callable.builtin_name];
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
        printf("eval_call: prepare call frame\n");
    }
#endif
    if (callable.is_macro) {
        callee_frame = caller_frame;
    } else {
        int arg_code = callable.arg_code;
        callee_frame = frame_get_callee_frame(caller_frame, call);
        /* set argument to call-frame */
        if (arg_code != -1 && frame_set(callee_frame, arg_code, arg) == NULL) {
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
        printf("eval_call: callee_frame=%p\nfunc_obj=", callee_frame);
        object_print(call, '\n');
        printf("arg=");
        object_print(arg, '\n');
    }
#endif
    /* eval from function's entry index */
    {
        context_t new_context = context;
        new_context.cur_frame = callee_frame;
        new_context.call_depth++;
        result = eval(new_context, callable.index);
    }
    if (!callable.is_macro) {
        /* free the objects own by this function call */
        frame_pop_stack(callee_frame);
        /* free the stacks */
        frame_free(callee_frame);
        free(callee_frame);
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        int* last_entry_index = dynarr_int_back(&caller_frame->entry_indexs);
        int caller_entry_index = context.tree->root_index;
        if (last_entry_index) {
            caller_entry_index = *last_entry_index;
        }
        printf(
            "eval_call: return to caller (entry_index=%d)\n", caller_entry_index
        );
    }
#endif
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
