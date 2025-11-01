#include "dynarr.h"
#include "token.h"

#ifndef TREE_H
#define TREE_H

typedef struct object object_t;

typedef struct token_tree {
    dynarr_t tokens; /* type: token_t */
    object_t** literals;
    int* lefts; /* index of left child, -1 of none */
    int* rights; /* index of right child, -1 of none */
    int* sizes; /* number of nodes under a node */
    int root_index; /* index of the root node */
    int max_id_name; /* number of ids in tree */
} token_tree_t;

extern token_tree_t token_tree_create(dynarr_t tokens);

extern void token_tree_free(token_tree_t* tree);

extern void token_tree_print(const token_tree_t* tree);

typedef struct tree_preorder_iterator {
    const token_tree_t* tree;
    dynarr_t index_stack; /* type: int */
    dynarr_t depth_stack; /* type: int */
} tree_preorder_iterator_t;

extern tree_preorder_iterator_t token_tree_iter_init(const token_tree_t*, int entry_index);

extern token_t* token_tree_iter_get(tree_preorder_iterator_t*);

extern void token_tree_iter_next(tree_preorder_iterator_t*);

extern void token_tree_iter_free(tree_preorder_iterator_t*);

#endif
