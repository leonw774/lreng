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
    syntax_tree_t* tree, const int cur_obj_id, const int assignee_index
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
            "    object_t* var_%d_%d = frame_set(FRAME, %d, "
            "as_pair(var_%d_%d).left ); // unpack branch\n",
            cur_obj_id, tree->lefts[assignee_index], left_token.code,
            cur_obj_id, assignee_index
        );
        char* left_child_buffer = transpile_frame_set_unpack(
            tree, cur_obj_id, tree->lefts[assignee_index]
        );
        left_size += strlen(left_child_buffer) + 1;
        left_buffer = realloc(left_buffer, left_size);
        strcat(left_buffer, left_child_buffer);
        free(left_child_buffer);
    } else {
        snprintf(
            left_buffer, LITERAL_BUFFER_SIZE,
            "    frame_set(FRAME, %d, as_pair(var_%d_%d).left); // unpack "
            "leaf\n",
            left_token.code, cur_obj_id, assignee_index
        );
    }
    if (right_token.type != TOK_ID) {
        snprintf(
            right_buffer, LITERAL_BUFFER_SIZE,
            "    object_t* var_%d_%d = frame_set(FRAME, %d, "
            "as_pair(var_%d_%d).right); // unpack branch\n",
            cur_obj_id, tree->rights[assignee_index], right_token.code,
            cur_obj_id, assignee_index
        );
        char* right_child_buffer = transpile_frame_set_unpack(
            tree, cur_obj_id, tree->rights[assignee_index]
        );
        right_size += strlen(right_child_buffer) + 1;
        right_buffer = realloc(left_buffer, right_size);
        strcat(right_buffer, right_child_buffer);
        free(right_child_buffer);
    } else {
        snprintf(
            right_buffer, LITERAL_BUFFER_SIZE,
            "    frame_set(FRAME, %d, as_pair(var_%d_%d).right); // unpack "
            "leaf\n",
            right_token.code, cur_obj_id, assignee_index
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

void
pop_rl_obj_id(dynarr_int_t* obj_id_stack, int* left_id, int* right_id)
{
    *right_id = *dynarr_int_back(obj_id_stack);
    dynarr_int_pop(obj_id_stack);
    *left_id = *dynarr_int_back(obj_id_stack);
    dynarr_int_pop(obj_id_stack);
}

void
pop_l_obj_id(dynarr_int_t* obj_id_stack, int* left_id)
{
    *left_id = *dynarr_int_back(obj_id_stack);
    dynarr_int_pop(obj_id_stack);
}

char*
transpile_bytecode(
    syntax_tree_t* tree, bytecode_t bc, size_t bc_index,
    dynarr_int_t* obj_id_stack, int cur_obj_id, int reg_arg
)
{
    static char buffer[BYTECODE_BUFFER_SIZE];
    char* tmp_buffer;
    char* code_tmpl;
    int left_id, right_id;
    buffer[0] = '\0';

    switch (bc.op) {
    case BOP_NOP:
        break;
    case BOP_PUSH_LIT:
        reg_arg |= bc.arg;
        code_tmpl = "inst_%d:\n    object_t* var_%d = %s; // PUSH_LITERAL\n";
        tmp_buffer = transpile_literal(tree->literals[reg_arg]);
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, tmp_buffer);
        free(tmp_buffer);
        tmp_buffer = NULL;
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_FGET:
        reg_arg |= bc.arg;
        code_tmpl
            = "inst_%d:\n"
              "    object_t* var_%d = frame_get(FRAME, %d); // FRAME_GET\n"
              "    if (!var_%d) { "
              "puts(\"\'%s\' is used uninitialized\"); exit(EXIT_FAILURE); }\n";
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, code_tmpl, bc_index, cur_obj_id,
            reg_arg, cur_obj_id, tree->id_code_str_map[reg_arg]
        );
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_FSET:
        reg_arg |= bc.arg;
        code_tmpl = "inst_%d:\n"
                    "    object_t* var_%d = frame_set(FRAME, %d, var_%d); "
                    "// FRAME_SET\n";
        pop_l_obj_id(obj_id_stack, &left_id);
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, code_tmpl, bc_index, cur_obj_id,
            reg_arg, left_id
        );
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_FSET_UNPACK:
        reg_arg |= bc.arg;
        code_tmpl = "inst_%d:\n"
                    "    object_t* var_%d = var_%d; // FRAME_SET_UNPACK\n"
                    "    object_t* var_%d_%d = var_%d; // FRAME_SET_UNPACK\n%s";
        tmp_buffer = transpile_frame_set_unpack(tree, cur_obj_id, reg_arg);
        pop_l_obj_id(obj_id_stack, &left_id);
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, code_tmpl, bc_index, cur_obj_id,
            left_id, cur_obj_id, reg_arg, left_id, tmp_buffer
        );
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        free(tmp_buffer);
        tmp_buffer = NULL;
        break;
    case BOP_POP:
        pop_l_obj_id(obj_id_stack, &left_id);
        code_tmpl = "inst_%d:\n    (void)var_%d; // POP\n";
        snprintf(buffer, BYTECODE_BUFFER_SIZE, code_tmpl, bc_index, left_id);
        break;
    case BOP_RET:
        code_tmpl = "inst_%d:\n"
                    "    frame_pop(FRAME); // RET\n    return var_%d; // RET\n";
        pop_l_obj_id(obj_id_stack, &left_id);
        snprintf(buffer, BYTECODE_BUFFER_SIZE, code_tmpl, bc_index, left_id);
        break;
    // case BOP_JUMP:
    //     /* not implemented */
    //     print_runtime_error(bc.pos, "BOP_JUMP is not implemented");
    //     break;
    // case BOP_BF_OR_POP:
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
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n"
                    "    object_t* var_%d = exec_call(FRAME, var_%d, var_%d); "
                    "// CALL\n";
        snprintf(
            buffer, BYTECODE_BUFFER_SIZE, code_tmpl, bc_index, cur_obj_id,
            left_id, right_id
        );
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_NEG:
        pop_l_obj_id(obj_id_stack, &left_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "-as_number(var_%d)); // NEG\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
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
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) * as_number(var_%d)); // MUL\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_DIV:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) * as_number(var_%d)); // DIV\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_MOD:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number(floor_modulo("
                    "as_number(var_%d), as_number(var_%d))); // MOD\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_ADD:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) + as_number(var_%d)); // ADD\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_SUB:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) - as_number(var_%d)); // SUB\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_LT:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) < as_number(var_%d)); // LT\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_LE:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) <= as_number(var_%d)); // LT\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_GT:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) > as_number(var_%d)); // LT\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_GE:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "as_number(var_%d) >= as_number(var_%d)); // LT\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_EQ:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "object_equal(var_%d, var_%d)); // EQ\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_NE:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_number("
                    "1.f - object_equal(var_%d, var_%d)); // EQ\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_AND:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n"
                    "    object_t* var_%d = from_number(object_to_bool(var_%d) "
                    "&& object_to_bool(var_%d)); // AND\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_OR:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n"
                    "    object_t* var_%d = from_number(object_to_bool(var_%d) "
                    "|| object_to_bool(var_%d)); // OR\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
        break;
    case BOP_PAIR:
        pop_rl_obj_id(obj_id_stack, &left_id, &right_id);
        code_tmpl = "inst_%d:\n    object_t* var_%d = from_pair((pair_t) {"
                    " .left = var_%d, .right = var_%d }); // PAIR\n";
        sprintf(buffer, code_tmpl, bc_index, cur_obj_id, left_id, right_id);
        dynarr_int_append(obj_id_stack, &cur_obj_id);
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

    int cur_obj_id = 0;
    dynarr_int_t obj_id_stack = dynarr_int_new();
    size_t reg_arg = 0;

    int i;
    int bc_count = bc_end - bc_start;
    assert(bc_count > 0);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "// transpile_bytecode_section: %d ~ %d (%d)\n", bc_start, bc_end,
            bc_count
        );
    }
#endif

    /* render function */
    if (tree->root_index == entry_index) {
        sprintf(sect_code_cstr, "%s", top_start);
    } else {
        sprintf(sect_code_cstr, func_start, entry_index);
    }
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(sect_code_cstr);
    }
#endif
    write(out_fd, sect_code_cstr, strlen(sect_code_cstr));

    for (i = 0; i < bc_count; i++) {
        bytecode_t bc = tree->bytecodes.data[bc_start + i];
        if (bc.op == BOP_EXTEND_ARG) {
            reg_arg += (bc.arg << 8 | bc.arg) << 8;
        } else {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                int i;
                printf("// cur_obj_id: %d\n", cur_obj_id);
                printf("// bytecode: ");
                bytecode_print(bc);
                printf("\n");
                printf("// obj_id_stack: [");
                for (i = 0; i < obj_id_stack.size; i++) {
                    printf("%d ", obj_id_stack.data[i]);
                }
                printf("]\n");
            }
#endif
            const char* bc_code_cstr = transpile_bytecode(
                tree, bc, bc_start + i, &obj_id_stack, cur_obj_id, reg_arg
            );
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(bc_code_cstr);
                printf("// --------\n");
            }
#endif
            write(out_fd, bc_code_cstr, strlen(bc_code_cstr));
            cur_obj_id++;
            reg_arg = 0;
        }
    }

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
