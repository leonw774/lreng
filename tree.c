#include <stdio.h>
#include "tree.h"

void
free_tree(tree_t* t) {
    free_dynarr(&t->tokens);
    free(t->lefts);
    free(t->rights);
    t->root = -1;
}

tree_infix_iterater_t
tree_iter_init(tree_t tree) {
    tree_infix_iterater_t iter = {
        .tree = tree,
        .index_stack = new_dynarr(sizeof(int)),
        .depth_stack = new_dynarr(sizeof(int))
    };
    int zero = 0;
    append(&iter.index_stack, &tree.root);
    append(&iter.depth_stack, &zero);
    return iter;
}

token_t*
tree_iter_get(tree_infix_iterater_t* iter) {
    if (iter->index_stack.count == 0) {
        return NULL;
    }
    int cur_index = *(int*) back(&iter->index_stack);
    return &((token_t*) (iter->tree.tokens.data))[cur_index];
}

void
tree_iter_next(tree_infix_iterater_t* iter) {
    if (iter->index_stack.count == 0) {
        return;
    }
    /* get */
    int cur_index = *(int*) back(&iter->index_stack),
        next_depth = *(int*) back(&iter->depth_stack) + 1;
    pop(&iter->index_stack);
    pop(&iter->depth_stack);
    /* update */
    if (cur_index != -1) {
        if (iter->tree.rights[cur_index] != -1) {
            append(&iter->index_stack, &iter->tree.rights[cur_index]);
            append(&iter->depth_stack, &next_depth);
        }
        if (iter->tree.lefts[cur_index] != -1) {
            append(&iter->index_stack, &iter->tree.lefts[cur_index]);
            append(&iter->depth_stack, &next_depth);
        }
    }
}

void
free_tree_iter(tree_infix_iterater_t* tree_iter) {
    free_dynarr(&tree_iter->index_stack);
    free_dynarr(&tree_iter->depth_stack);
}

void
print_tree(tree_t tree) {
    tree_infix_iterater_t iter = tree_iter_init(tree);
    while (iter.index_stack.count != 0) {
        /* get */
        int cur_index = *(int*) back(&iter.index_stack),
            next_depth = (*(int*) back(&iter.depth_stack)) + 1;
        pop(&iter.index_stack);
        pop(&iter.depth_stack);

        if (cur_index != -1) {
            /* print */
            token_t cur_token_ptr =
                ((token_t*) (iter.tree.tokens.data))[cur_index];
            printf("%*c", next_depth - 1, ' ');
            print_token(cur_token_ptr);
            printf("\n");
            fflush(stdout);
            
            /* update */
            if (iter.tree.rights[cur_index] != -1) {
                append(&iter.index_stack, &iter.tree.rights[cur_index]);
                append(&iter.depth_stack, &next_depth);
            }
            if (iter.tree.lefts[cur_index] != -1) {
                append(&iter.index_stack, &iter.tree.lefts[cur_index]);
                append(&iter.depth_stack, &next_depth);
            }
        }
    }
    free_tree_iter(&iter);
}