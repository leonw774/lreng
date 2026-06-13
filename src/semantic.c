#include "semantic.h"
#include "objects.h"
#include "operators.h"
#include "reserved.h"
#include "utils/debug_flag.h"
#include "utils/errormsg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int
is_assignable_node(const syntax_tree_t* tree, frame_t* frame, const int index)
{
    token_t* cur_token = dynarr_token_at(&tree->tokens, index);
    if (cur_token->type == TOK_ID) {
        if (frame_get(frame, cur_token->code)) {
            const char* err_msg = "Repeated initialization of identifier '%s'";
            sprintf(ERR_MSG_BUF, err_msg, cur_token->str);
            print_semantic_error(cur_token->pos, ERR_MSG_BUF);
            return 0;
        }
        frame_set(
            frame, cur_token->code,
            (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
        );
        return 1;
    } else if (cur_token->type == TOK_OP && cur_token->code == OP_PAIR) {
        /* pair unpacking assignment:
         * check recursively if all are uninitialized identifier
         */
        int is_left_passed
            = is_assignable_node(tree, frame, tree->lefts[index]);
        int is_right_passed
            = is_assignable_node(tree, frame, tree->rights[index]);
        return is_left_passed && is_right_passed;
    } else {
        const char* err_msg = "Node at %d is not assignable: ";
        sprintf(ERR_MSG_BUF, err_msg, index);
        print_semantic_error(cur_token->pos, ERR_MSG_BUF);
        token_print(cur_token);
        printf("\n");
        return 0;
    }
}

/* check if the left node of assignment is assignable */
int
check_assign_rule(const syntax_tree_t* tree, frame_t* frame, const int index)
{
    token_t* left_token = dynarr_token_at(&tree->tokens, tree->lefts[index]);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        token_t* cur_token = dynarr_token_at(&tree->tokens, index);
        printf(
            "Line %d, col %d, check assign rule at index %d (",
            cur_token->pos.line, cur_token->pos.col, index
        );
        token_print(left_token);
        printf(")\n");
    }
#endif
    if (!is_assignable_node(tree, frame, tree->lefts[index])) {
        const char* err_msg = "Left side of %s is not assignable.";
        sprintf(ERR_MSG_BUF, err_msg, OP_NAMES[left_token->code]);
        print_semantic_error(left_token->pos, ERR_MSG_BUF);
        return 0;
    }
    return 1;
}

int
check_bind_arg_rule(const syntax_tree_t* tree, frame_t* frame, const int index)
{
    token_t* cur_token = dynarr_token_at(&tree->tokens, index);
    token_t* left_token = dynarr_token_at(&tree->tokens, tree->lefts[index]);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "Line %d, col %d, check argument binder at index %d\n",
            cur_token->pos.line, cur_token->pos.col, index
        );
    }
#endif
    if (left_token->type != TOK_ID) {
        const char* err_msg = "Left side of %s is not identifier.";
        sprintf(ERR_MSG_BUF, err_msg, OP_NAMES[cur_token->code]);
        print_semantic_error(left_token->pos, ERR_MSG_BUF);
        return 0;
    }
    frame_set(
        frame, left_token->code,
        (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
    );
    return 1;
}

int
check_id_init_rule(const syntax_tree_t* tree, frame_t* frame, const int index)
{
    token_t* cur_token = dynarr_token_at(&tree->tokens, index);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "Line %d, col %d, check identifier usage: ", cur_token->pos.line,
            cur_token->pos.col
        );
        token_print(cur_token);
        printf("\n");
        fflush(stdout);
    }
#endif
    if (cur_token->code >= RESERVED_ID_CODE_END_OF_ENUM
        && frame_get(frame, cur_token->code) == NULL) {
        const char* err_msg = "identifier %s used uninitialized.";
        sprintf(ERR_MSG_BUF, err_msg, cur_token->str);
        print_semantic_error(cur_token->pos, ERR_MSG_BUF);
        return 0;
    }
    return 1;
}
