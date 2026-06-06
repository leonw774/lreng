#include "eval.h"
#include "exec_operator.h"
#include "frame.h"
#include "utils/debug_flag.h"
#include "utils/errormsg.h"
#include "reserved.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_OPRAND -1

static inline int
is_bad_type(
    bytecode_t bytecode, int left_good_type, int right_good_type,
    object_t* left, object_t* right
)
{
    int left_passed, right_passed;
    if (left_good_type == NO_OPRAND) {
        left_passed = left == NULL;
    } else if (left_good_type == TYPE_ANY) {
        left_passed = left != NULL;
    } else {
        left_passed = left != NULL && left->type == left_good_type;
    }
    if (right_good_type == NO_OPRAND) {
        right_passed = right == NULL;
    } else if (right_good_type == TYPE_ANY) {
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
        left_good_type == NO_OPRAND ? "" : OBJ_TYPE_STR[left_good_type],
        right_good_type == NO_OPRAND ? "" : OBJ_TYPE_STR[right_good_type],
        left == NULL ? "" : OBJ_TYPE_STR[left->type],
        right == NULL ? "" : OBJ_TYPE_STR[right->type]
    );
    print_runtime_error(bytecode.pos, ERR_MSG_BUF);
    return 1;
}

/* return 1 if check result is bad */
int
pop_l_and_check(
    dynarr_object_ptr_t* stack, bytecode_t bc, object_t** left_ptr,
    object_type_enum left_good_type
)
{
    *left_ptr = *dynarr_object_ptr_back(stack);
    dynarr_object_ptr_pop(stack);
    return is_bad_type(bc, left_good_type, NO_OPRAND, *left_ptr, NULL);
}

/* return 1 if check result is bad */
int
pop_lr_and_check(
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

registers_t
eval_bytecode(context_t context, registers_t regs, bytecode_t bc)
{
    dynarr_object_ptr_t* stack = context.stack;
    object_t* left;
    object_t* right;
    object_t* tmp = NULL;
    switch (bc.op) {
    case BOP_NOP:
        break;
    case BOP_EXTEND_ARG:
        regs.arg = ((regs.arg << 8) | bc.arg) << 8;
        break;
    case BOP_PUSH_LITERAL:
        regs.arg = regs.arg | bc.arg;
        tmp = object_ref(context.tree->literals[regs.arg]);
        dynarr_object_ptr_append(stack, &tmp);
        break;
    case BOP_FRAME_GET:
        regs.arg = regs.arg | bc.arg;
        tmp = frame_get(context.cur_frame, regs.arg);
        if (tmp == NULL) {
            const char* err_msg = "Identifier '%s' used uninitialized";
            sprintf(
                ERR_MSG_BUF, err_msg, context.tree->id_code_str_map[regs.arg]
            );
            print_runtime_error(bc.pos, ERR_MSG_BUF);
            regs.reto = (object_t*)ERR_OBJECT_PTR;
        } else {
            tmp = object_ref(tmp);
            dynarr_object_ptr_append(stack, &tmp);
        }
        break;
    case BOP_FRAME_SET:
        regs.arg = regs.arg | bc.arg;
        tmp = *dynarr_object_ptr_back(stack);
        if (!tmp || !frame_set(context.cur_frame, regs.arg, tmp)) {
            const char* err_msg = "Repeated initialization of identifier '%s'";
            sprintf(
                ERR_MSG_BUF, err_msg, context.tree->id_code_str_map[regs.arg]
            );
            print_runtime_error(bc.pos, ERR_MSG_BUF);
            regs.reto = (object_t*)ERR_OBJECT_PTR;
        }
        break;
    case BOP_POP:
        tmp = *dynarr_object_ptr_back(stack);
        dynarr_object_ptr_pop(stack);
        object_deref(tmp);
        tmp = NULL;
        break;
    case BOP_RET:
        /* top of stack is the returned object */
        tmp = *dynarr_object_ptr_back(stack);
        dynarr_object_ptr_pop(stack);
        regs.reto = tmp;
        break;
    case BOP_JUMP:
        /* not implemented */
        break;
    case BOP_JUMP_FALSE_OR_POP:
        tmp = *dynarr_object_ptr_back(stack);
        if (object_to_bool(tmp)) {
            dynarr_object_ptr_pop(stack);
            object_deref(tmp);
            tmp = NULL;
        } else {
            regs.arg = regs.arg | bc.arg;
            regs.insp += regs.arg;
        }
        break;
    case BOP_JUMP_TRUE_OR_POP:
        tmp = *dynarr_object_ptr_back(stack);
        if (!object_to_bool(tmp)) {
            dynarr_object_ptr_pop(stack);
            object_deref(tmp);
            tmp = NULL;
        } else {
            regs.arg = regs.arg | bc.arg;
            regs.insp += regs.arg;
        }
        break;
    case BOP_MAKE_FUNCT:
        regs.arg = regs.arg | bc.arg;
        tmp = object_create(
            TYPE_CALL,
            (object_data_union)(callable_t) {
                .is_macro = 0,
                .builtin_name = NOT_BUILTIN_FUNC,
                .arg_code = -1,
                .index = regs.arg,
                /* function owns copy of frame it created under */
                .init_frame = frame_copy(context.cur_frame),
            }
        );
        dynarr_object_ptr_append(stack, &tmp);
        break;
    case BOP_MAKE_MACRO:
        regs.arg = regs.arg | bc.arg;
        tmp = object_create(
            TYPE_CALL,
            (object_data_union)(callable_t) {
                .is_macro = 1,
                .builtin_name = NOT_BUILTIN_FUNC,
                .arg_code = -1,
                .index = regs.arg,
                /* macro does not have frame */
                .init_frame = NULL,
            }
        );
        dynarr_object_ptr_append(stack, &tmp);
        break;
    case BOP_CALL:
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_CALL, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = exec_call(context, bc.pos, left, right);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_MAP:
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_CALL, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = exec_map(context, bc.pos, left, right);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_FILTER:
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_CALL, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = exec_filter(context, bc.pos, left, right);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_REDUCE:
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_CALL, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = exec_reduce(context, bc.pos, left, right);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case BOP_NEG:
        if (pop_l_and_check(stack, bc, &left, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = object_create(
            left->type, (object_data_union)number_neg(&left->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_NOT:
        if (pop_l_and_check(stack, bc, &left, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_l_and_check(stack, bc, &left, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = object_create(
            TYPE_NUM, (object_data_union)number_ceil(&left->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_FLOOR:
        if (pop_l_and_check(stack, bc, &left, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = object_create(
            TYPE_NUM, (object_data_union)number_floor(&left->as.number)
        );
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_GETL:
        if (pop_l_and_check(stack, bc, &left, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = object_ref(left->as.pair.left);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_GETR:
        if (pop_l_and_check(stack, bc, &left, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        tmp = object_ref(left->as.pair.right);
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_COND_CALL:
        if (pop_l_and_check(stack, bc, &left, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        if (left->type == TYPE_CALL) {
            tmp = exec_call(
                context, bc.pos, left,
                (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
            );
        } else {
            tmp = object_ref(left);
        }
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        break;
    case BOP_SWAP:
        if (pop_l_and_check(stack, bc, &left, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        if (right->as.number.denom.size != 1
            || right->as.number.denom.digit[0] != 1) {
            print_runtime_error(bc.pos, "Exponent must be integer");
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        if (right->as.number.numer.size == 0) {
            print_runtime_error(bc.pos, "Divided by zero");
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_ANY)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
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
        if (is_bad_type(bc, NO_OPRAND, TYPE_CALL, NULL, tmp)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        if (tmp->as.callable.is_macro) {
            const char* err_msg
                = "Right side of argument binder should be function";
            print_runtime_error(bc.pos, err_msg);
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        if (tmp->as.callable.arg_code != -1) {
            const char* err_msg
                = "Bind argument to a function that already has one";
            print_runtime_error(bc.pos, err_msg);
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        regs.arg = regs.arg | bc.arg;
        tmp->as.callable.arg_code = regs.arg;
        break;
    case OP_COND_PAIR_GET:
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        /* check inside the pair */
        {
            object_t* right_left = right->as.pair.left;
            object_t* right_right = right->as.pair.right;
            tmp = (object_to_bool(left) ? right_left : right_right);
        }
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    case OP_COND_PAIR_CALL:
        if (pop_lr_and_check(stack, bc, &left, &right, TYPE_ANY, TYPE_PAIR)) {
            regs.reto = (object_t*)ERR_OBJECT_PTR;
            break;
        }
        /* check inside the pair */
        {
            object_t* right_left = right->as.pair.left;
            object_t* right_right = right->as.pair.right;
            if (is_bad_type(
                    bc, TYPE_CALL, TYPE_CALL, right_left, right_right
                )) {
                regs.reto = (object_t*)ERR_OBJECT_PTR;
                break;
            }
            tmp = exec_call(
                context, bc.pos,
                (object_to_bool(left) ? right_left : right_right),
                (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
            );
        }
        dynarr_object_ptr_append(stack, &tmp);
        object_deref(left);
        object_deref(right);
        break;
    default:
        sprintf(ERR_MSG_BUF, "eval: bad bytecode: %d,%d\n", bc.op, bc.arg);
        print_runtime_error(bc.pos, ERR_MSG_BUF);
        exit(RUNTIME_ERR_CODE);
    }
    if (tmp && tmp->is_error) {
        regs.reto = (object_t*)ERR_OBJECT_PTR;
    }
    return regs;
}

object_t*
eval(context_t context, const int entry_index)
{
    const syntax_tree_t* tree = context.tree;
    dynarr_bytecode_t bytecodes
        = syntax_tree_get_bytecode_from_node_index(tree, entry_index);
    registers_t regs = { .arg = 0, .insp = 0, .reto = NULL };
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

    /* begin evaluation */
    while (regs.reto == NULL) {
        bytecode_t bc = bytecodes.data[regs.insp];

#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            int i;
            printf("inst=%d, arg=%u, eval_stack=[", regs.insp, regs.arg);
            for (i = 0; i < context.stack->size; i++) {
                object_print(context.stack->data[i], ',');
                printf(" ");
            }
            printf("]\n");
            printf("exec bytecode: ");
            bytecode_print(bc);
            printf("\n");
        }
#endif

        regs = eval_bytecode(context, regs, bc);

        regs.insp++;
        if (bc.op != BOP_EXTEND_ARG) {
            regs.arg = 0;
        }
    }

    if (regs.reto && regs.reto->is_error) {
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("early return because intermediate result is error\n");
        }
#endif
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("eval returned ");
        object_print(regs.reto, '\n');
        fflush(stdout);
    }
#endif
    return regs.reto;
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
        /* clear stack because it may not be cleared if not properly
         * returned */
        for (i = 0; i < stack.size; i++) {
            object_deref(stack.data[i]);
        }
    }
    object_deref(final_result);
    dynarr_object_ptr_free(&stack);
    frame_free(root_frame);
    free(root_frame);
}
