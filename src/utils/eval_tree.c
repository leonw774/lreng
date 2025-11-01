#include "eval_tree.h"
#include "objects.h"
#include <stdio.h>

eval_tree_node_t*
eval_tree_node_create(int token_index)
{
    eval_tree_node_t* node = calloc(1, sizeof(eval_tree_node_t));
    node->token_index = token_index;
    return node;
}

/* recursively delete a node and its children, deref the objects they hold */
void
eval_tree_node_free(eval_tree_node_t* node)
{
#ifdef ENABLE_DEBUG_LOG
    printf("eval_tree_node_free: freeing node %p and object ", node);
    if (node->object) {
        object_print(node->object, '\n');
    } else {
        printf("nullptr");
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