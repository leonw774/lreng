#include "bytecode.h"
#include "token.h"
#include "utils/dynarr_int.h"

#ifndef TREE_H
#define TREE_H

typedef struct object object_t;

typedef struct syntax_tree {
    dynarr_token_t tokens;
    object_t** literals;
    dynarr_bytecode_t* bytecodes;
    dynarr_int_t bytecode_indexs;
    int* lefts; /* index of left child, -1 of none */
    int* rights; /* index of right child, -1 of none */
    int* sizes; /* number of nodes under a node */
    int root_index; /* index of the root node */
    int max_id_name; /* number of ids in tree */
} syntax_tree_t;

extern syntax_tree_t syntax_tree_create(dynarr_token_t tokens);

extern int syntax_tree_check_semantic(const syntax_tree_t* tree);

extern dynarr_bytecode_t
syntax_tree_compile(const syntax_tree_t* tree, const int root_index);

extern void syntax_tree_free(syntax_tree_t* tree);

extern void syntax_tree_print(const syntax_tree_t* tree);

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
