#include <stdio.h>
#include <stdlib.h>
#include "frame.h"
#include "token.h"
#include "tree.h"

inline void
free_tree(tree_t* tree) {
    int i;
    for (i = 0; i < tree->tokens.size; i++) {
        token_t* token = &((token_t*) tree->tokens.data)[i];
        if (token->type == TOK_ID && token->name < RESERVED_ID_NUM) {
            continue;
        }
        free_token_str(token);
    }
    free_dynarr(&tree->tokens);
    free(tree->lefts);
    free(tree->rights);
    tree->root_index = -1;
}

/* set entry_index to -1 to use tree's root index */
tree_preorder_iterator_t
tree_iter_init(const tree_t* tree, int entry_index) {
    int one = 1;
    tree_preorder_iterator_t iter = {
        .tree = tree,
        .index_stack = new_dynarr(sizeof(int)),
        .depth_stack = new_dynarr(sizeof(int))
    };
    append(
        &iter.index_stack,
        (entry_index == -1) ? &tree->root_index : &entry_index
    );
    append(&iter.depth_stack, &one);
    return iter;
}

token_t*
tree_iter_get(tree_preorder_iterator_t* iter) {
    if (iter->index_stack.size == 0) {
        return NULL;
    }
    int cur_index = *(int*) back(&iter->index_stack);
    return &((token_t*) (iter->tree->tokens.data))[cur_index];
}

void
tree_iter_next(tree_preorder_iterator_t* iter) {
    if (iter->index_stack.size == 0) {
        return;
    }
    /* get */
    int cur_index = *(int*) back(&iter->index_stack),
        next_depth = *(int*) back(&iter->depth_stack) + 1;
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
free_tree_iter(tree_preorder_iterator_t* tree_iter) {
    free_dynarr(&tree_iter->index_stack);
    free_dynarr(&tree_iter->depth_stack);
}

void
print_tree(const tree_t* tree) {
    tree_preorder_iterator_t iter = tree_iter_init(tree, -1);
    while (iter.index_stack.size != 0) {
        /* get */
        int cur_index = *(int*) back(&iter.index_stack),
            next_depth = (*(int*) back(&iter.depth_stack)) + 1;
        pop(&iter.index_stack);
        pop(&iter.depth_stack);

        if (cur_index != -1) {
            /* print */
            token_t cur_token =
                ((token_t*) (iter.tree->tokens.data))[cur_index];
            printf("%*c", next_depth - 1, ' ');
            print_token(cur_token);
            printf(" (%d)\n", cur_index);
            fflush(stdout);
            
            /* update */
            if (iter.tree->rights[cur_index] != -1) {
                append(&iter.index_stack, &iter.tree->rights[cur_index]);
                append(&iter.depth_stack, &next_depth);
            }
            if (iter.tree->lefts[cur_index] != -1) {
                append(&iter.index_stack, &iter.tree->lefts[cur_index]);
                append(&iter.depth_stack, &next_depth);
            }
        }
    }
    free_tree_iter(&iter);
}
