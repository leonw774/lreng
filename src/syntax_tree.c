#include "syntax_tree.h"
#include "frame.h"
#include "optimize.h"
#include "reserved.h"
#include "semantic.h"
#include "syntax_tree_iter.h"
#include "token.h"
#include "tree_parser.h"
#include "utils/arena.h"
#include "utils/debug_flag.h"
#include "utils/errormsg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int
int_cmp(const void* a, const void* b)
{
    int arg1 = *(const int*)a;
    int arg2 = *(const int*)b;
    if (arg1 < arg2) {
        return -1;
    }
    if (arg1 > arg2) {
        return 1;
    }
    return 0;
}

/* create tree from token list and prepare for fast lookup and pre-eval */
syntax_tree_t
syntax_tree_create(dynarr_token_t tokens)
{
    /* parse */

    /* old shunting yard */
    /* dynarr_token_t postfix_tokens = shunting_yard(tokens); */

    /* pratt parsing */
    pratt_parser_context_t pp_context = {
        .tokens = tokens,
        .output = dynarr_token_new(),
        .pos = 0,
    };
    parse_expr(&pp_context, 0);
    dynarr_token_t postfix_tokens = pp_context.output;

    int i = 0, token_size = postfix_tokens.size;
    syntax_tree_t tree = {
        .tokens = postfix_tokens,
        .bytecodes = dynarr_bytecode_new(),
        .bytecode_start_index = dynarr_int_new(),
        .entry_indexs = dynarr_int_new(),
        .root_index = -1,
        .lefts = NULL,
        .rights = NULL,
        .max_id_code = -1,
    };
    dynarr_int_t index_stack = dynarr_int_new();
    int* tree_data = malloc(token_size * sizeof(int) * 3);
    assert(tree_data != NULL);

    /* get max_id_code */
    for (i = postfix_tokens.size - 1; i >= 0; i--) {
        token_t* cur_token = dynarr_token_at(&postfix_tokens, i);
        if (cur_token->type == TOK_ID && tree.max_id_code < cur_token->code) {
            tree.max_id_code = cur_token->code;
        }
    }
    tree.id_code_str_map = calloc(tree.max_id_code + 1, sizeof(char*));
    for (i = postfix_tokens.size - 1; i >= 0; i--) {
        token_t* cur_token = dynarr_token_at(&postfix_tokens, i);
        if (cur_token->type == TOK_ID
            && tree.id_code_str_map[cur_token->code] == NULL) {
            tree.id_code_str_map[cur_token->code] = cur_token->str;
        }
    }

    /* root, lefts, rights and sizes */

    tree.lefts = tree_data;
    tree.rights = tree.lefts + token_size;
    memset(tree.lefts, -1, token_size * sizeof(int));
    memset(tree.rights, -1, token_size * sizeof(int));
    for (i = 0; i < token_size; i++) {
        token_t* cur_token = dynarr_token_at(&postfix_tokens, i);
#ifdef ENABLE_DEBUG_LOG_MORE
        token_print(cur_token);
#endif
        if (cur_token->type == TOK_OP) {
            int l_index, r_index = -1;
            if (!is_unary_op(cur_token->code)) {
                r_index = *dynarr_int_back(&index_stack);
                dynarr_int_pop(&index_stack);
                tree.rights[i] = r_index;
            }
            if (index_stack.size == 0) {
                throw_syntax_error(
                    cur_token->pos, "Operator has too few operands"
                );
            }
            l_index = *dynarr_int_back(&index_stack);
            dynarr_int_pop(&index_stack);
            tree.lefts[i] = l_index;
            dynarr_int_append(&index_stack, &i);
#ifdef ENABLE_DEBUG_LOG_MORE
            printf(" L=");
            token_print(dynarr_token_at(&postfix_tokens, l_index));
            printf(" R=");
            if (r_index != -1) {
                token_print(dynarr_token_at(&postfix_tokens, r_index));
            }
#endif
        } else {
            dynarr_int_append(&index_stack, &i);
        }
#ifdef ENABLE_DEBUG_LOG_MORE
        putchar('\n');
#endif
    }
    tree.root_index = *dynarr_int_at(&index_stack, 0);

    /* check parse result */

    if (index_stack.size != 1) {
#ifdef ENABLE_DEBUG_LOG
        int n;
        printf("REMAINED IN STACK:\n");
        for (n = 0; n < index_stack.size; n++) {
            int index = *dynarr_int_at(&index_stack, n);
            token_print(dynarr_token_at(&postfix_tokens, index));
            puts("");
        }
#endif
        sprintf(
            ERR_MSG_BUF,
            "Bad syntax somewhere: %d tokens left in stack when parsing tree",
            index_stack.size
        );
        throw_syntax_error((linecol_t) { 0, 0 }, ERR_MSG_BUF);
    }
    dynarr_int_free(&index_stack);

    /* check semantic */

    if (!syntax_tree_check_semantic(&tree)) {
        exit(SEMANTIC_ERR_CODE);
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("check semantic done\n");
    }
#endif

    /* optimization */

    syntax_tree_optimatize(&tree);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("optimatization done\n");
    }
#endif

    /* eval literal */

    tree.literals = calloc(token_size, sizeof(object_t*));
    assert(tree.literals != NULL);
    for (i = 0; i < token_size; i++) {
        token_t* cur_token = dynarr_token_at(&postfix_tokens, i);
        if (cur_token->type == TOK_ID && cur_token->code < RESERVED_ID_COUNT) {
            tree.literals[i]
                = object_ref((object_t*)&RESERVED_OBJS[cur_token->code]);
        } else if (cur_token->type == TOK_NUM) {
            tree.literals[i] = object_create(
                TYPE_NUM,
                (object_data_union) {
                    .number = number_from_str(cur_token->str),
                }
            );
        } else if (cur_token->type == TOK_OP && cur_token->code == OP_PAIR) {
            object_t* left = tree.literals[tree.lefts[i]];
            object_t* right = tree.literals[tree.rights[i]];
            /* if left and right are all literal, pair can become literal too */
            if (left && right) {
                tree.literals[i] = object_create(
                    TYPE_PAIR,
                    (object_data_union) { .pair = (pair_t) {
                                              .left = object_ref(left),
                                              .right = object_ref(right),
                                          } }
                );
            }
        }
#ifdef ENABLE_DEBUG_LOG_MORE
        if (tree.literals[i] != NULL) {
            printf("created literal at node %d: ", i);
            object_print(tree.literals[i], ' ');
            printf("addr: %p\n", tree.literals[i]);
        }
#endif
    }

    /* compile bytecodes for root and all functions and macros */

    for (i = 0; i < postfix_tokens.size; i++) {
        token_t* cur_token = dynarr_token_at(&postfix_tokens, i);
        if (cur_token->type == TOK_OP
            && (cur_token->code == OP_MAKE_FUNCT
                || cur_token->code == OP_MAKE_MACRO)) {
            dynarr_int_append(&tree.entry_indexs, &tree.lefts[i]);
        }
    }
    dynarr_int_append(&tree.entry_indexs, &tree.root_index);
    /* sort the array (should be already sorted but just to be sure) */
    qsort(tree.entry_indexs.data, tree.entry_indexs.size, sizeof(int), int_cmp);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("find %d callables\n", tree.entry_indexs.size - 1);
    }
#endif

    for (i = 0; i < tree.entry_indexs.size; i++) {
        int entry_index_i = tree.entry_indexs.data[i];
        dynarr_bytecode_t bc_i = syntax_tree_compile(&tree, entry_index_i);
        dynarr_int_append(&tree.bytecode_start_index, &tree.bytecodes.size);
        bytecode_array_extend(
            &bc_i, BOP_RET, 0, tree.tokens.data[entry_index_i].pos
        );
        bytecode_array_extend(
            &bc_i, BOP_NOP, 0, (linecol_t) { .line = -1, .col = -1 }
        );
        dynarr_bytecode_concat(&tree.bytecodes, &bc_i);
        dynarr_bytecode_free(&bc_i);
    }
    dynarr_int_append(&tree.bytecode_start_index, &tree.bytecodes.size);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("final_tree=\n");
        syntax_tree_print(&tree);
    }
#endif

    return tree;
}

int
syntax_tree_check_semantic(const syntax_tree_t* tree)
{
    frame_t* cur_frame = frame_new(NULL);
    tree_preorder_iterator_t tree_iter = syntax_tree_iter_init(tree, -1);
    token_t* cur_token = syntax_tree_iter_get(&tree_iter);
    dynarr_int_t func_depth_stack = dynarr_int_new();
    int is_passed = 1, cur_depth = -1, cur_index = -1, cur_func_depth = -1;

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("check semantic\n");
    }
#endif

    dynarr_int_append(&func_depth_stack, &cur_depth);
    while (cur_token != NULL && is_passed) {
        cur_index = *dynarr_int_back(&tree_iter.index_stack);
        cur_depth = *dynarr_int_back(&tree_iter.depth_stack);
        cur_func_depth = *dynarr_int_back(&func_depth_stack);
        if (cur_token->type == TOK_OP) {
            if (cur_depth <= cur_func_depth) {
                /* if we left a function */
                dynarr_int_pop(&func_depth_stack);
                frame_pop_stack(cur_frame);
            }
            if (cur_token->code == OP_ASSIGN) {
                /* check assign rule */
                is_passed = check_assign_rule(tree, cur_frame, cur_index);
            } else if (cur_token->code == OP_BIND_ARG) {
                /* check bind argument rule */
                is_passed = check_bind_arg_rule(tree, cur_frame, cur_index);
            } else if (cur_token->code == OP_MAKE_FUNCT) {
                /* walk into a function */
                frame_push_stack(cur_frame, cur_index);
                dynarr_int_append(&func_depth_stack, &cur_depth);
            }
        }
        // } else if (cur_token->type == TOK_ID) {
        //     is_checking = 1;
        //     is_passed = check_id_init_rule(tree, cur_frame, cur_index);
        // }
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            if (cur_token->code == OP_ASSIGN
                || cur_token->code == OP_BIND_ARG) {
                printf("- %s\n", is_passed ? "passed" : "not pass");
                fflush(stdout);
            }
        }
#endif
        syntax_tree_iter_next(&tree_iter);
        cur_token = syntax_tree_iter_get(&tree_iter);
    }

    syntax_tree_iter_free(&tree_iter);
    dynarr_int_free(&func_depth_stack);
    frame_free(cur_frame);
    return is_passed;
}

void
syntax_tree_optimatize(syntax_tree_t* tree)
{
    optimize_remove_op_pos(tree);
    optimize_remove_no_side_effect_expr(tree);
}

dynarr_bytecode_t
syntax_tree_compile(const syntax_tree_t* tree, const int root_index)
{
    dynarr_bytecode_t output = dynarr_bytecode_new();
    dynarr_bytecode_t left_code;
    dynarr_bytecode_t right_code;
    token_t cur_token = tree->tokens.data[root_index];
    linecol_t cur_pos = cur_token.pos;
    bytecode_op_code_enum bop_code;
    int left_index, right_index;

#ifdef ENABLE_DEBUG_LOG_MORE
    if (global_is_enable_debug_log) {
        printf("Compile at token index %d: ", root_index);
        token_print(&cur_token);
        printf("\n");
        fflush(stdout);
    }
#endif

    if (tree->literals[root_index] != NULL) {
        bytecode_array_extend(&output, BOP_PUSH_LITERAL, root_index, cur_pos);
        return output;
    } else if (cur_token.type == TOK_ID) {
        bytecode_array_extend(&output, BOP_FRAME_GET, cur_token.code, cur_pos);
        return output;
    }

    bop_code = op_to_bop_code(cur_token.code);
    left_index = tree->lefts[root_index];
    right_index = tree->rights[root_index];

    switch (cur_token.code) {
    case OP_MAKE_FUNCT:
    case OP_MAKE_MACRO:
        bytecode_array_extend(&output, bop_code, left_index, cur_pos);
        break;
    case OP_ASSIGN:
        right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&right_code);
        if (tree->tokens.data[left_index].type == TOK_OP) {
            /* is pair unpacking */
            bytecode_array_extend(
                &output, BOP_FRAME_SET_FROM_PAIR, left_index, cur_pos
            );
        } else {
            bytecode_array_extend(
                &output, BOP_FRAME_SET, tree->tokens.data[left_index].code,
                cur_pos
            );
        }
        break;
    case OP_BIND_ARG:
        right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&right_code);
        bytecode_array_extend(&output, bop_code, left_index, cur_pos);
        break;
    case OP_COND_AND:
        left_code = syntax_tree_compile(tree, left_index);
        right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &left_code);
        bytecode_array_extend(
            &output, BOP_JUMP_FALSE_OR_POP, right_code.size, cur_pos
        );
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&left_code);
        dynarr_bytecode_free(&right_code);
        break;
    case OP_COND_OR:
        left_code = syntax_tree_compile(tree, left_index);
        right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &left_code);
        bytecode_array_extend(
            &output, BOP_JUMP_TRUE_OR_POP, right_code.size, cur_pos
        );
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&left_code);
        dynarr_bytecode_free(&right_code);
        break;

    case OP_EXPRSEP:
        left_code = syntax_tree_compile(tree, left_index);
        right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &left_code);
        bytecode_array_extend(&output, BOP_POP, 0, cur_pos);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&left_code);
        dynarr_bytecode_free(&right_code);
        break;
    default:
        left_code = syntax_tree_compile(tree, left_index);
        if (right_index != -1) {
            right_code = syntax_tree_compile(tree, right_index);
            dynarr_bytecode_concat(&output, &left_code);
            dynarr_bytecode_concat(&output, &right_code);
            dynarr_bytecode_free(&right_code);
        } else {
            dynarr_bytecode_concat(&output, &left_code);
        }
        dynarr_bytecode_free(&left_code);
        bytecode_array_extend(&output, bop_code, 0, cur_pos);
        break;
    };
    return output;
}

inline int
syntax_tree_get_bytecode_start_index(
    const syntax_tree_t* tree, const int node_index
)
{
    const int* found_entry_index_ptr = bsearch(
        &node_index, tree->entry_indexs.data, tree->entry_indexs.size,
        sizeof(int), int_cmp
    );
    const int found_bytecode_start_index
        = found_entry_index_ptr - tree->entry_indexs.data;
    assert(found_entry_index_ptr != NULL);
    assert(found_bytecode_start_index < tree->entry_indexs.size);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "syntax_tree_get_bytecode_start_index: node_index=%d, "
            "found_bytecode_start_index=%d\n",
            node_index, found_bytecode_start_index
        );
    }
#endif
    return tree->bytecode_start_index.data[found_bytecode_start_index];
}

inline void
syntax_tree_free(syntax_tree_t* tree)
{
    int i;
    /* iterate from back because literal could be pair and we want to
     * free parents first */
    for (i = tree->tokens.size - 1; i >= 0; i--) {
        token_t* token = dynarr_token_at(&tree->tokens, i);
        if (token->type == TOK_ID && token->code < RESERVED_ID_COUNT) {
            continue;
        }
        if (tree->literals[i] != NULL) {
#ifdef ENABLE_DEBUG_LOG_MORE
            printf(
                "freeing literal at node %d, addr: %p\n", i, tree->literals[i]
            );
            object_print(tree->literals[i], '\n');
#endif
            object_deref(tree->literals[i]);
        }
    }
    free(tree->literals);

    tree->root_index = -1;
    dynarr_token_free(&tree->tokens);
    /* lefts, rights, and sizes share same heap chunk and left is the
     * head so only need to free left */
    free(tree->lefts);
    free(tree->id_code_str_map);

    /* bytecodes */
    dynarr_bytecode_free(&tree->bytecodes);
    dynarr_int_free(&tree->entry_indexs);
    dynarr_int_free(&tree->bytecode_start_index);
}

void
syntax_tree_print(const syntax_tree_t* tree)
{
    tree_preorder_iterator_t iter = syntax_tree_iter_init(tree, -1);
    int i;
    printf("----- TREE -----\n");
    while (iter.index_stack.size != 0) {
        /* get */
        int cur_index = *dynarr_int_back(&iter.index_stack),
            next_depth = *dynarr_int_back(&iter.depth_stack) + 1;
        dynarr_int_pop(&iter.index_stack);
        dynarr_int_pop(&iter.depth_stack);

        if (cur_index != -1) {
            /* print */
            printf("%*c", next_depth - 1, ' ');
            token_print(dynarr_token_at(&tree->tokens, cur_index));
            printf(" (%d)\n", cur_index);
            fflush(stdout);

            /* update */
            if (tree->rights[cur_index] != -1) {
                dynarr_int_append(&iter.index_stack, &tree->rights[cur_index]);
                dynarr_int_append(&iter.depth_stack, &next_depth);
            }
            if (tree->lefts[cur_index] != -1) {
                dynarr_int_append(&iter.index_stack, &tree->lefts[cur_index]);
                dynarr_int_append(&iter.depth_stack, &next_depth);
            }
        }
    }
    syntax_tree_iter_free(&iter);

    printf("----- LITERALS -----\n");
    for (i = 0; i < tree->tokens.size; i++) {
        if (tree->literals[i]) {
            printf("%u: ", i);
            token_print(&tree->tokens.data[i]);
            printf("\n");
        }
    }

    printf("----- BYTECODES -----\n");
    for (i = 0; i < tree->entry_indexs.size; i++) {
        int j = 0;
        int bc_start = tree->bytecode_start_index.data[i];
        int bc_end = tree->bytecode_start_index.data[i + 1];
        printf("Bytecode of node %d\n", tree->entry_indexs.data[i]);
        for (j = bc_start; j < bc_end; j++) {
            bytecode_t bc = tree->bytecodes.data[j];
            printf("%4u: ", j);
            bytecode_print(bc);
            if (bc.op == BOP_FRAME_GET || bc.op == BOP_FRAME_SET) {
                printf(" (\"%s\")", tree->id_code_str_map[bc.arg]);
            } else if (
                bc.op == BOP_JUMP_FALSE_OR_POP || bc.op == BOP_JUMP_TRUE_OR_POP
            ) {
                printf(" (to %u)", j + bc.arg + 1);
            } else if (
                bc.op == BOP_MAKE_FUNCT || bc.op == BOP_MAKE_MACRO
                || bc.op == BOP_BIND_ARG || bc.op == BOP_FRAME_SET_FROM_PAIR
                || bc.op == BOP_PUSH_LITERAL
            ) {
                printf(" ((node %u) ", bc.arg);
                token_print(&tree->tokens.data[bc.arg]);
                printf(")");
            }
            printf("\n");
        }
        printf("\n");
    }
}
