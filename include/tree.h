#include "dynarr.h"
#include "token.h"

#ifndef TREE_H
#define TREE_H

typedef struct object object_t;

typedef struct tree {
    dynarr_t tokens; /* type: token_t */
    object_t** literals;
    int* lefts; /* index of left child, -1 of none */
    int* rights; /* index of right child, -1 of none */
    int* sizes; /* number of nodes under a node */
    int root_index; /* index of the root node */
    int max_id_name; /* number of ids in tree */
} tree_t;

extern tree_t tree_create(dynarr_t tokens, const int is_debug);

extern void tree_free(tree_t* tree);

extern void tree_print(const tree_t* tree);

typedef struct tree_preorder_iterator {
    const tree_t* tree;
    dynarr_t index_stack; /* type: int */
    dynarr_t depth_stack; /* type: int */
} tree_preorder_iterator_t;

extern tree_preorder_iterator_t tree_iter_init(const tree_t*, int entry_index);

extern token_t* tree_iter_get(tree_preorder_iterator_t*);

extern void tree_iter_next(tree_preorder_iterator_t*);

extern void tree_iter_free(tree_preorder_iterator_t*);

#endif
