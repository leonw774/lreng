#include "transpile.h"
#include "objects.h"
#include "syntax_tree_iter.h"
#include "transpile/utils.meta.h"
#include "utils/dynarr_char.h"
#include "utils/global_flags.h"
#include <assert.h>

#define NUMBER_PRECISION 16
#define LITERAL_BUFFER_SIZE 256
#define BYTECODE_BUFFER_SIZE 1024

char*
get_transpile_util_codes()
{
    src_transpile_utils_meta_c[src_transpile_utils_meta_c_len] = '\0';
    return (char*)src_transpile_utils_meta_c;
}

char*
transpile_literal(object_t* literal_object)
{
    static char BUFFER[LITERAL_BUFFER_SIZE];
    char* tmp_cstr;
    dynarr_char_t tmp_str;

    if (literal_object->type == TYPE_NUM) {
        tmp_str = number_to_dec_string(
            &literal_object->as.number, NUMBER_PRECISION
        );
        tmp_cstr = dynarr_char_to_str(&tmp_str);
        assert(tmp_cstr);
        snprintf(BUFFER, LITERAL_BUFFER_SIZE, "(from_number(%s))", tmp_cstr);
        free(tmp_cstr);
        dynarr_char_free(&tmp_str);
    } else if (literal_object->type == TYPE_NULL) {
        sprintf(BUFFER, "(NULL_OBJPTR)");
    }
    return BUFFER;
}

char*
transpile_bytecode(
    syntax_tree_t* tree, bytecode_t bc, dynarr_int_t* obj_id_stack,
    int cur_obj_id, int reg_arg
)
{
    static char BUFFER[BYTECODE_BUFFER_SIZE];
    char* code_tmpl;
    int left_id, right_id;
    // char* tmp_cstr;

    BUFFER[0] = '\0';

    switch (bc.op) {
    case BOP_NOP:
        break;
    case BOP_PUSH_LITERAL:
        reg_arg |= bc.arg;
        sprintf(
            BUFFER, "object_t* var_%d = %s;\n", cur_obj_id,
            transpile_literal(tree->literals[reg_arg])
        );
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_FRAME_GET:
        reg_arg |= bc.arg;
        snprintf(
            BUFFER, BYTECODE_BUFFER_SIZE,
            "object_t* var_%d = frame_get(FRAME, %d);\n", cur_obj_id, reg_arg
        );
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_FRAME_SET:
        reg_arg |= bc.arg;
        snprintf(
            BUFFER, BYTECODE_BUFFER_SIZE,
            "object_t* var_%d = frame_set(FRAME, %d, var_%d);\n", cur_obj_id,
            reg_arg, *dynarr_int_back(obj_id_stack)
        );
        dynarr_int_pop(obj_id_stack);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
        //     case BOP_FRAME_SET_FROM_PAIR:
        //         reg_arg |= bc.arg;
        //         tmp = *dynarr_object_ptr_back(stack);
        //         if (tmp->type != TYPE_PAIR) {
        //             regs->errf = 1;
        //             break;
        //         }
        //         exec_frame_set_from_pair(context, bc.pos, regs->arg, tmp);
        //         break;
        //     case BOP_POP:
        //         tmp = *dynarr_object_ptr_back(stack);
        //         dynarr_object_ptr_pop(stack);
        //         object_deref(tmp);
        //         tmp = NULL;
        //         break;
    case BOP_RET:
        snprintf(BUFFER, BYTECODE_BUFFER_SIZE, "frame_pop(FRAME);\nreturn;\n");
        break;
        // case BOP_JUMP:
        //     /* not implemented */
        //     print_runtime_error(bc.pos, "BOP_JUMP is not implemented");
        //     break;
        // case BOP_JUMP_FALSE_OR_POP:
        //     tmp = *dynarr_object_ptr_back(stack);
        //     if (object_to_bool(tmp)) {
        //         dynarr_object_ptr_pop(stack);
        //         object_deref(tmp);
        //         tmp = NULL;
        //     } else {
        //         reg_arg |= bc.arg;
        //         /* already account for the +1 before exec */
        //         regs->insp += regs->arg;
        //     }
        //     break;
        // case BOP_JUMP_TRUE_OR_POP:
        //     tmp = *dynarr_object_ptr_back(stack);
        //     if (!object_to_bool(tmp)) {
        //         dynarr_object_ptr_pop(stack);
        //         object_deref(tmp);
        //         tmp = NULL;
        //     } else {
        //         reg_arg |= bc.arg;
        //         /* already account for the +1 before exec */
        //         regs->insp += regs->arg;
        //     }
        //     break;
        // case BOP_MAKE_FUNCT:
        //     reg_arg |= bc.arg;
        //     {
        //         tmp = object_create(
        //             TYPE_CALL,
        //             (object_data_union)(callable_t) {
        //                 .is_macro = 0,
        //                 .builtin_name = NOT_BUILTIN_FUNC,
        //                 .arg_subtree_index = -1,
        //                 .index = regs->arg,
        //                 /* function owns a deep copy of frame it created
        //                 under */ .init_frame = frame_copy(cur_frame),
        //             }
        //         );
        //     }
        //     dynarr_object_ptr_append(stack, &tmp);
        //     break;
        // case BOP_MAKE_MACRO:
        //     reg_arg |= bc.arg;
        //     tmp = object_create(
        //         TYPE_CALL,
        //         (object_data_union)(callable_t) {
        //             .is_macro = 1,
        //             .builtin_name = NOT_BUILTIN_FUNC,
        //             .arg_subtree_index = -1,
        //             .index = regs->arg,
        //             /* macro does not have frame */
        //             .init_frame = NULL,
        //         }
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     break;
        // case BOP_CALL:
        //     if (pop_lr_check(stack, bc, &left, &right, TYPE_CALL, ANY_TYPE))
        //     {
        //         regs->errf = 1;
        //         break;
        //     }
        //     exec_call(context, bc.pos, left, right);
        //     /* check call depth */
        //     if (context.frame_stack->size > 1000) {
        //         print_runtime_error(bc.pos, "Call stack too deep (> 1000)");
        //         regs->errf = 1;
        //     }
        //     // dynarr_object_ptr_append(object_stack, &tmp);
        //     object_deref(left);
        //     object_deref(right);
        //     break;
        // case BOP_NEG:
        //     if (pop_l_check(stack, bc, &left, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         left->type, (object_data_union)number_neg(&left->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_NOT:
        //     if (pop_l_check(stack, bc, &left, ANY_TYPE)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM,
        //         (object_data_union)(object_to_bool(left) ? ONE_NUMBER :
        //         ZERO_NUMBER)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_CEIL:
        //     if (pop_l_check(stack, bc, &left, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM, (object_data_union)number_ceil(&left->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_FLOOR:
        //     if (pop_l_check(stack, bc, &left, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM, (object_data_union)number_floor(&left->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_GETL:
        //     if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_ref(left->as.pair.left);
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_GETR:
        //     if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_ref(left->as.pair.right);
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_COND_CALL:
        //     if (pop_l_check(stack, bc, &left, ANY_TYPE)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     if (left->type == TYPE_CALL) {
        //         exec_call(
        //             context, bc.pos, left,
        //             (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
        //         );
        //         object_deref(left);
        //     } else {
        //         dynarr_object_ptr_append(stack, &tmp);
        //     }
        //     break;
        // case BOP_SWAP:
        //     if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_PAIR,
        //         (object_data_union)(pair_t) {
        //             .left = object_ref(left->as.pair.right),
        //             .right = object_ref(left->as.pair.left),
        //         }
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     break;
        // case BOP_EXP:
        //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     if (right->as.number.denom.size != 1
        //         || right->as.number.denom.digit[0] != 1) {
        //         print_runtime_error(bc.pos, "Exponent must be integer");
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM,
        //         (object_data_union)number_exp(&left->as.number,
        //         &right->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     object_deref(right);
        //     break;
        // case BOP_MUL:
        //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM,
        //         (object_data_union)number_mul(&left->as.number,
        //         &right->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     object_deref(right);
        //     break;
        // case BOP_DIV:
        //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     if (right->as.number.numer.size == 0) {
        //         print_runtime_error(bc.pos, "Divided by zero");
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM,
        //         (object_data_union)number_div(&left->as.number,
        //         &right->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     object_deref(right);
        //     break;
        // case BOP_MOD:
        //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
        //         regs->errf = 1;
        //         break;
        //     }
        //     tmp = object_create(
        //         TYPE_NUM,
        //         (object_data_union)number_mod(&left->as.number,
        //         &right->as.number)
        //     );
        //     dynarr_object_ptr_append(stack, &tmp);
        //     object_deref(left);
        //     object_deref(right);
        //     break;
    case BOP_ADD:
        left_id = *dynarr_int_back(obj_id_stack);
        dynarr_int_pop(obj_id_stack);
        right_id = *dynarr_int_back(obj_id_stack);
        dynarr_int_pop(obj_id_stack);
        code_tmpl = "object_t* var_%d = from_number(as_number(var_%d) + "
                    "as_number(var_%d));\n";
        sprintf(BUFFER, code_tmpl, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    // case BOP_SUB:
    //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)number_sub(&left->as.number,
    //         &right->as.number)
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_LT:
    //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)
    //             number_from_i32(number_lt(&left->as.number,
    //             &right->as.number))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_LE:
    //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)
    //             number_from_i32(!number_lt(&right->as.number,
    //             &left->as.number))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_GT:
    //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)
    //             number_from_i32(number_lt(&right->as.number,
    //             &left->as.number))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_GE:
    //     if (pop_lr_check(stack, bc, &left, &right, TYPE_NUM, TYPE_NUM)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)
    //             number_from_i32(!number_lt(&left->as.number,
    //             &right->as.number))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_EQ:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM, (object_data_union)number_from_i32(object_eq(left,
    //         right))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_NE:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)number_from_i32(!object_eq(left, right))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_AND:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)
    //             number_from_i32(object_to_bool(left) &&
    //             object_to_bool(right))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_OR:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_NUM,
    //         (object_data_union)
    //             number_from_i32(object_to_bool(left) ||
    //             object_to_bool(right))
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_PAIR:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, ANY_TYPE)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_create(
    //         TYPE_PAIR,
    //         (object_data_union)(pair_t) {
    //             .left = object_ref(left),
    //             .right = object_ref(right),
    //         }
    //     );
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_BIND_ARG:
    //     tmp = *dynarr_object_ptr_back(stack);
    //     if (is_bad_type(bc, NO_OPERAND, TYPE_CALL, NULL, tmp)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     if (tmp->as.callable.is_macro) {
    //         const char* err_msg
    //             = "Right side of argument binder should be function";
    //         print_runtime_error(bc.pos, err_msg);
    //         regs->errf = 1;
    //         break;
    //     }
    //     if (tmp->as.callable.arg_subtree_index != -1) {
    //         const char* err_msg
    //             = "Bind argument to a function that already has one";
    //         print_runtime_error(bc.pos, err_msg);
    //         regs->errf = 1;
    //         break;
    //     }
    //     reg_arg |= bc.arg;
    //     tmp->as.callable.arg_subtree_index = regs->arg;
    //     break;
    // case BOP_COND_PAIR_GET:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, TYPE_PAIR)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     /* check inside the pair */
    //     tmp = object_to_bool(left) ? right->as.pair.left :
    //     right->as.pair.right; dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    // case BOP_COND_PAIR_CALL:
    //     if (pop_lr_check(stack, bc, &left, &right, ANY_TYPE, TYPE_PAIR)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     /* check inside the pair */
    //     {
    //         object_t* res_object = object_to_bool(left) ? right->as.pair.left
    //                                                     :
    //                                                     right->as.pair.right;
    //         if (res_object->type == TYPE_CALL) {
    //             exec_call(
    //                 context, bc.pos, res_object,
    //                 (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
    //             );
    //         } else {
    //             object_ref(res_object);
    //             dynarr_object_ptr_append(stack, &res_object);
    //         }
    //     }
    //     // dynarr_object_ptr_append(object_stack, &tmp);
    //     object_deref(left);
    //     object_deref(right);
    //     break;
    default:
        sprintf(
            ERR_MSG_BUF, "transpile_bytecode: bad bytecode: %d,%d\n", bc.op,
            bc.arg
        );
        print_runtime_error(bc.pos, ERR_MSG_BUF);
        exit(RUNTIME_ERR_CODE);
    }
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(BUFFER);
    }
#endif
    return BUFFER;
}

char*
transpile_bytecode_section(
    syntax_tree_t* tree, int entry_index, bytecode_t* bc_start,
    bytecode_t* bc_end
)
{
    size_t sect_code_len = 0;
    size_t sect_code_cap = BYTECODE_BUFFER_SIZE * 8;
    char* sect_code_cstr = malloc(sect_code_cap);

    const char* func_start = "void\nfunc_%d(frame_t* FRAME)\n{\n";
    const char* top_start = "void\ntop(frame_t* FRAME)\n{\n";
    const char* func_end = "}\n\n";

    int cur_obj_id = 1;
    dynarr_int_t obj_id_stack = dynarr_int_new();
    size_t reg_arg = 0;

    long int i;
    long int bc_count = bc_end - bc_start;
    assert(bc_count);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "transpile_bytecode_section: %p ~ %p (%ld)\n", bc_start, bc_end,
            bc_count
        );
    }
#endif

    if (tree->root_index == entry_index) {
        sprintf(sect_code_cstr, top_start);
    } else {
        sprintf(sect_code_cstr, func_start, entry_index);
    }

    dynarr_int_append(&obj_id_stack, &cur_obj_id);
    for (i = 0; i < bc_count; i++) {
        bytecode_t bc = bc_start[i];
        if (bc.op == BOP_EXTEND_ARG) {
            reg_arg += (bc.arg << 8 | bc.arg) << 8;
        } else {
            const char* bc_code_cstr = transpile_bytecode(
                tree, bc, &obj_id_stack, cur_obj_id, reg_arg
            );
            sect_code_len += strlen(bc_code_cstr);
            if (sect_code_len >= sect_code_cap) {
                sect_code_cap *= 2;
                sect_code_cstr = realloc(sect_code_cstr, sect_code_cap);
            }
            strcat(sect_code_cstr, bc_code_cstr);
            cur_obj_id++;
            reg_arg = 0;
        }
    }

    sect_code_len += strlen(func_end);
    if (sect_code_len + 1 >= sect_code_cap) {
        sect_code_cap += strlen(func_end);
        sect_code_cstr = realloc(sect_code_cstr, sect_code_cap);
    }
    strcat(sect_code_cstr, func_end);

    return sect_code_cstr;
}

void
transpile(syntax_tree_t* tree)
{
    const int bytecode_section_count = tree->bytecode_start_index.size - 1;
    char** transpiled_codes = calloc(tree->bytecodes.size, sizeof(char*));
    int i;

    // transpile bytecodes
    for (i = 0; i < bytecode_section_count; i++) {
        int start_index = tree->bytecode_start_index.data[i];
        int end_index = tree->bytecode_start_index.data[i + 1];
        transpiled_codes[i] = transpile_bytecode_section(
            tree,
            tree->entry_indexs.data[i],
            dynarr_bytecode_at(&tree->bytecodes, start_index),
            dynarr_bytecode_at(&tree->bytecodes, end_index)
        );
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("====================\n");
    }
#endif

    {
        const char* util_codes = get_transpile_util_codes(tree);
        char* concated_codes = NULL;
        size_t total_code_len = strlen(util_codes) + 1; // add 1 for ending NULL
        for (i = 0; i < bytecode_section_count; i++) {
            total_code_len += strlen(transpiled_codes[i]);
        }

        concated_codes = calloc(total_code_len, 1);
        strcat(concated_codes, util_codes);
        for (i = 0; i < bytecode_section_count; i++) {
            strcat(concated_codes, transpiled_codes[i]);
        }

        printf(concated_codes);

        free(concated_codes);
    }

    for (i = 0; i < bytecode_section_count; i++) {
        free(transpiled_codes[i]);
    }
}
