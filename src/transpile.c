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
#define LIT_BUFFER_SIZE 255
#define BC_BUFFER_SIZE 4095
#define DEBUG_FILE_NAME "debug_transpile.c"

void
emit(int fd, const void* buf, unsigned int len)
{
    static FILE* debug_file = NULL;
    write(fd, buf, len);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        if (debug_file == NULL) {
            debug_file = fopen(DEBUG_FILE_NAME, "w");
        }
        fprintf(debug_file, "%s", (const char*)buf);
        fflush(debug_file);
    }
#endif
}

char*
transpile_frame_set_unpack(
    syntax_tree_t* tree, int stack_size, const int assignee_index
)
{
    char* left_buffer = malloc(LIT_BUFFER_SIZE + 1);
    char* right_buffer = malloc(LIT_BUFFER_SIZE + 1);
    size_t left_size = LIT_BUFFER_SIZE;
    size_t right_size = LIT_BUFFER_SIZE;
    const token_t left_token = tree->tokens.data[tree->lefts[assignee_index]];
    const token_t right_token = tree->tokens.data[tree->rights[assignee_index]];

    if (left_token.type != TOK_ID) {
        snprintf(
            left_buffer, LIT_BUFFER_SIZE,
            "    object_t* s%d_%d = frame_set("
            "FRAME, %d, as_pair(s%d_%d).left ); // unpack branch\n",
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
            left_buffer, LIT_BUFFER_SIZE,
            "    frame_set(FRAME, %d, as_pair(s%d_%d).left); // unpack leaf\n",
            left_token.code, stack_size, assignee_index
        );
    }
    if (right_token.type != TOK_ID) {
        snprintf(
            right_buffer, LIT_BUFFER_SIZE,
            "    object_t* s%d_%d = frame_set("
            "FRAME, %d, as_pair(s%d_%d).right); // unpack branch\n",
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
            right_buffer, LIT_BUFFER_SIZE,
            "    frame_set(FRAME, %d, as_pair(s%d_%d).right); // unpack leaf\n",
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
    char* buffer = malloc(LIT_BUFFER_SIZE + 1);
    char* tmp_cstr;
    dynarr_char_t tmp_str;

    if (literal_object->type == TYPE_NUM) {
        tmp_str = number_to_dec_string(
            &literal_object->as.number, NUMBER_PRECISION
        );
        tmp_cstr = dynarr_char_to_str(&tmp_str);
        assert(tmp_cstr);
        snprintf(buffer, LIT_BUFFER_SIZE, "(from_number(%s))", tmp_cstr);
        free(tmp_cstr);
        dynarr_char_free(&tmp_str);
    } else if (literal_object->type == TYPE_NULL) {
        snprintf(buffer, LIT_BUFFER_SIZE, "(object_t*)(NULL_OBJPTR)");
    } else if (literal_object->type == TYPE_CALL) {
        snprintf(
            buffer, LIT_BUFFER_SIZE, "make_funct(NULL, %s)",
            RESERVED_IDS[literal_object->as.callable.builtin_name]
        );
    } else {
        print_runtime_error((linecol_t) { 0, 0 }, "un-compilable literal");
    }
    return buffer;
}

char*
transpile_bytecode(
    syntax_tree_t* tree, bytecode_t bc, size_t bci, int stack_size, int reg_arg
)
{
    static char buffer[BC_BUFFER_SIZE];
    char* tmp_buffer;
    char* t; /* template */
    int ss = stack_size, ssm1 = stack_size - 1, ssm2 = stack_size - 2;
    buffer[0] = '\0';

    switch (bc.op) {
    case BOP_NOP:
        break;
    case BOP_PUSH_LIT:
        reg_arg |= bc.arg;
        t = "inst%d: // PUSH_LIT\n    s%d = %s;\n";
        tmp_buffer = transpile_literal(tree->literals[reg_arg]);
        sprintf(buffer, t, bci, ss, tmp_buffer);
        free(tmp_buffer);
        tmp_buffer = NULL;
        break;
    case BOP_FGET:
        reg_arg |= bc.arg;
        t = "inst%d: // FGET\n"
            "    s%d = object_ref(frame_get(FRAME, %d));\n"
            "    check_uninit(s%d, \"%s\");\n";
        const char* code_str = tree->id_code_str_map[reg_arg];
        sprintf(buffer, t, bci, ss, reg_arg, ss, code_str);
        break;
    case BOP_FSET:
        reg_arg |= bc.arg;
        t = "inst%d: // FSET\n    frame_set(FRAME, %d, s%d);\n";
        sprintf(buffer, t, bci, reg_arg, ssm1);
        break;
    case BOP_FSET_UNPACK:
        reg_arg |= bc.arg;
        t = "inst%d: // FSET_UNPACK\n    object_t* s%d_%d = s%d;\n%s";
        tmp_buffer = transpile_frame_set_unpack(tree, ssm1, reg_arg);
        snprintf(
            buffer, BC_BUFFER_SIZE, t, bci, ssm1, reg_arg, ssm1, tmp_buffer
        );
        free(tmp_buffer);
        tmp_buffer = NULL;
        break;
    case BOP_POP:
        t = "inst%d: // POP\n    s%d = object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm1, ssm1);
        break;
    case BOP_RET:
        t = "inst%d: // RET\n    frame_pop(FRAME);\n    return s%d;\n";
        sprintf(buffer, t, bci, ssm1);
        break;
    case BOP_BF_OR_POP:
        reg_arg |= bc.arg;
        t = "inst%d: // BF_OR_POP\n"
            "    if (!object_to_bool(s%d))\n        goto inst%d;\n"
            "    else\n        s%d = object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm1, reg_arg, ssm1, ssm1);
        break;
    case BOP_BT_OR_POP:
        reg_arg |= bc.arg;
        t = "inst%d: // BT_OR_POP\n"
            "    if (object_to_bool(s%d))\n        goto inst%d;\n"
            "    else\n        s%d = object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm1, reg_arg, ssm1, ssm1);
        break;
    case BOP_MAKE_FUNCT:
        reg_arg |= bc.arg;
        t = "inst%d: // MAKE_FUNCT\n"
            "    s%d = make_funct(frame_copy(FRAME), fun%d);\n";
        sprintf(buffer, t, bci, ss, reg_arg);
        break;
    case BOP_CALL:
        t = "inst%d: // CALL\n"
            "    exec_call(FRAME, &tmp, &s%d, &s%d);\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_NEG:
        t = "inst%d: // NEG\n    s%d->as.number = -as_number(s%d);\n";
        sprintf(buffer, t, bci, ssm1, ssm1);
        break;
    case BOP_NOT:
        t = "inst%d:// NOT\n"
            "    tmp = !object_to_bool(s%d);\n"
            "    object_deref(s%d)\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm1, ssm1, ssm1);
        break;
    case BOP_CEIL:
        t = "inst%d: // CEIL\n    s%d->as.number = ceil(as_number(s%d));\n";
        sprintf(buffer, t, bci, ssm1, ssm1);
        break;
    case BOP_FLOOR:
        t = "inst%d: // FLOOR\n    s%d->as.number = floor(as_number(s%d));\n";
        sprintf(buffer, t, bci, ssm1, ssm1);
        break;
    case BOP_PGETL:
        t = "inst%d: // PGETL\n"
            "    tmp = object_ref(as_pair(s%d).left)\n;"
            "    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm1, ssm1, ssm1);
        break;
    case BOP_PGETR:
        t = "inst%d: // PGETL\n"
            "    tmp = object_ref(as_pair(s%d).right)\n;"
            "    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm1, ssm1, ssm1);
        break;
    case BOP_COND_CALL:
        t = "inst%d: // COND_CALL\n"
            "    if (%s->type == TYPE_CALL) {\n"
            "        exec_call(FRAME, &tmp, &s%d, &NULL_OBJPTR);\n"
            "        object_deref(s%d);\n        s%d = tmp;\n"
            "    }";
        sprintf(buffer, t, bci, ssm1, ssm1, ssm1, ssm1);
        break;
    case BOP_SWAP:
        t = "inst%d: // SWAP\n"
            "    tmp = s%d->as.right;\n"
            "    s%d->as.right = s%d->as.left;\n"
            "    s%d->as.left = tmp;\n";
        sprintf(buffer, t, bci, ssm1, ssm1, ssm1, ssm1);
        break;
    case BOP_EXP:
        t = "inst%d: // EXP\n"
            "{\n"
            "    double exp = as_number(s%d);\n"
            "    if (trunc(exp) != exp) {\n"
            "        fprintf(stderr, ERRMSG_OP_EXP);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    s%d->as.number = pow(as_number(s%d), exp);\n"
            "    object_deref(s%d);\n"
            "}\n";
        sprintf(buffer, t, bci, ssm1, ssm2, ssm2, ssm1);
        break;
    case BOP_MUL:
        t = "inst%d: // MUL\n"
            "    s%d->as.number = as_number(s%d) * as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_DIV:
        t = "inst%d: // DIV\n"
            "    s%d->as.number = as_number(s%d) / as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_MOD:
        t = "inst%d: // MOD\n"
            "    s%d->as.number = floor_modulo("
            "as_number(s%d), as_number(s%d));\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_ADD:
        t = "inst%d: // ADD\n"
            "    s%d->as.number = as_number(s%d) + as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_SUB:
        t = "inst%d: // SUB\n"
            "    s%d->as.number = as_number(s%d) - as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_LT:
        t = "inst%d: // LT\n"
            "    s%d->as.number = as_number(s%d) < as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_LE:
        t = "inst%d: // LT\n"
            "    s%d->as.number = as_number(s%d) <= as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_GT:
        t = "inst%d: // LT\n"
            "    s%d->as.number = as_number(s%d) > as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_GE:
        t = "inst%d: // LT\n"
            "    s%d->as.number = as_number(s%d) >= as_number(s%d);\n"
            "    object_deref(s%d);\n";
        sprintf(buffer, t, bci, ssm2, ssm2, ssm1, ssm1);
        break;
    case BOP_EQ:
        t = "inst%d: // EQ\n"
            "    tmp = from_number(object_equal(s%d, s%d));\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_NE:
        t = "inst%d: // NE\n"
            "    tmp = from_number(!object_equal(s%d, s%d));\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_AND:
        t = "inst%d: // AND\n"
            "    tmp = from_number("
            "object_to_bool(s%d) && object_to_bool(s%d));\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_OR:
        t = "inst%d: // OR\n"
            "    tmp = from_number("
            "object_to_bool(s%d) || object_to_bool(s%d));\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_PAIR:
        t = "inst%d: // PAIR\n"
            "    tmp = from_pair((pair_t) {\n"
            "        .left = object_ref(s%d), .right = object_ref(s%d)\n"
            "    });\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_BIND_ARG:
        reg_arg |= bc.arg;
        t = "inst%d: // BOP_BIND_ARG\n"
            "    if (as_callable(s%d).arg_code != -1) {\n"
            "        fprintf(stderr, ERRMSG_OP_BIND_ARG);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    s%d->as.callable.arg_code = %d;\n";
        snprintf(buffer, BC_BUFFER_SIZE, t, bci, ssm1, ssm1, reg_arg);
        break;
    case BOP_COND_PGET:
        t = "inst%d: // COND_PGET\n"
            "    tmp = "
            "object_to_bool(s%d) ? as_pair(s%s).left : as_pair(s%s).right;\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm1, ssm2, ssm1, ssm2);
        break;
    case BOP_COND_PCALL:
        t = "inst%d: // COND_PCALL\n"
            "    tmp = "
            "object_to_bool(s%d) ? as_pair(s%s).left : as_pair(s%s).right;\n"
            "    if (tmp->type == TYPE_CALL) {\n"
            "        exec_call(FRAME, tmp, tmp, &NULL_OBJPTR);\n"
            "    }\n"
            "    object_deref(s%d);\n    object_deref(s%d);\n    s%d = tmp;\n";
        sprintf(buffer, t, bci, ssm2, ssm1, ssm1, ssm2, ssm1, ssm2);
        break;
    default:
        sprintf(
            ERR_MSG_BUF, "transpile_bytecode: bad bytecode: %d,%d\n", bc.op,
            bc.arg
        );
        print_runtime_error(bc.pos, ERR_MSG_BUF);
        return NULL;
    }
    return buffer;
}

void
transpile_bytecode_section(
    syntax_tree_t* tree, int out_fd, int entry_index, int bc_start, int bc_end
)
{
    char sect_code_cstr[128];
    static const char* top_start = "object_t*\n"
                                   "top(frame_t* FRAME, object_t* arg)\n{\n";
    static const char* fun_start = "object_t*\n"
                                   "fun%d(frame_t* FRAME, object_t* arg)\n{\n";
    static const char* fun_end = "}\n\n";

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
        snprintf(sect_code_cstr, 128, fun_start, entry_index);
    }
    emit(out_fd, sect_code_cstr, strlen(sect_code_cstr));
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(sect_code_cstr);
    }
#endif

    /* render stack slots */
    for (i = 0; i < stack_size_max; i++) {
        snprintf(sect_code_cstr, 128, "    object_t* s%d;\n", i);
        emit(out_fd, sect_code_cstr, strlen(sect_code_cstr));
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf(sect_code_cstr);
        }
#endif
    }

    /* render temp object slots */
    snprintf(sect_code_cstr, 128, "%s", "    object_t* tmp;\n");
    emit(out_fd, sect_code_cstr, strlen(sect_code_cstr));
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(sect_code_cstr);
    }
#endif

    /* render bytecode transcriptions */
    stack_size = 0;
    for (i = 0; i < bc_count; i++) {
        bytecode_t bc = tree->bytecodes.data[bc_start + i];
        if (bc.op == BOP_EXTEND_ARG) {
            reg_arg += (reg_arg << 8 | bc.arg) << 8;
        } else {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("// bytecode: ");
                bytecode_print(bc);
                printf(" | stack_size: %d\n", stack_size);
                fflush(stdout);
            }
#endif
            const char* bc_code_cstr = transpile_bytecode(
                tree, bc, bc_start + i, stack_size, reg_arg
            );
            if (!bc_code_cstr) {
                printf("// transpile ERROR\n");
                fflush(stdout);
                break;
            }
            stack_size += bytecode_stack_diff(bc.op);
            emit(out_fd, bc_code_cstr, strlen(bc_code_cstr));
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(bc_code_cstr);
                fflush(stdout);
            }
#endif
            reg_arg = 0;
        }
    }

    /* render function end */
    emit(out_fd, fun_end, strlen(fun_end));
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(fun_end);
    }
#endif
}

void
transpile(syntax_tree_t* tree, int out_fd)
{
    const int bytecode_section_count = tree->bytecode_start_index.size - 1;
    int i;

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("====================\n");
    }
    /* to link the debug info to debug file */
    emit(out_fd, "#line 2 \"", 9);
    emit(out_fd, DEBUG_FILE_NAME, strlen(DEBUG_FILE_NAME));
    emit(out_fd, "\"\n", 2);
#endif

    /* prevent leak */
    src_transpile_utils_meta_c[src_transpile_utils_meta_c_len] = '\0';
    emit(out_fd, src_transpile_utils_meta_c, src_transpile_utils_meta_c_len);

    /* transpile bytecodes */
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
