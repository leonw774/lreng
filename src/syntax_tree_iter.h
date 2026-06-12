#include "syntax_tree.h"

#ifndef SYNTEX_TREE_ITER_H
#define SYNTEX_TREE_ITER_H

typedef struct tree_preorder_iterator {
    const syntax_tree_t* tree;
    dynarr_int_t index_stack;
    dynarr_int_t depth_stack;
} tree_preorder_iterator_t;

extern tree_preorder_iterator_t
syntax_tree_iter_init(const syntax_tree_t*, int entry_index);

extern token_t* syntax_tree_iter_get(tree_preorder_iterator_t* tree_iter);

extern void syntax_tree_iter_next(tree_preorder_iterator_t* tree_iter);

extern void syntax_tree_iter_free(tree_preorder_iterator_t* tree_iter);

#endif
