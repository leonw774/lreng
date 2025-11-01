#include "objects.h"

typedef struct eval_tree_node eval_tree_node_t;

typedef struct eval_tree_node {
    int token_index;
    object_t* object;
    eval_tree_node_t* left;
    eval_tree_node_t* right;
} eval_tree_node_t;

extern eval_tree_node_t* eval_tree_node_create(int token_index);
extern void eval_tree_node_free(eval_tree_node_t* node);
