#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errormsg.h"
#include "tree.h"
#include "builtin_funcs.h"
#include "dynarr.h"
#include "frame.h"
#include "lreng.h"

#define NO_OPRAND -1

static inline int
is_bad_type(
    token_t op_token,
    int left_type,
    int right_type,
    object_t* left_obj,
    object_t* right_obj
) {
    int left_passed, right_passed;
    if (left_type == NO_OPRAND) {
        left_passed = left_obj == NULL;
    }
    else if (left_type == TYPE_ANY) {
        left_passed = left_obj != NULL;
    }
    else {
        left_passed = left_obj != NULL && left_obj->type == left_type;
    }
    if (right_type == NO_OPRAND) {
        right_passed = right_obj == NULL;
    }
    else if (right_type == TYPE_ANY) {
        right_passed = right_obj != NULL;
    }
    else {
        right_passed = right_obj != NULL && right_obj->type == right_type;
    }
    if (!left_passed || !right_passed) {
        printf("%d", op_token.name);
        sprintf(
            ERR_MSG_BUF,
            "Bad type for operator \"%s\": expect (%s, %s), get (%s, %s)",
            OP_STRS[op_token.name],
            left_type == NO_OPRAND ? "" : OBJECT_TYPE_STR[left_type],
            right_type == NO_OPRAND ? "" : OBJECT_TYPE_STR[right_type],
            left_obj == NULL ? "" : OBJECT_TYPE_STR[left_obj->type],
            right_obj == NULL ? "" : OBJECT_TYPE_STR[right_obj->type]
        );
        print_runtime_error(op_token.pos, ERR_MSG_BUF);
        return 1;
    }
    return 0;
}

static inline object_t*
exec_call(
    const tree_t* tree,
    const frame_t* cur_frame,
    linecol_t token_pos,
    object_t* func_obj,
    object_t* arg_obj,
    const int is_debug
) {
    object_t* returned_obj;
    /* if is builtin */
    if (func_obj->data.callable.builtin_name != -1) {
        object_t* (*func_ptr)(object_t*) =
            BUILDTIN_FUNC_ARRAY[func_obj->data.callable.builtin_name];
        if (func_ptr == NULL) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        ERR_MSG_BUF[0] = '\0';
        returned_obj = func_ptr(arg_obj);
        if (returned_obj->is_error && ERR_MSG_BUF[0] != '\0') {
            print_runtime_error(token_pos, ERR_MSG_BUF);
        }
        return returned_obj;
    }

#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_call: prepare call frame\n");
    }
#endif
    frame_t* call_frame;
    if (func_obj->data.callable.is_macro) {
        call_frame = (frame_t*) cur_frame;
    }
    else {
        #define f_init_frame func_obj->data.callable.init_time_frame
        int i, last_cur_index, cur_index = -1, init_index = -1, is_forked = 0;
        call_frame = calloc(1, sizeof(frame_t));
        call_frame->global_pairs = cur_frame->global_pairs;

        if (
            cur_frame->stack.size > 0
            && *(int*) back(&cur_frame->indexs) == func_obj->data.callable.index
        ) {
            // if is recursion, copy the current stack except the last
            call_frame->indexs = copy_dynarr(&cur_frame->indexs);
            pop(&call_frame->indexs);
            call_frame->stack = copy_dynarr(&cur_frame->stack);
            pop(&call_frame->stack);
        }
        else {
            /* if the i-th stack of init-time frame and current frame is the
            same, call-frame's i-th stack = current frame's i-th stack. but once
            the entry_index is different they are in different closure path, so
            only the rest of init-time-frame stack is used */
            new_stack(call_frame);
            for (i = 0; i < f_init_frame->stack.size; i++) {
                init_index = *(int*) at(&f_init_frame->indexs, i);
                cur_index = -1;
                if (i < cur_frame->stack.size) {
                    cur_index = *(int*) at(&cur_frame->indexs, i);
                }
                /* push the entry_index */
                append(&call_frame->indexs, &init_index);

                dynarr_t* src_pairs = NULL;
                if (!is_forked && cur_index == init_index) {
                    src_pairs = (dynarr_t*) at(&cur_frame->stack, i);
    #ifdef ENABLE_DEBUG
                    if (is_debug) {
                        printf("exec_call: stack[%d] use current\n", i);
                    }
    #endif
                }
                else {
                    is_forked = 1;
                    src_pairs = (dynarr_t*) at(&f_init_frame->stack, i);
    #ifdef ENABLE_DEBUG
                    if (is_debug) {
                        printf("exec_call: stack[%d] use init-time\n", i);
                    }
    #endif
                }
                /* push the shallow copy of source pairs */
                append(&call_frame->stack, src_pairs);
            }
        }

        /* push new stack to call_frame and set argument */
        push_stack(call_frame, func_obj->data.callable.index);
        if (func_obj->data.callable.arg_name != -1) {
            frame_set(call_frame, func_obj->data.callable.arg_name, arg_obj);
        }
        #undef func_init_time_frame
    }

#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_call: call_frame=%p\nfunc_obj=", call_frame);
        print_object(func_obj, '\n');
        printf("arg_obj=");
        print_object(arg_obj, '\n');
    }
#endif
    /* eval from function's entry index */
    returned_obj = eval_tree(
        tree,
        call_frame,
        func_obj->data.callable.index,
        is_debug
    );
    if (!func_obj->data.callable.is_macro) {
        /* free the object own by this function call */
        pop_stack(call_frame);
        /* free the rest of stack but not free bollowed pairs */
        clear_stack(call_frame, 0);
        free(call_frame);
    }
    return returned_obj;
}

/* apply function recursively in postfix order and return a new pair */
object_t*
exec_map(
    const tree_t* tree,
    const frame_t* cur_frame,
    linecol_t token_pos,
    object_t* func_obj,
    object_t* pair_obj,
    const int is_debug
) {
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_map\n");
        printf("func_obj="); print_object(func_obj, '\n');
        printf("pair_obj="); print_object(pair_obj, '\n');
    }
#endif
    int is_error = 0;
    object_t* returned_obj = create_object(
        TYPE_PAIR,
        (object_data_t) {.pair = (pair_t) {NULL, NULL}}
    );
    dynarr_t arg_pair_stack, ret_pair_stack;
    arg_pair_stack = new_dynarr(sizeof(object_t*));
    ret_pair_stack = new_dynarr(sizeof(object_t*));
    append(&arg_pair_stack, &pair_obj);
    append(&ret_pair_stack, &returned_obj);
    while(arg_pair_stack.size > 0) {
        object_t* arg_pair_obj = *(object_t**) back(&arg_pair_stack);
        object_t* ret_pair_obj = *(object_t**) back(&ret_pair_stack);
        /* check */
        if (
            arg_pair_obj->data.pair.left == NULL
            || arg_pair_obj->data.pair.right == NULL
        ) {
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf("exec_map: bad pair - left or right is empty: ");
                print_object(arg_pair_obj, '\n');
            }
#endif
            is_error = 1;
            break;
        }
        /* left */
        if (arg_pair_obj->data.pair.left->type == TYPE_PAIR) {
            if (ret_pair_obj->data.pair.left == NULL) {
                ret_pair_obj->data.pair.left = create_object(
                    TYPE_PAIR,
                    (object_data_t) {.pair = (pair_t) {NULL, NULL}}
                );
                append(&arg_pair_stack, &arg_pair_obj->data.pair.left);
                append(&ret_pair_stack, &ret_pair_obj->data.pair.left);
                continue;
            }
        }
        else if (ret_pair_obj->data.pair.left == NULL) {
            object_t* left_ret_obj = exec_call(
                tree, cur_frame, token_pos, func_obj,
                arg_pair_obj->data.pair.left, is_debug
            );
            if (left_ret_obj->is_error) {
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("exec_map: a left child return with error: ");
                    print_object(left_ret_obj, '\n');
                }
#endif
                is_error = 1;
                break;
            }
            ret_pair_obj->data.pair.left = left_ret_obj;
        }
        /* right */
        if (arg_pair_obj->data.pair.right->type == TYPE_PAIR) {
            if (ret_pair_obj->data.pair.right == NULL) {
                ret_pair_obj->data.pair.right = create_object(
                    TYPE_PAIR,
                    (object_data_t) {.pair = (pair_t) {NULL, NULL}}
                );
                append(&arg_pair_stack, &arg_pair_obj->data.pair.right);
                append(&ret_pair_stack, &ret_pair_obj->data.pair.right);
                continue;
            }
        }
        else if (ret_pair_obj->data.pair.right == NULL) {
            object_t* right_ret_obj = exec_call(
                tree, cur_frame, token_pos, func_obj,
                arg_pair_obj->data.pair.right, is_debug
            );
            if (right_ret_obj->is_error) {
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("exec_map: a right child return with error: ");
                    print_object(right_ret_obj, '\n');
                }
#endif
                is_error = 1;
                break;
            }
            ret_pair_obj->data.pair.right = right_ret_obj;
        }

        pop(&arg_pair_stack);
        pop(&ret_pair_stack);
    }
    if (is_error) {
        free_object(returned_obj);
        returned_obj = (object_t*) ERR_OBJECT_PTR;
    }
    free_dynarr(&arg_pair_stack);
    free_dynarr(&ret_pair_stack);
    return returned_obj;
}

/* apply function recursively in postfix order to the children. if the return
   value is true, keep the child, otherwise, set it to NULL.
   if a pair's two children's return value is all false, the pair itself becomes
   NULL as well. */
object_t*
exec_filter(
    const tree_t* tree,
    const frame_t* cur_frame,
    linecol_t token_pos,
    object_t* func_obj,
    object_t* pair_obj,
    const int is_debug
) {
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_filter\n");
        printf("func_obj="); print_object(func_obj, '\n');
        printf("pair_obj="); print_object(pair_obj, '\n');
    }
#endif
    int is_error = 0;
    object_t* returned_obj = create_object(
        TYPE_PAIR,
        (object_data_t) {.pair = (pair_t) {NULL, NULL}}
    );
    dynarr_t arg_pair_stack, ret_pair_stack;
    arg_pair_stack = new_dynarr(sizeof(object_t*));
    ret_pair_stack = new_dynarr(sizeof(object_t*));
    append(&arg_pair_stack, &pair_obj);
    append(&ret_pair_stack, &returned_obj);
    while(arg_pair_stack.size > 0) {
        object_t* arg_pair_obj = *(object_t**) back(&arg_pair_stack);
        object_t* ret_pair_obj = *(object_t**) back(&ret_pair_stack);
        /* check */
        if (
            arg_pair_obj->data.pair.left == NULL
            || arg_pair_obj->data.pair.right == NULL
        ) {
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf("exec_filter: bad pair - left or right is empty: ");
                print_object(arg_pair_obj, '\n');
            }
#endif
            is_error = 1;
            break;
        }
        /* left */
        if (arg_pair_obj->data.pair.left->type == TYPE_PAIR) {
            if (ret_pair_obj->data.pair.left == NULL) {
                ret_pair_obj->data.pair.left = create_object(
                    TYPE_PAIR,
                    (object_data_t) {.pair = (pair_t) {NULL, NULL}}
                );
                append(&arg_pair_stack, &arg_pair_obj->data.pair.left);
                append(&ret_pair_stack, &ret_pair_obj->data.pair.left);
                continue;
            }
            /* the child's children are all null, make it also null */
            object_t* temp_left = ret_pair_obj->data.pair.left;
            if (
                temp_left->type == TYPE_PAIR
                && temp_left->data.pair.left->type == TYPE_NULL
                && temp_left->data.pair.right->type == TYPE_NULL
            ) {
                free_object(temp_left);
                ret_pair_obj->data.pair.left = (object_t*) NULL_OBJECT_PTR;
            }
        }
        else if (ret_pair_obj->data.pair.left == NULL) {
            object_t* left_ret_obj = exec_call(
                tree, cur_frame, token_pos, func_obj,
                arg_pair_obj->data.pair.left, is_debug
            );
            if (left_ret_obj->is_error) {
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("exec_filter: a left child return with error: ");
                    print_object(left_ret_obj, '\n');
                }
#endif
                is_error = 1;
                free_object(left_ret_obj);
                break;
            }
            if (to_bool(left_ret_obj)) {
                ret_pair_obj->data.pair.left = copy_object(
                    arg_pair_obj->data.pair.left
                );
            }
            else {
                ret_pair_obj->data.pair.left = (object_t*) NULL_OBJECT_PTR;
            }
            free_object(left_ret_obj);
        }
        /* right */
        if (arg_pair_obj->data.pair.right->type == TYPE_PAIR) {
            if (ret_pair_obj->data.pair.right == NULL) {
                ret_pair_obj->data.pair.right = create_object(
                    TYPE_PAIR,
                    (object_data_t) {.pair = (pair_t) {NULL, NULL}}
                );
                append(&arg_pair_stack, &arg_pair_obj->data.pair.right);
                append(&ret_pair_stack, &ret_pair_obj->data.pair.right);
                continue;
            }
            /* the child's children are all null, make it also null */
            object_t* temp_right = ret_pair_obj->data.pair.right;
            if (
                temp_right->type == TYPE_PAIR
                && temp_right->data.pair.left->type == TYPE_NULL
                && temp_right->data.pair.right->type == TYPE_NULL
            ) {
                free_object(temp_right);
                ret_pair_obj->data.pair.right = (object_t*) NULL_OBJECT_PTR;
            }
        }
        else if (ret_pair_obj->data.pair.right == NULL) {
            object_t* right_ret_obj = exec_call(
                tree, cur_frame, token_pos, func_obj,
                arg_pair_obj->data.pair.right, is_debug
            );
            if (right_ret_obj->is_error) {
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("exec_filter: a right child return with error: ");
                    print_object(right_ret_obj, '\n');
                }
#endif
                is_error = 1;
                free_object(right_ret_obj);
                break;
            }
            if (to_bool(right_ret_obj)) {
                ret_pair_obj->data.pair.right = copy_object(
                    arg_pair_obj->data.pair.right
                );
            }
            else {
                ret_pair_obj->data.pair.right = (object_t*) NULL_OBJECT_PTR;
            }
            free_object(right_ret_obj);
        }

        pop(&arg_pair_stack);
        pop(&ret_pair_stack);
    }
    if (is_error) {
        free_object(returned_obj);
        returned_obj = (object_t*) ERR_OBJECT_PTR;
    }
    if (
        returned_obj->data.pair.left->type == TYPE_NULL
        && returned_obj->data.pair.right->type == TYPE_NULL
    ) {
        free_object(returned_obj);
        returned_obj = (object_t*) NULL_OBJECT_PTR;
    }
    free_dynarr(&arg_pair_stack);
    free_dynarr(&ret_pair_stack);
    return returned_obj;
}

object_t*
exec_reduce(
    const tree_t* tree,
    const frame_t* cur_frame,
    linecol_t token_pos,
    object_t* func_obj,
    object_t* pair_obj,
    const int is_debug
) {
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("exec_reduce\n");
        printf("func_obj="); print_object(func_obj, '\n');
        printf("pair_obj="); print_object(pair_obj, '\n');
    }
#endif
    printf("exec_reduce: not implemented yet\n");
    return (object_t*) ERR_OBJECT_PTR;
}

static inline object_t*
exec_op(
    const tree_t* tree,
    const frame_t* cur_frame,
    token_t op_token,
    object_t* left_obj,
    object_t* right_obj,
    const int is_debug
) {
    object_t* tmp_obj;
    switch(op_token.name) {
    case OP_FCALL:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        };
        return exec_call(
            tree, cur_frame, op_token.pos, left_obj, right_obj, is_debug
        );
    case OP_MAP:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return exec_map(
            tree, cur_frame, op_token.pos, left_obj, right_obj, is_debug
        );
    case OP_FILTER:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return exec_filter(
            tree, cur_frame, op_token.pos, left_obj, right_obj, is_debug
        );
    case OP_REDUCE:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return exec_reduce(
            tree, cur_frame, op_token.pos, left_obj, right_obj, is_debug
        );
    /* case OP_POS: */
        /* OP_POS would be discarded in tree parser */
    case OP_NEG:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        tmp_obj = create_object(
            left_obj->type,
            left_obj->data
        );
        tmp_obj->data.number.numer.sign = !tmp_obj->data.number.numer.sign;
        return tmp_obj;
    case OP_NOT:
        if (is_bad_type(op_token, TYPE_ANY, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) (to_bool(left_obj) ? ONE_NUMBER : ZERO_NUMBER)
        );
    case OP_CEIL:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_ceil(&left_obj->data.number)
        );
    case OP_FLOOR:
        if (is_bad_type(op_token, TYPE_NUM, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_floor(&left_obj->data.number)
        );
    case OP_GETL:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return copy_object(left_obj->data.pair.left);
    case OP_GETR:
        if (is_bad_type(op_token, TYPE_PAIR, NO_OPRAND, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return copy_object(left_obj->data.pair.right);
    case OP_EXP:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        if (right_obj->data.number.denom.size != 1
            || right_obj->data.number.denom.digit[0] != 1) {
            print_runtime_error(op_token.pos, "Exponent must be integer");
            return (object_t*) ERR_OBJECT_PTR;
        }
        return  create_object(
            TYPE_NUM,
            (object_data_t) number_exp(
                &left_obj->data.number,
                &right_obj->data.number
            )
        );
    case OP_MUL:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_mul(
                &left_obj->data.number,
                &right_obj->data.number
            )
        );
    case OP_DIV:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        if (right_obj->data.number.numer.size == 0) {
            print_runtime_error(op_token.pos, "Divided by zero");
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_div(
                &left_obj->data.number,
                &right_obj->data.number
            )
        );
    case OP_MOD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_mod(
                &left_obj->data.number,
                &right_obj->data.number
            )
        );
    case OP_ADD:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_add(
                &left_obj->data.number,
                &right_obj->data.number
            )
        );
    case OP_SUB:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_sub(
                &left_obj->data.number,
                &right_obj->data.number
            )
        );
    case OP_LT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(
                number_lt(&left_obj->data.number, &right_obj->data.number)
            )
        );
    case OP_LE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(
                !number_lt(&right_obj->data.number, &left_obj->data.number)
            )
        );
    case OP_GT:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(
                number_lt(&right_obj->data.number, &left_obj->data.number)
            )
        );
    case OP_GE:
        if (is_bad_type(op_token, TYPE_NUM, TYPE_NUM, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(
                !number_lt(&left_obj->data.number, &right_obj->data.number)
            )
        );
    case OP_EQ:
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(object_eq(left_obj, right_obj))
        );
    case OP_NE:
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(!object_eq(left_obj, right_obj))
        );
    case OP_AND:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(
                to_bool(left_obj) && to_bool(right_obj)
            )
        );
    case OP_OR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_NUM,
            (object_data_t) number_from_i32(
                to_bool(left_obj) || to_bool(right_obj)
            )
        );
    case OP_PAIR:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return create_object(
            TYPE_PAIR,
            (object_data_t) (pair_t) {
                .left = copy_object(left_obj),
                .right = copy_object(right_obj)
            }
        );
    case OP_FCALLR:
        if (is_bad_type(op_token, TYPE_CALL, TYPE_ANY, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        return exec_call(
            tree, cur_frame, op_token.pos, left_obj, right_obj, is_debug
        );
    case OP_CONDFCALL:
        if (is_bad_type(op_token, TYPE_ANY, TYPE_PAIR, left_obj, right_obj)) {
            return (object_t*) ERR_OBJECT_PTR;
        }
        /* check inside the pair */
        {
            token_t tmp_op_token = (token_t) {
                .name = OP_CONDFCALL,
                .pos = op_token.pos,
                .type = TOK_OP,
                .str = "the members in the conditional function caller"
            };
            if (is_bad_type(
                tmp_op_token, TYPE_CALL, TYPE_CALL,
                right_obj->data.pair.left, right_obj->data.pair.right
            )) {
                return (object_t*) ERR_OBJECT_PTR;
            }

        }
        return exec_call(
            tree,
            cur_frame,
            op_token.pos,
            (to_bool(left_obj)
                ? right_obj->data.pair.left : right_obj->data.pair.right),
            (object_t*) &RESERVED_OBJS[RESERVED_ID_NAME_NULL],
            is_debug
        );
    default:
        print_runtime_error(op_token.pos, "exec_op: bad op name");
        return (object_t*) ERR_OBJECT_PTR;
    }
}


object_t*
eval_tree(
    const tree_t* tree,
    frame_t* cur_frame,
    const int entry_index,
    const int is_debug
) {
    const token_t* global_tokens = tree->tokens.data;
    const int tree_size = tree->sizes[entry_index];
    /* obj_table_offset = entry_index - tree_size + 1
       because global tokens are order in postfix */
    const int obj_table_offset = entry_index - tree_size + 1;
    
    dynarr_t token_index_stack; /* type: int */
    object_t** obj_table;

    token_t cur_token;
    int cur_index, left_index, right_index;
    object_t* left_obj;
    object_t* right_obj;

    int i, is_error = 0;
    object_t* returned_obj;

#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("eval_tree\n");
        printf("entry_index=%d cur_frame=%p ", entry_index, cur_frame);
        printf("obj_table_offset = %d ", obj_table_offset);
        printf("tree_size = %d\n", tree_size);
    }
#endif

    /* obj_table store shallow copyed intermidiate results */
    obj_table = calloc(tree_size, sizeof(object_t*));
    #define _OBJ_TABLE(index) (obj_table[index - obj_table_offset])

    /* begin evaluation */
    token_index_stack = new_dynarr(sizeof(int));
    append(&token_index_stack, &entry_index);
    while (token_index_stack.size > 0) {
        cur_index = *(int*) back(&token_index_stack);
        cur_token = global_tokens[cur_index];
        left_index = tree->lefts[cur_index];
        right_index = tree->rights[cur_index];
#ifdef ENABLE_DEBUG
        if(is_debug) {
            printf("> (node %d) ", cur_index);
            print_token(cur_token);
            printf(
                " (local=%d) left=%d right=%d\n",
                cur_index - obj_table_offset, left_index, right_index
            );
            fflush(stdout);
        }
#endif
        left_obj = (left_index == -1) ? NULL : _OBJ_TABLE(left_index);
        right_obj = (right_index == -1) ? NULL : _OBJ_TABLE(right_index);

        if (cur_token.type == TOK_OP) {
            const token_t* left_token = (left_index == -1)
                ? NULL : &global_tokens[left_index];
            const token_t* right_token = (right_index == -1)
                ? NULL : &global_tokens[right_index];

            switch (cur_token.name) {
            /* function maker */
            case OP_FMAKE:
                _OBJ_TABLE(cur_index) = create_object(
                    TYPE_CALL,
                    (object_data_t) (callable_t) {
                        .is_macro = 0,
                        .builtin_name = NOT_BUILTIN_FUNC,
                        .arg_name = -1,
                        .index = left_index,
                        /* owns a deep copy of the frame it created under */
                        .init_time_frame = copy_frame(cur_frame)
                    }
                );
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf(" make function:");
                    print_object(_OBJ_TABLE(cur_index), '\n');
                }
#endif
                break;
            /* macro maker */
            case OP_MMAKE:
                _OBJ_TABLE(cur_index) = create_object(
                    TYPE_CALL,
                    (object_data_t) (callable_t) {
                        .is_macro = 1,
                        .builtin_name = NOT_BUILTIN_FUNC,
                        .arg_name = -1,
                        .index = left_index,
                        /* owns a deep copy of the frame it created under */
                        .init_time_frame = copy_frame(cur_frame)
                    }
                );
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf(" make macro:");
                    print_object(_OBJ_TABLE(cur_index), '\n');
                }
#endif
                break;
            /* argument binder */
            case OP_ARG:
                if (right_obj == NULL) {
                    append(&token_index_stack, &tree->rights[cur_index]);
                    continue;
                }
                if (
                    right_obj->type != TYPE_CALL
                    || right_obj->data.callable.is_macro
                ) {
                    print_runtime_error(
                        cur_token.pos,
                        "Right side of argument binder should be function"
                    );
                    is_error = 1;
                    break;
                }
                if (right_obj->data.callable.arg_name != -1) {
                    print_runtime_error(
                        cur_token.pos,
                        "Bind argument to a function that already has one"
                    );
                    is_error = 1;
                    break;
                }
                /* move right to current and modify arg_name */
                right_obj->data.callable.arg_name = left_token->name;
                _OBJ_TABLE(cur_index) = right_obj;
                _OBJ_TABLE(right_index) = NULL;
                break;
            /* assignment */
            case OP_ASSIGN:
                if (right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    continue;
                }
                if (frame_get(cur_frame, left_token->name) != NULL) {
                    sprintf(
                        ERR_MSG_BUF,
                        "Repeated initialization of identifier '%s'",
                        left_token->str
                    );
                    print_runtime_error(left_token->pos, ERR_MSG_BUF);
                    is_error = 1;
                    break;
                }

                /* set frame */
                frame_set(cur_frame, left_token->name, right_obj);
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf("initialized identifier '%s' to ", left_token->str);
                    print_object(right_obj, '\n');
                }
#endif
                /* move right to current */
                _OBJ_TABLE(cur_index) = right_obj;
                _OBJ_TABLE(right_index) = NULL;
                break;

            /* logic-and or logic-or */
            case OP_CONDAND:
            case OP_CONDOR:
                /* eval left first and eval right conditionally */
                if (left_obj == NULL) {
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* is_left_true | is_condor | is_eval_right
                   F            | F         | F
                   F            | T         | T
                   T            | F         | T
                   T            | T         | F             */
                if (to_bool(left_obj) != (cur_token.name == OP_CONDOR)) {
                    if (right_obj == NULL) {
                        append(&token_index_stack, &right_index);
                        continue;
                    }
                    /* move right to current */
                    _OBJ_TABLE(cur_index) = right_obj;
                    _OBJ_TABLE(right_index) = NULL;
                }
                else {
                    /* move left to current */
                    _OBJ_TABLE(cur_index) = left_obj;
                    _OBJ_TABLE(left_index) = NULL;
                }
                break;
            /* expresion seperator */
            case OP_EXPRSEP:
                if (left_obj == NULL && right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    append(&token_index_stack, &left_index);
                    continue;
                }
                /* move right to current */
                _OBJ_TABLE(cur_index) = right_obj;
                _OBJ_TABLE(right_index) = NULL;
                break;
            /* other operator */
            default:
                if (is_unary_op(cur_token.name) && left_obj == NULL) {
                    append(&token_index_stack, &left_index);
                    continue;
                }
                else if (left_obj == NULL && right_obj == NULL) {
                    append(&token_index_stack, &right_index);
                    append(&token_index_stack, &left_index);
                    continue;
                }
                returned_obj = exec_op(
                    tree, cur_frame, cur_token, left_obj, right_obj, is_debug
                );
                if (returned_obj->is_error) {
                    sprintf(
                        ERR_MSG_BUF,
                        "operator \"%s\" returns with error",
                        OP_STRS[cur_token.name]
                    );
                    print_runtime_error(cur_token.pos, ERR_MSG_BUF);
                }
#ifdef ENABLE_DEBUG
                if (is_debug) {
                    printf(
                        "^ (node %d) exec_op %s returned\n",
                        cur_index, OP_STRS[cur_token.name]
                    );
                }
#endif
                /* end eval if error */
                if (returned_obj->is_error) {
                    is_error = 1;
                    break;
                }
                /* don't need to copy because return_obj is temporary */
                _OBJ_TABLE(cur_index) = returned_obj;
            }
            // end switch
            if (is_error) {
                break; /* break the while loop */
            }
        }
        else if (cur_token.type == TOK_ID) {
            object_t* o = frame_get(cur_frame, cur_token.name);
            if (o == NULL) {
                sprintf(
                    ERR_MSG_BUF,
                    "Identifier '%s' used uninitialized",
                    cur_token.str
                );
                print_runtime_error(cur_token.pos, ERR_MSG_BUF);
                is_error = 1;
                break;
            }
            /* copy object from frame */
            _OBJ_TABLE(cur_index) = copy_object(o);
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf(
                    "  get identifiter '%s' (name=%d) from frame (addr=%p): ",
                    cur_token.str, cur_token.name, o
                );
                print_object(o, '\n');
            }
#endif
        }
        else if (cur_token.type == TOK_NUM) {
            _OBJ_TABLE(cur_index) = copy_object(tree->literals[cur_index]);
        }
        else {
            printf("eval_tree: bad token type: %d\n", cur_token.type);
            exit(RUNTIME_ERR_CODE);
        }
#ifdef ENABLE_DEBUG
        if (is_debug) {
            printf("< (node %d) eval result: ", cur_index);
            print_object(_OBJ_TABLE(cur_index), '\n');
            fflush(stdout);
        }
#endif
        pop(&token_index_stack);
    }

    /* prepare return object */
    if (is_error) {
#ifdef ENABLE_DEBUG
        if (is_debug) {
            printf("eval_tree return with error\n");
        }
#endif
        returned_obj = (object_t*) ERR_OBJECT_PTR;
    }
    else {
        returned_obj = copy_object(_OBJ_TABLE(entry_index));
    }

    /* free things */
    free_dynarr(&token_index_stack);
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("free obj_tables\n");
        fflush(stdout);
    }
#endif
    for (i = 0; i < tree_size; i++) {
        if (obj_table[i] != NULL) {
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf("free [%d]:", i); print_object(obj_table[i], '\n');
                fflush(stdout);
            }
#endif
            free_object(obj_table[i]);
        }
    }
    free(obj_table);
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("eval_tree returned ");
        print_object(returned_obj, '\n');
        fflush(stdout);
    }
#endif
    return returned_obj;
}
