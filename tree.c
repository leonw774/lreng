#include "tree.h"

tree_infix_iterater
init_infix_iterator(tree* root) {
    return (tree_infix_iterater) {
        .root = root,
        .treeptr_stack = new_dyn_arr(sizeof(tree*)),
        .depth_stack = new_dyn_arr(sizeof(int))
    };
}

tree*
tree_read(tree_infix_iterater* iter) {
    if (iter->treeptr_stack.count == 0) {
        return NULL;
    }
    tree* cur_tree_ptr = *(tree**) back(&iter->treeptr_stack);
    pop(&iter->treeptr_stack);
    if (cur_tree_ptr->right != NULL) {
        append(&iter->treeptr_stack, &cur_tree_ptr->right);
    }
    if (cur_tree_ptr->left != NULL) {
        append(&iter->treeptr_stack, &cur_tree_ptr->left);
    }
    return cur_tree_ptr;
}
