#include "objects.h"

typedef struct eval_tree eval_tree_t;

typedef struct eval_tree {
    int token_index;
    object_t* object;
    eval_tree_t* left;
    eval_tree_t* right;
} eval_tree_t;
