#include "operators.h"
#include "syntax_tree.h"
#include "syntax_tree_iter.h"
#include <stdlib.h>

void
optimize_remove_op_pos(syntax_tree_t* tree)
{
    int i;
    for (i = 0; i < tree->tokens.size; ++i) {
        token_t token = tree->tokens.data[i];
        if (token.type == TOK_OP && token.code == OP_POS) {
            int keep_node = tree->lefts[i];
            /* redirect any parent that points to this OP_POS node to
             * point to the operand instead. we don't remove tokens from
             * the arrays; unreachable OP_POS nodes will simply be skipped.
             */
            int j;
            for (j = 0; j < tree->tokens.size; ++j) {
                if (tree->lefts[j] == i) {
                    tree->lefts[j] = keep_node;
                }
                if (tree->rights[j] == i) {
                    tree->rights[j] = keep_node;
                }
            }
            if (tree->root_index == i) {
                tree->root_index = keep_node;
            }
        }
    }
}

static int
is_side_effect_op(op_code_enum code)
{
    return (
        code == OP_ASSIGN || code == OP_BIND_ARG || code == OP_CALL
        || code == OP_CALLR || code == OP_COND_CALL || code == OP_COND_PAIR_CALL
    );
}

static int
is_subtree_side_effect_free(const syntax_tree_t* tree, int entry_index)
{
    tree_preorder_iterator_t iter = syntax_tree_iter_init(tree, entry_index);
    token_t* cur_token = syntax_tree_iter_get(&iter);
    int has_side_effect = 0;
    while (cur_token != NULL && has_side_effect == 0) {
        if (cur_token->type == TOK_OP && is_side_effect_op(cur_token->code)) {
            has_side_effect = 1;
        }
        syntax_tree_iter_next(&iter);
        cur_token = syntax_tree_iter_get(&iter);
    }
    syntax_tree_iter_free(&iter);
    return !has_side_effect;
}

void
optimize_remove_no_side_effect_expr(syntax_tree_t* tree)
{
    int i;
    for (i = 0; i < tree->tokens.size; ++i) {
        token_t token = tree->tokens.data[i];
        if (token.type == TOK_OP && token.code == OP_EXPRSEP) {
            int left_index = tree->lefts[i];
            if (is_subtree_side_effect_free(tree, left_index)) {
                int keep_node = tree->rights[i];
                /* redirect any parent that points to this OP_POS node to
                 * point to the operand instead. we don't remove tokens from
                 * the arrays; unreachable OP_POS nodes will simply be skipped.
                 */
                int j;
                for (j = 0; j < tree->tokens.size; ++j) {
                    if (tree->lefts[j] == i) {
                        tree->lefts[j] = keep_node;
                    }
                    if (tree->rights[j] == i) {
                        tree->rights[j] = keep_node;
                    }
                }
                if (tree->root_index == i) {
                    tree->root_index = keep_node;
                }
            }
        }
    }
}
