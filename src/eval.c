#include "builtin_funcs.h"
#include "frame.h"
#include "main.h"
#include "syntax_tree.h"
#include "utils/debug_flag.h"
#include "utils/errormsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

object_t* eval(context_t context, const int entry_index);

#define NO_OPRAND -1

static inline int
is_bad_type(
    bytecode_t bytecode, int left_good_type, int right_good_type,
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
        BYTECODE_OP_NAMES[bytecode.op],
        left_good_type == NO_OPRAND ? "" : OBJ_TYPE_STR[left_good_type],
        right_good_type == NO_OPRAND ? "" : OBJ_TYPE_STR[right_good_type],
        left_obj == NULL ? "" : OBJ_TYPE_STR[left_obj->type],
        right_obj == NULL ? "" : OBJ_TYPE_STR[right_obj->type]
    );
    print_runtime_error(bytecode.pos, ERR_MSG_BUF);
    return 1;
}

static inline object_t*
eval_call(context_t context, linecol_t pos, const object_t* call, object_t* arg)
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
        object_t* result = eval_call(context, pos, call, arg);
        if (result->is_error) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("eval_map: a child return with error: ");
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
eval_map(context_t context, linecol_t pos, const object_t* call, object_t* pair)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval_map\n");
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
                printf("eval_map: bad pair - left or right is empty: ");
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
                printf("eval_map: a left child return with error: ");
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
                printf("eval_map: a right child return with error: ");
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
        object_t* result = eval_call(context, pos, func, arg);
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
eval_filter(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval_filter\n");
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
                printf("eval_filter: bad pair - left or right is empty: ");
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
                printf("eval_filter: a left child return with error: ");
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
                printf("eval_filter: a right child return with error: ");
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
            printf("eval_filter: pair=");
            object_print(arg_pair, '\n');
            printf("eval_filter: res_obj=");
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
            printf("eval_filter: error\n");
        }
#endif
        object_deref(result);
        result = (object_t*)ERR_OBJECT_PTR;
    }

    /* final merge */
    if (result->type == TYPE_PAIR) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("eval_filter: do final merge on ");
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
eval_reduce(
    context_t context, linecol_t token_pos, const object_t* call, object_t* pair
)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval_reduce\n");
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
                printf("eval_reduce: bad pair - left or right is empty: ");
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
                printf("eval_reduce: a left child return with error: ");
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
                printf("eval_reduce: a right child return with error: ");
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
            = eval_call(context, token_pos, call, *res_pair_ptr);
        if (reduce_result->is_error) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("eval_reduce: a child return with error: ");
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

void
eval_root(const syntax_tree_t* syntax_tree)
{
    frame_t* root_frame = frame_new(NULL);
    dynarr_object_ptr_t stack = dynarr_object_ptr_new();
    context_t root_context = {
        .tree = syntax_tree,
        .cur_frame = root_frame,
        .stack = &stack,
        .call_depth = 0,
    };
    object_t* final_result = eval(root_context, syntax_tree->root_index);
    if (final_result == NULL || final_result->is_error) {
        int i;
        /* clear stack because it may not be cleared if not properly returned */
        for (i = 0; i < stack.size; i++) {
            object_deref(stack.data[i]);
        }
    }
    object_deref(final_result);
    dynarr_object_ptr_free(&stack);
    frame_free(root_frame);
    free(root_frame);
}

object_t*
eval(context_t context, const int entry_index)
{
    int i;
    const syntax_tree_t* tree = context.tree;
    dynarr_bytecode_t bytecodes
        = syntax_tree_get_bytecode_from_node_index(tree, entry_index);
    dynarr_object_ptr_t* stack = context.stack;

    /*  registers */
    object_t* return_obj = NULL;
    uint32_t arg = 0; /* extendable argument */
    int inst_ptr = 0; /* instruction pointer */
    int ret_flag = 0; /* return flag */
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval\n");
        printf(
            "entry_index=%d stack_depth=%d cur_frame=", entry_index,
            context.call_depth
        );
        frame_print(context.cur_frame);
        printf("\n");
    }
#endif

    /* check call depth */
    if (context.call_depth > 1000) {
        print_runtime_error(
            tree->tokens.data[entry_index].pos, "Call stack too deep (> 1000)"
        );
        return (object_t*)ERR_OBJECT_PTR;
    }

#define GET_L_CHECK_TYPE(stack, left_tmp, left_type)                           \
    {                                                                          \
        left_tmp = *dynarr_object_ptr_back(stack);                             \
        if (is_bad_type(bc, left_type, NO_OPRAND, left_tmp, NULL)) {           \
            return_obj = (object_t*)ERR_OBJECT_PTR;                            \
            break;                                                             \
        }                                                                      \
    }

#define POP_L_CHECK_TYPE(stack, left_tmp, left_type)                           \
    {                                                                          \
        left_tmp = *dynarr_object_ptr_back(stack);                             \
        dynarr_object_ptr_pop(stack);                                          \
        if (is_bad_type(bc, left_type, NO_OPRAND, left_tmp, NULL)) {           \
            return_obj = (object_t*)ERR_OBJECT_PTR;                            \
            break;                                                             \
        }                                                                      \
    }

#define POP_LR_CHECK_TYPE(stack, left_obj, right_obj, left_type, right_type)   \
    {                                                                          \
        right_obj = *dynarr_object_ptr_back(stack);                            \
        dynarr_object_ptr_pop(stack);                                          \
        left_obj = *dynarr_object_ptr_back(stack);                             \
        dynarr_object_ptr_pop(stack);                                          \
        if (is_bad_type(bc, left_type, right_type, left_obj, right_obj)) {     \
            return_obj = (object_t*)ERR_OBJECT_PTR;                            \
            break;                                                             \
        }                                                                      \
    }

#define DEREF_RL(left_obj, right_obj)                                          \
    {                                                                          \
        object_deref(left_obj);                                                \
        object_deref(right_obj);                                               \
    }

    /* begin evaluation */
    while (ret_flag == 0) {
        bytecode_t bc = bytecodes.data[inst_ptr];
        object_t* left_obj;
        object_t* right_obj;

#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("inst_ptr=%d, arg=%u, eval_stack=[", inst_ptr, arg);
            for (i = 0; i < stack->size; i++) {
                object_print(stack->data[i], ',');
                printf(" ");
            }
            printf("]\n");
            printf("exec bytecode: ");
            bytecode_print(bc);
            printf("\n");
        }
#endif

        switch (bc.op) {
        case BOP_NOP:
            break;
        case BOP_EXTEND_ARG:
            arg = ((arg << 8) | bc.arg) << 8;
            break;
        case BOP_PUSH_LITERAL:
            arg = arg | bc.arg;
            return_obj = object_ref(tree->literals[arg]);
            dynarr_object_ptr_append(stack, &return_obj);
            break;
        case BOP_FRAME_GET:
            arg = arg | bc.arg;
            {
                object_t* o = frame_get(context.cur_frame, arg);
                if (o == NULL) {
                    const char* err_msg = "Identifier '%s' used uninitialized";
                    sprintf(ERR_MSG_BUF, err_msg, tree->id_code_str_map[arg]);
                    print_runtime_error(bc.pos, ERR_MSG_BUF);
                    return_obj = (object_t*)ERR_OBJECT_PTR;
                } else {
                    return_obj = object_ref(o);
                }
            }
            dynarr_object_ptr_append(stack, &return_obj);
            break;
        case BOP_FRAME_SET:
            arg = arg | bc.arg;
            return_obj = *dynarr_object_ptr_back(stack);
            if (!frame_set(context.cur_frame, arg, return_obj)) {
                const char* err_msg
                    = "Repeated initialization of identifier '%s'";
                sprintf(ERR_MSG_BUF, err_msg, tree->id_code_str_map[arg]);
                print_runtime_error(bc.pos, ERR_MSG_BUF);
                return_obj = (object_t*)ERR_OBJECT_PTR;
            }
            break;
        case BOP_POP:
            return_obj = *dynarr_object_ptr_back(stack);
            dynarr_object_ptr_pop(stack);
            object_deref(return_obj);
            return_obj = NULL;
            break;
        case BOP_RET:
            return_obj = *dynarr_object_ptr_back(stack);
            dynarr_object_ptr_pop(stack);
            ret_flag = 1;
            break;
        case BOP_JUMP:
            /* not implemented */
            break;
        case BOP_JUMP_FALSE_OR_POP:
            return_obj = *dynarr_object_ptr_back(stack);
            if (object_to_bool(return_obj)) {
                dynarr_object_ptr_pop(stack);
                object_deref(return_obj);
                return_obj = NULL;
            } else {
                arg = arg | bc.arg;
                inst_ptr += arg;
            }
            break;
        case BOP_JUMP_TRUE_OR_POP:
            return_obj = *dynarr_object_ptr_back(stack);
            if (!object_to_bool(return_obj)) {
                dynarr_object_ptr_pop(stack);
                object_deref(return_obj);
                return_obj = NULL;
            } else {
                arg = arg | bc.arg;
                inst_ptr += arg;
            }
            break;
        case BOP_MAKE_FUNCT:
            arg = arg | bc.arg;
            return_obj = object_create(
                TYPE_CALL,
                (object_data_union)(callable_t) {
                    .is_macro = 0,
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_code = -1,
                    .index = arg,
                    /* function owns copy of frame it created under */
                    .init_frame = frame_copy(context.cur_frame),
                }
            );
            dynarr_object_ptr_append(stack, &return_obj);
            break;
        case BOP_MAKE_MACRO:
            arg = arg | bc.arg;
            return_obj = object_create(
                TYPE_CALL,
                (object_data_union)(callable_t) {
                    .is_macro = 1,
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_code = -1,
                    .index = arg,
                    /* macro does not have frame */
                    .init_frame = NULL,
                }
            );
            dynarr_object_ptr_append(stack, &return_obj);
            break;
        case BOP_CALL:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_CALL, TYPE_ANY);
            return_obj = eval_call(context, bc.pos, left_obj, right_obj);
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_MAP:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_CALL, TYPE_PAIR);
            return_obj = eval_map(context, bc.pos, left_obj, right_obj);
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_FILTER:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_CALL, TYPE_PAIR);
            return_obj = eval_filter(context, bc.pos, left_obj, right_obj);
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_REDUCE:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_CALL, TYPE_PAIR);
            return_obj = eval_reduce(context, bc.pos, left_obj, right_obj);
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_NEG:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_NUM);
            return_obj = object_create(
                left_obj->type,
                (object_data_union)number_neg(&left_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_NOT:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_ANY);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)(object_to_bool(left_obj) ? ONE_NUMBER
                                                             : ZERO_NUMBER)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_CEIL:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM, (object_data_union)number_ceil(&left_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_FLOOR:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM, (object_data_union)number_floor(&left_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_GETL:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_PAIR);
            return_obj = object_ref(left_obj->as.pair.left);
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_GETR:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_PAIR);
            return_obj = object_ref(left_obj->as.pair.right);
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_COND_CALL:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_ANY);
            if (left_obj->type == TYPE_CALL) {
                return_obj = eval_call(
                    context, bc.pos, left_obj,
                    (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
                );
            } else {
                return_obj = object_ref(left_obj);
            }
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_SWAP:
            POP_L_CHECK_TYPE(stack, left_obj, TYPE_PAIR);
            return_obj = object_create(
                TYPE_PAIR,
                (object_data_union)(pair_t) {
                    .left = object_ref(left_obj->as.pair.right),
                    .right = object_ref(left_obj->as.pair.left),
                }
            );
            dynarr_object_ptr_append(stack, &return_obj);
            object_deref(left_obj);
            break;
        case BOP_EXP:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            if (right_obj->as.number.denom.size != 1
                || right_obj->as.number.denom.digit[0] != 1) {
                print_runtime_error(bc.pos, "Exponent must be integer");
                return_obj = (object_t*)ERR_OBJECT_PTR;
                break;
            }
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_exp(&left_obj->as.number, &right_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_MUL:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_mul(&left_obj->as.number, &right_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_DIV:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            if (right_obj->as.number.numer.size == 0) {
                print_runtime_error(bc.pos, "Divided by zero");
                return_obj = (object_t*)ERR_OBJECT_PTR;
                break;
            }
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_div(&left_obj->as.number, &right_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_MOD:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_mod(&left_obj->as.number, &right_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_ADD:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_add(&left_obj->as.number, &right_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_SUB:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_sub(&left_obj->as.number, &right_obj->as.number)
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_LT:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)number_from_i32(
                    number_lt(&left_obj->as.number, &right_obj->as.number)
                )
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_LE:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)number_from_i32(
                    !number_lt(&right_obj->as.number, &left_obj->as.number)
                )
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_GT:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)number_from_i32(
                    number_lt(&right_obj->as.number, &left_obj->as.number)
                )
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_GE:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_NUM, TYPE_NUM);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)number_from_i32(
                    !number_lt(&left_obj->as.number, &right_obj->as.number)
                )
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_EQ:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_ANY, TYPE_ANY);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_from_i32(object_eq(left_obj, right_obj))
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_NE:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_ANY, TYPE_ANY);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)
                    number_from_i32(!object_eq(left_obj, right_obj))
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_AND:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_ANY, TYPE_ANY);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)number_from_i32(
                    object_to_bool(left_obj) && object_to_bool(right_obj)
                )
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_OR:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_ANY, TYPE_ANY);
            return_obj = object_create(
                TYPE_NUM,
                (object_data_union)number_from_i32(
                    object_to_bool(left_obj) || object_to_bool(right_obj)
                )
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_PAIR:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_ANY, TYPE_ANY);
            return_obj = object_create(
                TYPE_PAIR,
                (object_data_union)(pair_t) {
                    .left = object_ref(left_obj),
                    .right = object_ref(right_obj),
                }
            );
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        case BOP_BIND_ARG:
            return_obj = *dynarr_object_ptr_back(stack);
            if (is_bad_type(bc, NO_OPRAND, TYPE_CALL, NULL, return_obj)) {
                return_obj = (object_t*)ERR_OBJECT_PTR;
                break;
            }
            if (return_obj->as.callable.is_macro) {
                const char* err_msg
                    = "Right side of argument binder should be function";
                print_runtime_error(bc.pos, err_msg);
                return_obj = (object_t*)ERR_OBJECT_PTR;
                break;
            }
            if (return_obj->as.callable.arg_code != -1) {
                const char* err_msg
                    = "Bind argument to a function that already has one";
                print_runtime_error(bc.pos, err_msg);
                return_obj = (object_t*)ERR_OBJECT_PTR;
                break;
            }
            arg = arg | bc.arg;
            return_obj->as.callable.arg_code = arg;
            break;
        case OP_COND_PAIR_CALL:
            POP_LR_CHECK_TYPE(stack, left_obj, right_obj, TYPE_ANY, TYPE_PAIR);
            /* check inside the pair */
            {
                object_t* right_left = right_obj->as.pair.left;
                object_t* right_right = right_obj->as.pair.right;
                if (is_bad_type(
                        bc, TYPE_CALL, TYPE_CALL, right_left, right_right
                    )) {
                    return_obj = (object_t*)ERR_OBJECT_PTR;
                    break;
                }
                return_obj = eval_call(
                    context, bc.pos,
                    (object_to_bool(left_obj) ? right_left : right_right),
                    (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
                );
            }
            dynarr_object_ptr_append(stack, &return_obj);
            DEREF_RL(left_obj, right_obj);
            break;
        default:
            sprintf(ERR_MSG_BUF, "eval: bad bytecode: %d,%d\n", bc.op, bc.arg);
            print_runtime_error(bc.pos, ERR_MSG_BUF);
            exit(RUNTIME_ERR_CODE);
            break;
        }
        if (return_obj && return_obj->is_error) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("early return because intermediate result is error\n");
            }
#endif
            break;
        }
        inst_ptr++;
        if (bc.op != BOP_EXTEND_ARG) {
            arg = 0;
        }
    }

    if (return_obj == NULL) {
        return_obj = (object_t*)ERR_OBJECT_PTR;
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval returned ");
        object_print(return_obj, '\n');
        fflush(stdout);
    }
#endif
    return return_obj;
}
