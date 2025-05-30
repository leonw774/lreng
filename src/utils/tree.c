#include "tree.h"
#include "arena.h"
#include "errormsg.h"
#include "frame.h"
#include "reserved.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>

tree_t
tree_create(dynarr_t tokens)
{
    int i, token_size = tokens.size;
    tree_t tree = {
        .tokens = tokens,
        .lefts = NULL,
        .rights = NULL,
        .root_index = -1,
        .max_id_name = -1,
    };
    dynarr_t stack = dynarr_new(sizeof(int));
    int* tree_data = malloc(token_size * sizeof(int) * 3);

    /* max_id_name */
    for (i = tokens.size - 1; i >= 0; i--) {
        token_t* t = at(&tokens, i);
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
        token_t* cur_token = at(&tree.tokens, i);
#ifdef ENABLE_DEBUG_LOG_MORE
        token_print(cur_token);
#endif
        if (cur_token->type == TOK_OP) {
            int l_index, r_index = -1;
            if (!is_unary_op(cur_token->name)) {
                r_index = *(int*)back(&stack);
                pop(&stack);
                tree.rights[i] = r_index;
            }
            if (stack.size == 0) {
                throw_syntax_error(
                    cur_token->pos, "Operator has too few operands"
                );
            }
            l_index = *(int*)back(&stack);
            pop(&stack);
            tree.lefts[i] = l_index;
            append(&stack, &i);
#ifdef ENABLE_DEBUG_LOG_MORE
            printf(" L=");
            token_print((token_t*)at(&tree.tokens, l_index));
            printf(" R=");
            if (r_index != -1) {
                token_print((token_t*)at(&tree.tokens, r_index));
            }
#endif
        } else {
            append(&stack, &i);
        }

        tree.sizes[i]
            = (1 + ((tree.lefts[i] == -1) ? 0 : tree.sizes[tree.lefts[i]])
               + ((tree.rights[i] == -1) ? 0 : tree.sizes[tree.rights[i]]));
#ifdef ENABLE_DEBUG_LOG_MORE
        putchar('\n');
#endif
    }

    /* check result */
    if (stack.size != 1) {
#ifdef ENABLE_DEBUG_LOG
        int n;
        printf("REMAINED IN STACK:\n");
        for (n = 0; n < stack.size; n++) {
            int index = *(int*)at(&stack, n);
            token_print((token_t*)at(&tree.tokens, index));
            puts("");
        }
#endif
        sprintf(
            ERR_MSG_BUF,
            "Bad syntax somewhere: %d tokens left in stack when parsing tree",
            stack.size
        );
        throw_syntax_error((linecol_t) { 0, 0 }, ERR_MSG_BUF);
    }

    /* root_index */
    tree.root_index = *(int*)at(&stack, 0);

    /* eval literal */
    /* TODO: extend this to a function and also find pair literal */
    tree.literals = calloc(token_size, sizeof(object_t*));
    for (i = 0; i < token_size; i++) {
        token_t* cur_token = at(&tree.tokens, i);
        if (cur_token->type == TOK_NUM) {
            tree.literals[i] = object_create(
                TYPE_NUM,
                (object_data_t) { .number = number_from_str(cur_token->str) }
            );
        } else if (cur_token->type == TOK_OP && cur_token->name == OP_PAIR) {
            if (tree.lefts[i] == -1 || tree.rights[i] == -1) {
                throw_syntax_error(
                    cur_token->pos, "Pair operator has too few operands"
                );
            } else if (tree.literals[tree.lefts[i]] != NULL
                       && tree.literals[tree.rights[i]] != NULL) {
                tree.literals[i] = object_create(
                    TYPE_PAIR,
                    (object_data_t) {
                        .pair = (pair_t) {
                            .left = tree.literals[tree.lefts[i]],
                            .right = tree.literals[tree.rights[i]],
                        }
                    }
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
    dynarr_free(&stack);
    return tree;
}

inline void
tree_free(tree_t* tree)
{
    int i;
    /* iterate from back because literal could be pair and we waant to free
       parents first
    */
    for (i = tree->tokens.size - 1; i >= 0; i--) {
        token_t* token = at(&tree->tokens, i);
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
            object_free(tree->literals[i]);
        }
    }
    dynarr_free(&tree->tokens);
    free(tree->lefts);
    /* lefts, rights, sizes share same heap chunk so freeing left is enough */
    /*
    free(tree->rights);
    free(tree->sizes);
    */
    free(tree->literals);
    tree->root_index = -1;
}

/* set entry_index to -1 to use tree's root index */
inline tree_preorder_iterator_t
tree_iter_init(const tree_t* tree, int entry_index)
{
    static int one = 1;
    tree_preorder_iterator_t iter = {
        .tree = tree,
        .index_stack = dynarr_new(sizeof(int)),
        .depth_stack = dynarr_new(sizeof(int)),
    };
    append(
        &iter.index_stack,
        (entry_index == -1) ? &tree->root_index : &entry_index
    );
    append(&iter.depth_stack, &one);
    return iter;
}

inline token_t*
tree_iter_get(tree_preorder_iterator_t* iter)
{
    if (iter->index_stack.size == 0) {
        return NULL;
    }
    return at(&iter->tree->tokens, *(int*)back(&iter->index_stack));
}

inline void
tree_iter_next(tree_preorder_iterator_t* iter)
{
    if (iter->index_stack.size == 0) {
        return;
    }
    /* get */
    int cur_index = *(int*)back(&iter->index_stack),
        next_depth = *(int*)back(&iter->depth_stack) + 1;
    pop(&iter->index_stack);
    pop(&iter->depth_stack);
    /* update */
    if (cur_index != -1) {
        if (iter->tree->rights[cur_index] != -1) {
            append(&iter->index_stack, &iter->tree->rights[cur_index]);
            append(&iter->depth_stack, &next_depth);
        }
        if (iter->tree->lefts[cur_index] != -1) {
            append(&iter->index_stack, &iter->tree->lefts[cur_index]);
            append(&iter->depth_stack, &next_depth);
        }
    }
}

inline void
tree_iter_free(tree_preorder_iterator_t* tree_iter)
{
    dynarr_free(&tree_iter->index_stack);
    dynarr_free(&tree_iter->depth_stack);
}

void
tree_print(const tree_t* tree)
{
    tree_preorder_iterator_t iter = tree_iter_init(tree, -1);
    while (iter.index_stack.size != 0) {
        /* get */
        int cur_index = *(int*)back(&iter.index_stack),
            next_depth = (*(int*)back(&iter.depth_stack)) + 1;
        pop(&iter.index_stack);
        pop(&iter.depth_stack);

        if (cur_index != -1) {
            /* print */
            printf("%*c", next_depth - 1, ' ');
            token_print((token_t*)at(&tree->tokens, cur_index));
            printf(" (%d)\n", cur_index);
            fflush(stdout);

            /* update */
            if (tree->rights[cur_index] != -1) {
                append(&iter.index_stack, &tree->rights[cur_index]);
                append(&iter.depth_stack, &next_depth);
            }
            if (tree->lefts[cur_index] != -1) {
                append(&iter.index_stack, &tree->lefts[cur_index]);
                append(&iter.depth_stack, &next_depth);
            }
        }
    }
    tree_iter_free(&iter);
}
