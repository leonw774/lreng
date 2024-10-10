#include "token.h"
#include "dyn_arr.h"

#ifndef TREE_H
#define TREE_H

typedef struct {
    token token;
    struct tree* left;
    struct tree* right;
} tree;

typedef struct {
    tree* root;
    dyn_arr treeptr_stack;
    dyn_arr depth_stack;
} tree_infix_iterater;

extern tree_infix_iterater
init_infix_iterator(tree* root);

extern tree*
tree_read(tree_infix_iterater* iter);

#endif
