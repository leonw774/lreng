#include "syntax_tree.h"
#include "frame.h"
#include "reserved.h"
#include "semantic.h"
#include "token.h"
#include "tree_parser.h"
#include "utils/arena.h"
#include "utils/errormsg.h"
#include "utils/debug_flag.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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
        .bytecodes = NULL,
        .bytecode_indexs = dynarr_int_new(),
        .lefts = NULL,
        .rights = NULL,
        .sizes = NULL,
        .root_index = -1,
        .max_id_name = -1,
    };
    dynarr_int_t index_stack = dynarr_int_new();
    int* tree_data = malloc(token_size * sizeof(int) * 3);
    assert(tree_data != NULL);

    /* find max_id_name */
    for (i = postfix_tokens.size - 1; i >= 0; i--) {
        token_t* t = dynarr_token_at(&postfix_tokens, i);
        if (t->type == TOK_ID && tree.max_id_name < t->code) {
            tree.max_id_name = t->code;
        }
    }

    /* root, lefts, rights and sizes */
    tree.lefts = tree_data;
    tree.rights = tree.lefts + token_size;
    tree.sizes = tree.rights + token_size;
    memset(tree.lefts, -1, token_size * sizeof(int));
    memset(tree.rights, -1, token_size * sizeof(int));
    memset(tree.sizes, 0, token_size * sizeof(int));
    for (i = 0; i < token_size; i++) {
        token_t* cur_token = dynarr_token_at(&tree.tokens, i);
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
            token_print(dynarr_token_at(&tree.tokens, l_index));
            printf(" R=");
            if (r_index != -1) {
                token_print(dynarr_token_at(&tree.tokens, r_index));
            }
#endif
        } else {
            dynarr_int_append(&index_stack, &i);
        }

        tree.sizes[i]
            = (1 + ((tree.lefts[i] == -1) ? 0 : tree.sizes[tree.lefts[i]])
               + ((tree.rights[i] == -1) ? 0 : tree.sizes[tree.rights[i]]));
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
            token_print(dynarr_token_at(&tree.tokens, index));
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

    /* eval literal */

    tree.literals = calloc(token_size, sizeof(object_t*));
    assert(tree.literals != NULL);
    for (i = 0; i < token_size; i++) {
        token_t* cur_token = dynarr_token_at(&tree.tokens, i);
        if (cur_token->type == TOK_NUM) {
            tree.literals[i] = object_create(
                TYPE_NUM,
                (object_data_union) {
                    .number = number_from_str(cur_token->str),
                }
            );
        } else if (cur_token->type == TOK_OP && cur_token->code == OP_PAIR) {
            if (tree.lefts[i] == -1 || tree.rights[i] == -1) {
                throw_syntax_error(
                    cur_token->pos, "Pair operator has too few operands"
                );
            } else if (
                tree.literals[tree.lefts[i]] != NULL
                && tree.literals[tree.rights[i]] != NULL
            ) {
                tree.literals[i] = object_create(
                    TYPE_PAIR,
                    (object_data_union) {
                        .pair = (pair_t) {
                            .left = object_ref(tree.literals[tree.lefts[i]]),
                            .right = object_ref(tree.literals[tree.rights[i]]),
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

    /* check semantic */

    if (!syntax_tree_check_semantic(&tree)) {
        exit(SEMANTIC_ERR_CODE);
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("check semantic done\n");
    }
#endif

    /* compile bytecodes for top and all functions and macros */

    dynarr_int_append(&tree.bytecode_indexs, &tree.root_index);
    for (i = 0; i < tree.tokens.size; i++) {
        token_t* t = dynarr_token_at(&tree.tokens, i);
        if (t->type == TOK_OP
            && (t->code == OP_MAKE_FUNCT || t->code == OP_MAKE_MACRO)) {
            dynarr_int_append(&tree.bytecode_indexs, &tree.lefts[i]);
        }
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("find %d callables\n", tree.bytecode_indexs.size);
    }
#endif

    tree.bytecodes
        = malloc(tree.bytecode_indexs.size * sizeof(dynarr_bytecode_t));
    for (i = 0; i < tree.bytecode_indexs.size; i++) {
        tree.bytecodes[i] = syntax_tree_compile(
            &tree, *dynarr_int_at(&tree.bytecode_indexs, i)
        );
        bytecode_array_extend(&tree.bytecodes[i], BOP_RET, 0);
    }

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
    int i, is_passed = 1;
    uint8_t* id_usage = calloc(tree->max_id_name + 1, sizeof(uint8_t));
    assert(id_usage != NULL);
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        id_usage[i] = (uint8_t)1;
    }

    frame_t* cur_frame = frame_new(NULL);

    tree_preorder_iterator_t tree_iter = syntax_tree_iter_init(tree, -1);
    token_t* cur_token = syntax_tree_iter_get(&tree_iter);
    int cur_depth = -1, cur_index = -1, cur_func_depth = -1;
    dynarr_int_t func_depth_stack = dynarr_int_new();

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("check semantic\n");
    }
#endif

    dynarr_int_append(&func_depth_stack, &cur_depth);
    while (cur_token != NULL) {
        cur_index = *dynarr_int_back(&tree_iter.index_stack);
        cur_depth = *dynarr_int_back(&tree_iter.depth_stack);
        cur_func_depth = *dynarr_int_back(&func_depth_stack);
        if (cur_token->type == TOK_OP) {
            /* if we left a function */
            if (cur_depth <= cur_func_depth) {
                dynarr_int_pop(&func_depth_stack);
                frame_return(cur_frame);
            }
            /* check assign rule */
            if (cur_token->code == OP_ASSIGN) {
                token_t* left_token
                    = dynarr_token_at(&tree->tokens, tree->lefts[cur_index]);
                is_passed = check_assign_rule(tree, cur_frame, cur_index);
                id_usage[left_token->code] = (uint8_t)1;
            }
            /* check bind argument rule */
            else if (cur_token->code == OP_BIND_ARG) {
                is_passed = check_bind_arg_rule(tree, cur_index);
            }
            /* walk into a function */
            else if (cur_token->code == OP_MAKE_FUNCT) {
                frame_call(cur_frame, cur_index);
                dynarr_int_append(&func_depth_stack, &cur_depth);
            }
        } else if (cur_token->type == TOK_ID) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(
                    "Line %d, col %d, checking identifier usage: ",
                    cur_token->pos.line, cur_token->pos.col
                );
                token_print(cur_token);
                printf("\n");
                fflush(stdout);
            }
#endif
            id_usage[cur_token->code] = (uint8_t)1;
        }
        syntax_tree_iter_next(&tree_iter);
        cur_token = syntax_tree_iter_get(&tree_iter);
    }

    syntax_tree_iter_free(&tree_iter);
    dynarr_int_free(&func_depth_stack);
    frame_free(cur_frame);
    free(cur_frame);
    free(id_usage);
    return is_passed;
}

dynarr_bytecode_t
syntax_tree_compile(const syntax_tree_t* tree, const int root_index)
{
    dynarr_bytecode_t output = dynarr_bytecode_new();
    token_t cur_token = tree->tokens.data[root_index];

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("Compile at token index %d: ", root_index);
        token_print(&cur_token);
        printf("\n");
        fflush(stdout);
    }
#endif

    if (tree->literals[root_index] != NULL) {
        bytecode_array_extend(&output, BOP_PUSH_LITERAL, root_index);
        return output;
    } else if (cur_token.type == TOK_ID) {
        bytecode_array_extend(&output, BOP_FRAME_GET, cur_token.code);
        return output;
    }

    bytecode_op_code_enum bop_code = op_to_bop_code(cur_token.code);
    int left_index = tree->lefts[root_index];
    int right_index = tree->rights[root_index];

    switch (cur_token.code) {
    case OP_MAKE_FUNCT:
    case OP_MAKE_MACRO:
        bytecode_array_extend(&output, bop_code, left_index);
        break;
    case OP_ASSIGN:
    case OP_BIND_ARG: {
        dynarr_bytecode_t right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&right_code);
        bytecode_array_extend(
            &output, bop_code, tree->tokens.data[left_index].code
        );
        break;
    }
    case OP_COND_AND: {
        dynarr_bytecode_t left_code = syntax_tree_compile(tree, left_index);
        dynarr_bytecode_t right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &left_code);
        bytecode_array_extend(&output, BOP_JUMP_FALSE_OR_POP, right_code.size);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&left_code);
        dynarr_bytecode_free(&right_code);
        break;
    }
    case OP_COND_OR: {
        dynarr_bytecode_t left_code = syntax_tree_compile(tree, left_index);
        dynarr_bytecode_t right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &left_code);
        bytecode_array_extend(&output, BOP_JUMP_TRUE_OR_POP, right_code.size);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&left_code);
        dynarr_bytecode_free(&right_code);
        break;
    }
    case OP_EXPRSEP: {
        dynarr_bytecode_t left_code = syntax_tree_compile(tree, left_index);
        dynarr_bytecode_t right_code = syntax_tree_compile(tree, right_index);
        dynarr_bytecode_concat(&output, &left_code);
        dynarr_bytecode_concat(&output, &right_code);
        dynarr_bytecode_free(&left_code);
        dynarr_bytecode_free(&right_code);
        bytecode_array_extend(&output, BOP_POP, 0);
        break;
    }
    default: {
        dynarr_bytecode_t left_code = syntax_tree_compile(tree, left_index);
        if (right_index != -1) {
            dynarr_bytecode_t right_code
                = syntax_tree_compile(tree, right_index);
            if (is_right_associative_op(cur_token.code)) {
                dynarr_bytecode_concat(&output, &right_code);
                dynarr_bytecode_concat(&output, &left_code);
            } else {
                dynarr_bytecode_concat(&output, &left_code);
                dynarr_bytecode_concat(&output, &right_code);
            }
            dynarr_bytecode_free(&right_code);
        } else {
            dynarr_bytecode_concat(&output, &left_code);
        }
        dynarr_bytecode_free(&left_code);
        bytecode_array_extend(&output, bop_code, 0);
        break;
    }
    };
    return output;
}

inline void
syntax_tree_free(syntax_tree_t* tree)
{
    int i;
    /* iterate from back because literal could be pair and we waant to
       free parents first
    */
    for (i = tree->tokens.size - 1; i >= 0; i--) {
        token_t* token = dynarr_token_at(&tree->tokens, i);
        if (token->type == TOK_ID && token->code < RESERVED_ID_NUM) {
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
    dynarr_token_free(&tree->tokens);
    free(tree->lefts);
    /* lefts, rights, and sizes share same heap chunk and left is the
     * head so only need to free left */
    free(tree->literals);
    tree->root_index = -1;

    /* bytecodes */
    if (tree->bytecodes) {
        free(tree->bytecodes);
    }
    dynarr_int_free(&tree->bytecode_indexs);
}

/* set entry_index to -1 to use tree's root index */
inline tree_preorder_iterator_t
syntax_tree_iter_init(const syntax_tree_t* tree, int entry_index)
{
    static int one = 1;
    tree_preorder_iterator_t iter = {
        .tree = tree,
        .index_stack = dynarr_int_new(),
        .depth_stack = dynarr_int_new(),
    };
    dynarr_int_append(
        &iter.index_stack,
        (entry_index == -1) ? (int*)&tree->root_index : &entry_index
    );
    dynarr_int_append(&iter.depth_stack, &one);
    return iter;
}

inline token_t*
syntax_tree_iter_get(tree_preorder_iterator_t* iter)
{
    if (iter->index_stack.size == 0) {
        return NULL;
    }
    return dynarr_token_at(
        &iter->tree->tokens, *dynarr_int_back(&iter->index_stack)
    );
}

inline void
syntax_tree_iter_next(tree_preorder_iterator_t* iter)
{
    if (iter->index_stack.size == 0) {
        return;
    }
    /* get */
    int cur_index = *dynarr_int_back(&iter->index_stack),
        next_depth = *dynarr_int_back(&iter->depth_stack) + 1;
    dynarr_int_pop(&iter->index_stack);
    dynarr_int_pop(&iter->depth_stack);
    /* update */
    if (cur_index != -1) {
        if (iter->tree->rights[cur_index] != -1) {
            dynarr_int_append(
                &iter->index_stack, &iter->tree->rights[cur_index]
            );
            dynarr_int_append(&iter->depth_stack, &next_depth);
        }
        if (iter->tree->lefts[cur_index] != -1) {
            dynarr_int_append(
                &iter->index_stack, &iter->tree->lefts[cur_index]
            );
            dynarr_int_append(&iter->depth_stack, &next_depth);
        }
    }
}

inline void
syntax_tree_iter_free(tree_preorder_iterator_t* tree_iter)
{
    dynarr_int_free(&tree_iter->index_stack);
    dynarr_int_free(&tree_iter->depth_stack);
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
    printf("----- BYTECODES -----\n");
    for (i = 0; i < tree->bytecode_indexs.size; i++) {
        printf(
            "Bytecode of node %d\n", *dynarr_int_at(&tree->bytecode_indexs, i)
        );
        bytecode_array_print(&tree->bytecodes[i], tree->tokens.data);
    }
}
