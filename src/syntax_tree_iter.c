#include "syntax_tree_iter.h"

/* set entry_index to -1 to use tree's root index */
tree_preorder_iterator_t
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

token_t*
syntax_tree_iter_get(tree_preorder_iterator_t* iter)
{
    if (iter->index_stack.size == 0) {
        return NULL;
    }
    return dynarr_token_at(
        &iter->tree->tokens, *dynarr_int_back(&iter->index_stack)
    );
}

void
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

void
syntax_tree_iter_free(tree_preorder_iterator_t* tree_iter)
{
    dynarr_int_free(&tree_iter->index_stack);
    dynarr_int_free(&tree_iter->depth_stack);
}
