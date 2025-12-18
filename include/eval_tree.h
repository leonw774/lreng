#include "objects.h"

typedef struct eval_tree eval_tree_t;

typedef struct eval_tree {
    int token_index;
    object_t* object;
    eval_tree_t* left;
    eval_tree_t* right;
} eval_tree_t;

static inline eval_tree_t*
eval_tree_node_create(int token_index)
{
    eval_tree_t* node = calloc(1, sizeof(eval_tree_t));
    node->token_index = token_index;
    return node;
}

/* recursively delete a node and its children, deref the objects they hold */
static inline void
eval_tree_node_free(eval_tree_t* node)
{
#ifdef ENABLE_DEBUG_MORE_LOG
    printf("eval_tree_node_free: freeing node %p and object ", node);
    if (node->object) {
        object_print(node->object, '\n');
    } else {
        printf("nullptr\n");
    }
    fflush(stdout);
#endif
    if (node->left) {
        eval_tree_node_free(node->left);
    }
    if (node->right) {
        eval_tree_node_free(node->right);
    }
    if (node->object) {
        object_deref(node->object);
    }
    free(node);
}
