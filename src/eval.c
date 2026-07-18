#include "eval.h"
#include "exec_operator.h"
#include "frame.h"
#include "reserved.h"
#include "utils/debug_flag.h"
#include "utils/errormsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int
is_bad_type(
    bytecode_t bytecode, int left_good_type, int right_good_type,
    object_t* left, object_t* right
)
{
    int left_passed, right_passed;
    if (left_good_type == NO_OPERAND) {
        left_passed = left == NULL;
    } else if (left_good_type == ANY_TYPE) {
        left_passed = left != NULL;
    } else {
        left_passed = left != NULL && left->type == left_good_type;
    }
    if (right_good_type == NO_OPERAND) {
        right_passed = right == NULL;
    } else if (right_good_type == ANY_TYPE) {
        right_passed = right != NULL;
    } else {
        right_passed = right != NULL && right->type == right_good_type;
    }
    if (left_passed && right_passed) {
        return 0;
    }
    sprintf(
        ERR_MSG_BUF,
        "Bad type for operator \"%s\": expect (%s, %s), get (%s, %s)",
        BYTECODE_OP_NAMES[bytecode.op],
        left_good_type < 0 ? "" : OBJ_TYPE_SIG_STR[left_good_type],
        right_good_type < 0 ? "" : OBJ_TYPE_SIG_STR[right_good_type],
        left == NULL ? "" : OBJ_TYPE_SIG_STR[left->type],
        right == NULL ? "" : OBJ_TYPE_SIG_STR[right->type]
    );
    print_runtime_error(bytecode.pos, ERR_MSG_BUF);
    return 1;
}

/* return 1 if check result is bad */
static inline int
pop_l_check(
    dynarr_object_ptr_t* stack, bytecode_t bc, object_t** left_ptr,
    object_type_enum left_good_type
)
{
    *left_ptr = *dynarr_object_ptr_back(stack);
    dynarr_object_ptr_pop(stack);
    return is_bad_type(bc, left_good_type, NO_OPERAND, *left_ptr, NULL);
}

/* return 1 if check result is bad */
static inline int
pop_lr_check(
    dynarr_object_ptr_t* stack, bytecode_t bc, object_t** left_ptr,
    object_t** right_ptr, object_type_enum left_good_type,
    object_type_enum right_good_type
)
{
    *right_ptr = *dynarr_object_ptr_back(stack);
    dynarr_object_ptr_pop(stack);
    *left_ptr = *dynarr_object_ptr_back(stack);
    dynarr_object_ptr_pop(stack);
    return is_bad_type(
        bc, left_good_type, right_good_type, *left_ptr, *right_ptr
    );
}

registers_t*
eval_bytecode(context_t context, bytecode_t bc)
{
    dynarr_object_ptr_t* stack = context.object_stack;
    frame_t* cur_frame = *dynarr_frameptr_back(context.frame_stack);
    registers_t* regs = dynarr_registers_back(context.regs_stack);
    object_t* left;
    object_t* right;
    object_t* tmp = NULL;

    /* increase insp before bytecode is execute so we don't need to worried
     * about jump and call and ret
     */
    regs->insp++;

    switch (bc.op) {
    case BOP_NOP:
        break;
    case BOP_EXTEND_ARG:
        regs->arg = (regs->arg | bc.arg) << 8;
        break;
    case BOP_PUSH_LITERAL:
        regs->arg = regs->arg | bc.arg;
        tmp = object_ref(context.tree->literals[regs->arg]);
        dynarr_object_ptr_append(stack, &tmp);
        break;
    case BOP_FRAME_GET:
        regs->arg = regs->arg | bc.arg;
        tmp = frame_get(cur_frame, regs->arg);
        if (tmp == NULL) {
            const char* err_msg = "Identifier '%s' used uninitialized";
            sprintf(
                ERR_MSG_BUF, err_msg, context.tree->id_code_str_map[regs->arg]
            );
            print_runtime_error(bc.pos, ERR_MSG_BUF);
            regs->errf = 1;
        } else {
            tmp = object_ref(tmp);
            dynarr_object_ptr_append(stack, &tmp);
        }
        break;
    case BOP_FRAME_SET:
        regs->arg = regs->arg | bc.arg;
        tmp = *dynarr_object_ptr_back(stack);
        if (!tmp || !frame_set(cur_frame, regs->arg, tmp)) {
            const char* err_msg = "Repeated initialization of identifier '%s'";
            sprintf(
                ERR_MSG_BUF, err_msg, context.tree->id_code_str_map[regs->arg]
            );
            print_runtime_error(bc.pos, ERR_MSG_BUF);
            regs->errf = 1;
        }
        break;
    case BOP_FRAME_SET_FROM_PAIR:
        regs->arg = regs->arg | bc.arg;
        tmp = *dynarr_object_ptr_back(stack);
        if (tmp->type != TYPE_PAIR) {
            regs->errf = 1;
            break;
        }
        exec_frame_set_from_pair(context, bc.pos, regs->arg, tmp);
        break;
    case BOP_POP:
        tmp = *dynarr_object_ptr_back(stack);
        dynarr_object_ptr_pop(stack);
        object_deref(tmp);
        tmp = NULL;
        break;
    case BOP_RET:
        dynarr_registers_pop(context.regs_stack);
        frame_free(cur_frame);
        dynarr_frameptr_pop(context.frame_stack);
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            int caller_entry_index = context.tree->root_index;
            if (context.frame_stack->size > 0) {
                /* if not root */
                frame_t* caller_frame
                    = *dynarr_frameptr_back(context.frame_stack);
                int* index_ptr = dynarr_int_back(&caller_frame->entry_indexs);
                if (index_ptr) {
                    caller_entry_index = *index_ptr;
                }
            }
            printf(
                "BOP_RET: return to caller (entry_index=%d)\n",
                caller_entry_index
            );
        }
#endif
        break;
    case BOP_JUMP:
        /* not implemented */
        print_runtime_error(bc.pos, "BOP_JUMP is not implemented");
        break;
    case BOP_JUMP_FALSE_OR_POP:
        tmp = *dynarr_object_ptr_back(stack);
        if (object_to_bool(tmp)) {
            dynarr_object_ptr_pop(stack);
            object_deref(tmp);
            tmp = NULL;
        } else {
            regs->arg = regs->arg | bc.arg;
            /* already account for the +1 before exec */
            regs->insp += regs->arg;
        }
        break;
    case BOP_JUMP_TRUE_OR_POP:
        tmp = *dynarr_object_ptr_back(stack);
        if (!object_to_bool(tmp)) {
            dynarr_object_ptr_pop(stack);
            object_deref(tmp);
            tmp = NULL;
        } else {
            regs->arg = regs->arg | bc.arg;
            /* already account for the +1 before exec */
            regs->insp += regs->arg;
        }
        break;
    case BOP_MAKE_FUNCT:
        regs->arg = regs->arg | bc.arg;
        {
            tmp = object_create(
                TYPE_CALL,
                (object_data_union)(callable_t) {
                    .is_macro = 0,
                    .builtin_name = NOT_BUILTIN_FUNC,
                    .arg_subtree_index = -1,
                    .index = regs->arg,
                    /* function owns a deep copy of frame it created under */
                    .init_frame = frame_copy(cur_frame),
                }
            );
        }
        dynarr_object_ptr_append(stack, &tmp);
        break;
    case BOP_MAKE_MACRO:
        regs->arg = regs->arg | bc.arg;
        tmp = object_create(
            TYPE_CALL,
            (object_data_union)(callable_t) {
                .is_macro = 1,
                .builtin_name = NOT_BUILTIN_FUNC,
                .arg_subtree_index = -1,
                .index = regs->arg,
                /* macro does not have frame */
                .init_frame = NULL,
            }
        );
        dynarr_object_ptr_append(stack, &tmp);
        break;
    case BOP_CALL:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_CALL, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        exec_call(context, bc.pos, left, right);
        /* check call depth */
        if (context.frame_stack->size > 1000) {
            print_runtime_error(bc.pos, "Call stack too deep (> 1000)");
            regs->errf = 1;
        }
        // dynarr_object_ptr_append(object_stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
#if DEPRECATED
    case BOP_MAP:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_CALL, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        tmp = exec_map(context, bc.pos, left, right);
        // dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_FILTER:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_CALL, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        tmp = exec_filter(context, bc.pos, left, right);
        // dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_REDUCE:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_CALL, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        tmp = exec_reduce(context, bc.pos, left, right);
        // dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
#endif
    case BOP_NEG:
        if (pop_l_check(stack, bc, &left, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            left->type, (object_data_union)number_neg(&left->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_NOT:
        if (pop_l_check(stack, bc, &left, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)(object_to_bool(left) ? ONE_NUMBER : ZERO_NUMBER)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_CEIL:
        if (pop_l_check(stack, bc, &left, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM, (object_data_union)number_ceil(&left->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_FLOOR:
        if (pop_l_check(stack, bc, &left, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM, (object_data_union)number_floor(&left->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_GETL:
        if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        tmp = object_ref(left->as.pair.left);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_GETR:
        if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        tmp = object_ref(left->as.pair.right);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_COND_CALL:
        if (pop_l_check(stack, bc, &left, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        if (left->type == TYPE_CALL) {
            exec_call(
                context, bc.pos, left,
                (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
            );
            object_deref(left);
        } else {
            dynarr_object_ptr_append(stack, &tmp);
        }
        break;
    case BOP_SWAP:
        if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_PAIR,
            (object_data_union)(pair_t) {
                .left = object_ref(left->as.pair.right),
                .right = object_ref(left->as.pair.left),
            }
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_EXP:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        if (right->as.number.denom.size != 1
            || right->as.number.denom.digit[0] != 1) {
            print_runtime_error(bc.pos, "Exponent must be integer");
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_exp(&left->as.number, &right->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_MUL:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_mul(&left->as.number, &right->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_DIV:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        if (right->as.number.numer.size == 0) {
            print_runtime_error(bc.pos, "Divided by zero");
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_div(&left->as.number, &right->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_MOD:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_mod(&left->as.number, &right->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_ADD:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_add(&left->as.number, &right->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_SUB:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_sub(&left->as.number, &right->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_LT:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)
                number_from_i32(number_lt(&left->as.number, &right->as.number))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_LE:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)
                number_from_i32(!number_lt(&right->as.number, &left->as.number))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_GT:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)
                number_from_i32(number_lt(&right->as.number, &left->as.number))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_GE:
        if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)
                number_from_i32(!number_lt(&left->as.number, &right->as.number))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_EQ:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM, (object_data_union)number_from_i32(object_eq(left, right))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_NE:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)number_from_i32(!object_eq(left, right))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_AND:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)
                number_from_i32(object_to_bool(left) && object_to_bool(right))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_OR:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_NUM,
            (object_data_union)
                number_from_i32(object_to_bool(left) || object_to_bool(right))
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_PAIR:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
            regs->errf = 1;
            break;
        }
        tmp = object_create(
            TYPE_PAIR,
            (object_data_union)(pair_t) {
                .left = object_ref(left),
                .right = object_ref(right),
            }
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_BIND_ARG:
        tmp = *dynarr_object_ptr_back(stack);
        if (is_bad_type(bc, NO_OPERAND, TYPE_CALL, NULL, tmp)) {
            regs->errf = 1;
            break;
        }
        if (tmp->as.callable.is_macro) {
            const char* err_msg
                = "Right side of argument binder should be function";
            print_runtime_error(bc.pos, err_msg);
            regs->errf = 1;
            break;
        }
        if (tmp->as.callable.arg_subtree_index != -1) {
            const char* err_msg
                = "Bind argument to a function that already has one";
            print_runtime_error(bc.pos, err_msg);
            regs->errf = 1;
            break;
        }
        regs->arg = regs->arg | bc.arg;
        tmp->as.callable.arg_subtree_index = regs->arg;
        break;
    case BOP_COND_PAIR_GET:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        /* check inside the pair */
        tmp = object_to_bool(left) ? right->as.pair.left : right->as.pair.right;
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_COND_PAIR_CALL:
        if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, TYPE_PAIR)) {
            regs->errf = 1;
            break;
        }
        /* check inside the pair */
        {
            object_t* res_object = object_to_bool(left) ? right->as.pair.left
                                                        : right->as.pair.right;
            if (res_object->type == TYPE_CALL) {
                exec_call(
                    context, bc.pos, res_object,
                    (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
                );
            } else {
                object_ref(res_object);
                dynarr_object_ptr_append(stack, &res_object);
            }
        }
        // dynarr_object_ptr_append(object_stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    default:
        sprintf(
            ERR_MSG_BUF, "eval_bytecode: bad bytecode: %d,%d\n", bc.op, bc.arg
        );
        print_runtime_error(bc.pos, ERR_MSG_BUF);
        exit(RUNTIME_ERR_CODE);
    }
    /* re-getting the back because there can be a call */
    regs = dynarr_registers_back(context.regs_stack);
    if (regs && tmp && tmp->is_error) {
        regs->errf = 1;
    }
    if (regs && bc.op != BOP_EXTEND_ARG) {
        regs->arg = 0;
    }
    return regs;
}

void
eval(context_t context)
{
    const syntax_tree_t* tree = context.tree;
    /* begin evaluation */
    while (1) {
        registers_t* cur_regs = dynarr_registers_back(context.regs_stack);
        bytecode_t bc = tree->bytecodes.data[cur_regs->insp];

#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            int i;
            printf(
                "inst=%d, arg=%u, object_stack=[", cur_regs->insp, cur_regs->arg
            );
            for (i = 0; i < context.object_stack->size; i++) {
                object_print(context.object_stack->data[i], ',');
                printf(" ");
            }
            printf("]\n");
            printf("exec bytecode: ");
            bytecode_print(bc);
            printf("\n");
        }
#endif

        cur_regs = eval_bytecode(context, bc);

        if (cur_regs == NULL || context.regs_stack->size == 0) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("eval returned ");
                object_print(
                    *dynarr_object_ptr_back(context.object_stack), '\n'
                );
                fflush(stdout);
            }
#endif
            break;
        }

        if (cur_regs->errf) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("early end because intermediate result is error\n");
            }
#endif
            break;
        }
    }
}

void
eval_root(const syntax_tree_t* syntax_tree)
{
    frame_t* root_frame = frame_new(NULL);
    dynarr_frameptr_t frame_stack = dynarr_frameptr_new();

    registers_t regs = {
        .arg = 0,
        .insp = syntax_tree_get_bytecode_start_index(
            syntax_tree, syntax_tree->root_index
        ),
        .errf = 0,
    };
    dynarr_registers_t regs_stack = dynarr_registers_new();

    dynarr_object_ptr_t object_stack = dynarr_object_ptr_new();

    context_t root_context = {
        .tree = syntax_tree,
        .frame_stack = &frame_stack,
        .regs_stack = &regs_stack,
        .object_stack = &object_stack,
    };
    object_t* final_result = NULL;

    dynarr_registers_append(&regs_stack, &regs);
    dynarr_frameptr_append(&frame_stack, &root_frame);

    eval(root_context);

    final_result = *dynarr_object_ptr_back(&object_stack);
    if (final_result == NULL || final_result->is_error) {
        int i;
        /* clear stack because it may not be cleared if not properly
         * returned */
        for (i = 0; i < object_stack.size; i++) {
            object_deref(object_stack.data[i]);
        }
    }
    object_deref(final_result);
    dynarr_object_ptr_free(&object_stack);

    dynarr_registers_free(&regs_stack);
    /* don't need to free frame because callee free it in RET */
    /* frame_free(root_frame); */
    dynarr_frameptr_free(&frame_stack);
}
