#include "bytecode.h"
#include "token.h"
#include "utils/dynarr_int.h"

#ifndef TREE_H
#define TREE_H

typedef struct object object_t;

typedef struct syntax_tree {
    dynarr_token_t tokens;
    object_t** literals;
    dynarr_bytecode_t bytecodes;
    /* sorted array of index of nodes that can be called to */
    dynarr_int_t entry_indexs;
    dynarr_int_t bytecode_start_index;
    int root_index; /* index of the root node */
    int* lefts; /* index of left child, -1 of none */
    int* rights; /* index of right child, -1 of none */
    int max_id_code; /* number of ids in tree */
    const char** id_code_str_map; /* map of id's code to their name string */
} syntax_tree_t;

extern syntax_tree_t syntax_tree_create(dynarr_token_t tokens);

extern int syntax_tree_check_semantic(const syntax_tree_t* tree);

extern void syntax_tree_optimatize(syntax_tree_t* tree);

extern dynarr_bytecode_t
syntax_tree_compile(const syntax_tree_t* tree, const int root_index);

extern int syntax_tree_get_bytecode_start_index(
    const syntax_tree_t* tree, const int node_index
);

extern void syntax_tree_free(syntax_tree_t* tree);

extern void syntax_tree_print(const syntax_tree_t* tree);

#endif
