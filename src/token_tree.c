#include "token_tree.h"
#include "frame.h"
#include "reserved.h"
#include "token.h"
#include "utils/arena.h"
#include "utils/errormsg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* prepare the tree struct for fast lookup and pre-eval of the postfix tokens */
token_tree_t
token_tree_create(dynarr_token_t postfix_tokens)
{
    int i = 0, token_size = postfix_tokens.size;
    token_tree_t tree = {
        .tokens = postfix_tokens,
        .lefts = NULL,
        .rights = NULL,
        .root_index = -1,
        .max_id_name = -1,
    };
    dynarr_int_t index_stack = dynarr_int_new();
    int* tree_data = malloc(token_size * sizeof(int) * 3);
    assert(tree_data != NULL);

    /* find max_id_name */
    for (i = postfix_tokens.size - 1; i >= 0; i--) {
        token_t* t = dynarr_token_at(&postfix_tokens, i);
        if (t->type == TOK_ID && tree.max_id_name < t->name) {
            tree.max_id_name = t->name;
        }
    }

    /* lefts, rights and sizes */
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
            if (!is_unary_op(cur_token->name)) {
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

    /* check result */
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

    /* root_index */
    tree.root_index = *dynarr_int_at(&index_stack, 0);

    /* eval literal */
    /* TODO: extend this to a function and also find pair literal */
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
        } else if (cur_token->type == TOK_OP && cur_token->name == OP_PAIR) {
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

    /* free things */
    dynarr_int_free(&index_stack);
    return tree;
}

inline void
token_tree_free(token_tree_t* tree)
{
    int i;
    /* iterate from back because literal could be pair and we waant to free
       parents first
    */
    for (i = tree->tokens.size - 1; i >= 0; i--) {
        token_t* token = dynarr_token_at(&tree->tokens, i);
        if (token->type == TOK_ID && token->name < RESERVED_ID_NUM) {
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
    /* lefts, rights, and sizes share same heap chunk and left is the head
       so only need to free left
    */
    free(tree->literals);
    tree->root_index = -1;
}

/* set entry_index to -1 to use tree's root index */
inline tree_preorder_iterator_t
token_tree_iter_init(const token_tree_t* tree, int entry_index)
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
token_tree_iter_get(tree_preorder_iterator_t* iter)
{
    if (iter->index_stack.size == 0) {
        return NULL;
    }
    return dynarr_token_at(
        &iter->tree->tokens, *dynarr_int_back(&iter->index_stack)
    );
}

inline void
token_tree_iter_next(tree_preorder_iterator_t* iter)
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
token_tree_iter_free(tree_preorder_iterator_t* tree_iter)
{
    dynarr_int_free(&tree_iter->index_stack);
    dynarr_int_free(&tree_iter->depth_stack);
}

void
token_tree_print(const token_tree_t* tree)
{
    tree_preorder_iterator_t iter = token_tree_iter_init(tree, -1);
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
    token_tree_iter_free(&iter);
}
