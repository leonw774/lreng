#include "transpile.h"
#include "objects.h"
#include "reserved.h"
#include "syntax_tree_iter.h"
#include "transpile/utils.meta.h"
#include "utils/dynarr_char.h"
#include "utils/global_flags.h"
#include <assert.h>
#include <unistd.h>

#define NUMBER_PRECISION 16
#define LITERAL_BUFFER_SIZE 255
#define BYTECODE_BUFFER_SIZE 4095

char*
transpile_frame_set_unpack(
    syntax_tree_t* tree, int stack_size, const int assignee_index
)
{
    char* left_buffer = malloc(LITERAL_BUFFER_SIZE + 1);
    char* right_buffer = malloc(LITERAL_BUFFER_SIZE + 1);
    size_t left_size = LITERAL_BUFFER_SIZE;
    size_t right_size = LITERAL_BUFFER_SIZE;
    const token_t left_token = tree->tokens.data[tree->lefts[assignee_index]];
    const token_t right_token = tree->tokens.data[tree->rights[assignee_index]];

    if (left_token.type != TOK_ID) {
        snprintf(
            left_buffer, LITERAL_BUFFER_SIZE,
            "    object_t* s_%d_%d = frame_set(FRAME, %d, "
            "as_pair(s_%d_%d).left ); // unpack branch\n",
            stack_size, tree->lefts[assignee_index], left_token.code,
            stack_size, assignee_index
        );
        char* left_child_buffer = transpile_frame_set_unpack(
            tree, stack_size, tree->lefts[assignee_index]
        );
        left_size += strlen(left_child_buffer) + 1;
        left_buffer = realloc(left_buffer, left_size);
        strcat(left_buffer, left_child_buffer);
        free(left_child_buffer);
    } else {
        snprintf(
            left_buffer, LITERAL_BUFFER_SIZE,
            "    frame_set(FRAME, %d, as_pair(s_%d_%d).left); // unpack "
            "leaf\n",
            left_token.code, stack_size, assignee_index
        );
    }
    if (right_token.type != TOK_ID) {
        snprintf(
            right_buffer, LITERAL_BUFFER_SIZE,
            "    object_t* s_%d_%d = frame_set(FRAME, %d, "
            "as_pair(s_%d_%d).right); // unpack branch\n",
            stack_size, tree->rights[assignee_index], right_token.code,
            stack_size, assignee_index
        );
        char* right_child_buffer = transpile_frame_set_unpack(
            tree, stack_size, tree->rights[assignee_index]
        );
        right_size += strlen(right_child_buffer) + 1;
        right_buffer = realloc(left_buffer, right_size);
        strcat(right_buffer, right_child_buffer);
        free(right_child_buffer);
    } else {
        snprintf(
            right_buffer, LITERAL_BUFFER_SIZE,
            "    frame_set(FRAME, %d, as_pair(s_%d_%d).right); // unpack "
            "leaf\n",
            right_token.code, stack_size, assignee_index
        );
    }
    left_buffer = realloc(left_buffer, left_size + right_size);
    strcat(left_buffer, right_buffer);
    free(right_buffer);
    return left_buffer;
}

char*
transpile_literal(object_t* literal_object)
{
    char* buffer = malloc(LITERAL_BUFFER_SIZE + 1);
    char* tmp_cstr;
    dynarr_char_t tmp_str;

    if (literal_object->type == TYPE_NUM) {
        tmp_str = number_to_dec_string(
            &literal_object->as.number, NUMBER_PRECISION
        );
        tmp_cstr = dynarr_char_to_str(&tmp_str);
        assert(tmp_cstr);
        snprintf(buffer, LITERAL_BUFFER_SIZE, "(from_number(%s))", tmp_cstr);
        free(tmp_cstr);
        dynarr_char_free(&tmp_str);
    } else if (literal_object->type == TYPE_NULL) {
        snprintf(buffer, LITERAL_BUFFER_SIZE, "(object_t*)(NULL_OBJPTR)");
    } else if (literal_object->type == TYPE_CALL) {
        snprintf(
            buffer, LITERAL_BUFFER_SIZE,
            "(from_callable((callable_t){ .func_ptr = %s, .init_frame = NULL, "
            ".arg_code = -1 }))",
            RESERVED_IDS[literal_object->as.callable.builtin_name]
        );
    } else {
        print_runtime_error((linecol_t) { 0, 0 }, "un-compilable literal");
    }
    return buffer;
}

char*
transpile_bytecode(
    syntax_tree_t* tree, bytecode_t bc, size_t bc_index, int top_index,
    int reg_arg
)
{
    static char buffer[BYTECODE_BUFFER_SIZE];
    char* tmp_buffer;
    char* tmplt;
    buffer[0] = '\0';

    switch (bc.op) {
    case BOP_NOP:
        break;
    case BOP_PUSH_LIT:
        reg_arg |= bc.arg;
        tmplt = "inst_%d:\n    s_%d = %s; // PUSH_LITERAL\n";
        tmp_buffer = transpile_literal(tree->literals[reg_arg]);
        sprintf(buffer, tmplt, bc_index, top_index + 1, tmp_buffer);
        free(tmp_buffer);
        tmp_buffer = NULL;
        break;
    case BOP_FGET:
        reg_arg |= bc.arg;
        tmplt = "inst_%d:\n"
                "    s_%d = frame_get(FRAME, %d); // FRAME_GET\n"
                "    if (!s_%d) {\n"
                "        puts(\"\'%s\' is used uninitialized\");\n"
                "        exit(EXIT_FAILURE);\n"
                "    }\n";
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, tmplt, bc_index, top_index + 1,
            reg_arg, top_index + 1, tree->id_code_str_map[reg_arg]
        );
        break;
    case BOP_FSET:
        reg_arg |= bc.arg;
        tmplt = "inst_%d:\n"
                "    frame_set(FRAME, %d, s_%d); // FRAME_SET\n";
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, tmplt, bc_index, reg_arg, top_index
        );
        break;
    case BOP_FSET_UNPACK:
        reg_arg |= bc.arg;
        tmplt = "inst_%d:\n"
                "    object_t* s_%d_%d = s_%d; // FRAME_SET_UNPACK\n%s";
        tmp_buffer = transpile_frame_set_unpack(tree, top_index, reg_arg);
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, tmplt, bc_index, top_index, reg_arg,
            top_index, tmp_buffer
        );
        free(tmp_buffer);
        tmp_buffer = NULL;
        break;
    case BOP_POP:
        tmplt = "inst_%d:\n";
        snprintf(buffer, BYTECODE_BUFFER_SIZE, tmplt, bc_index);
        break;
    case BOP_RET:
        tmplt = "inst_%d:\n"
                "    frame_pop(FRAME); // RET\n"
                "    return s_%d; // RET\n";
        snprintf(buffer, BYTECODE_BUFFER_SIZE, tmplt, bc_index, top_index);
        break;
    case BOP_BF_OR_POP:
        tmplt = "inst_%d:\n"
                "    if (!object_to_bool(s_%d)) goto inst_%d;\n";
        /* we can safely assume that the jump does not happen */
        break;
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
    // case BOP_BT_OR_POP:
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
    case BOP_CALL:
        tmplt = "inst_%d:\n"
                "    s_%d = exec_call(FRAME, s_%d, s_%d); // CALL\n";
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, tmplt, bc_index, top_index - 1,
            top_index - 1, top_index
        );
        break;
    case BOP_NEG:
        tmplt = "inst_%d:\n"
                "    s_%d = from_number(-as_number(s_%d)); // NEG\n";
        sprintf(buffer, tmplt, bc_index, top_index, top_index);
        break;
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
    // case BOP_PGETL:
    //     if (pop_l_check(stack, bc, &left, TYPE_PAIR)) {
    //         regs->errf = 1;
    //         break;
    //     }
    //     tmp = object_ref(left->as.pair.left);
    //     dynarr_object_ptr_append(stack, &tmp);
    //     object_deref(left);
    //     break;
    // case BOP_PGETR:
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
    case BOP_MUL:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) * as_number(s_%d)); // MUL\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_DIV:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) * as_number(s_%d)); // DIV\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_MOD:
        tmplt = "inst_%d:\n    s_%d = from_number(floor_modulo("
                "as_number(s_%d), as_number(s_%d))); // MOD\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_ADD:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) + as_number(s_%d)); // ADD\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_SUB:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) - as_number(s_%d)); // SUB\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_LT:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) < as_number(s_%d)); // LT\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_LE:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) <= as_number(s_%d)); // LT\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_GT:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) > as_number(s_%d)); // LT\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_GE:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "as_number(s_%d) >= as_number(s_%d)); // LT\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_EQ:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "object_equal(s_%d, s_%d)); // EQ\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_NE:
        tmplt = "inst_%d:\n    s_%d = from_number("
                "1.f - object_equal(s_%d, s_%d)); // EQ\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_AND:
        tmplt = "inst_%d:\n    s_%d = from_number(object_to_bool(s_%d) "
                "&& object_to_bool(s_%d)); // AND\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_OR:
        tmplt = "inst_%d:\n    s_%d = from_number(object_to_bool(s_%d) "
                "|| object_to_bool(s_%d)); // OR\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
    case BOP_PAIR:
        tmplt = "inst_%d:\n    s_%d = from_pair("
                "(pair_t) { .left = s_%d, .right = s_%d }); // PAIR\n";
        sprintf(
            buffer, tmplt, bc_index, top_index - 1, top_index - 1, top_index
        );
        break;
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
    // case BOP_COND_PGET:
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
    // case BOP_COND_PCALL:
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
    return buffer;
}

void
transpile_bytecode_section(
    syntax_tree_t* tree, int out_fd, int entry_index, int bc_start, int bc_end
)
{
    char sect_code_cstr[128];
    static const char* top_start
        = "object_t*\ntop(frame_t* FRAME, object_t* arg)\n{\n";
    static const char* func_start
        = "object_t*\nfunc_%d(frame_t* FRAME, object_t* arg)\n{\n";
    static const char* func_end = "}\n\n";

    int stack_size = 0;
    int stack_size_max = 0;
    size_t reg_arg = 0;

    int i;
    int bc_count = bc_end - bc_start;
    assert(bc_count > 0);

    /* calculate max stack size in this function */
    for (i = 0; i < bc_count; i++) {
        bytecode_t bc = tree->bytecodes.data[bc_start + i];
        stack_size += bytecode_stack_diff(bc.op);
        if (stack_size > stack_size_max) {
            stack_size_max = stack_size;
        }
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "// transpile_bytecode_section: %d ~ %d (%d)\n"
            "// stack_size_max: %d\n",
            bc_start, bc_end, bc_count, stack_size_max
        );
    }
#endif

    /* render function start */
    if (tree->root_index == entry_index) {
        snprintf(sect_code_cstr, 128, "%s", top_start);
    } else {
        snprintf(sect_code_cstr, 128, func_start, entry_index);
    }
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(sect_code_cstr);
    }
#endif
    write(out_fd, sect_code_cstr, strlen(sect_code_cstr));

    /* render stack slots */
    for (i = 0; i < stack_size_max; i++) {
        snprintf(sect_code_cstr, 128, "   object_t* s_%d;\n", i);
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf(sect_code_cstr);
        }
#endif
        write(out_fd, sect_code_cstr, strlen(sect_code_cstr));
    }

    /* render bytecode transcriptions */
    stack_size = 0;
    for (i = 0; i < bc_count; i++) {
        bytecode_t bc = tree->bytecodes.data[bc_start + i];
        if (bc.op == BOP_EXTEND_ARG) {
            reg_arg += (bc.arg << 8 | bc.arg) << 8;
        } else {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("// bytecode: ");
                bytecode_print(bc);
                printf("\n");
                printf("// stack_size: %d\n", stack_size);
            }
#endif
            const char* bc_code_cstr = transpile_bytecode(
                tree, bc, bc_start + i, stack_size - 1, reg_arg
            );
            stack_size += bytecode_stack_diff(bc.op);

#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(bc_code_cstr);
                printf("// --------\n");
            }
#endif
            write(out_fd, bc_code_cstr, strlen(bc_code_cstr));
            reg_arg = 0;
        }
    }

    /* render function end */
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(func_end);
    }
#endif
    write(out_fd, func_end, strlen(func_end));
}

void
transpile(syntax_tree_t* tree, int out_fd)
{
    const int bytecode_section_count = tree->bytecode_start_index.size - 1;
    int i;

    write(out_fd, src_transpile_utils_meta_c, src_transpile_utils_meta_c_len);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("====================\n");
    }
#endif

    // transpile bytecodes
    for (i = 0; i < bytecode_section_count; i++) {
        int start_index = tree->bytecode_start_index.data[i];
        int end_index = tree->bytecode_start_index.data[i + 1];
        transpile_bytecode_section(
            tree, out_fd, tree->entry_indexs.data[i], start_index, end_index
        );
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("====================\n");
    }
#endif
}
