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
check_assign_rule(
    const syntax_tree_t* tree, frame_t* frame, const int index
)
{
    token_t* cur_token = dynarr_token_at(&tree->tokens, index);
    token_t* left_token = dynarr_token_at(&tree->tokens, tree->lefts[index]);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "Line %d, col %d, checking assign rule: ", cur_token->pos.line,
            cur_token->pos.col
        );
        token_print(left_token);
        printf(" ");
        token_print(cur_token);
        printf("\n");
    }
#endif
    if (left_token->type != TOK_ID) {
        const char* err_msg = "Left side of %s is not identifier.";
        sprintf(ERR_MSG_BUF, err_msg, OP_NAMES[cur_token->code]);
        print_semantic_error(left_token->pos, ERR_MSG_BUF);
        return 0;
    } else if (frame_get(frame, left_token->code)) {
        const char* err_msg = "Repeated initialization of identifier '%s'";
        sprintf(ERR_MSG_BUF, err_msg, left_token->str);
        print_semantic_error(left_token->pos, ERR_MSG_BUF);
        return 0;
    } else {
        frame_set(
            frame, left_token->code,
            (object_t*)&RESERVED_OBJS[RESERVED_ID_CODE_NULL]
        );
    }
    return 1;
}

int
check_bind_arg_rule(const syntax_tree_t* tree, const int index)
{
    token_t* cur_token = dynarr_token_at(&tree->tokens, index);
    token_t* left_token = dynarr_token_at(&tree->tokens, tree->lefts[index]);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(
            "Line %d, col %d, checking argument binder: ", cur_token->pos.line,
            cur_token->pos.col
        );
        token_print(left_token);
        printf(" ");
        token_print(cur_token);
        printf("\n");
    }
#endif
    if (left_token->type != TOK_ID) {
        const char* err_msg = "Left side of %s is not identifier.";
        sprintf(ERR_MSG_BUF, err_msg, OP_NAMES[cur_token->code]);
        print_semantic_error(left_token->pos, ERR_MSG_BUF);
        return 0;
    }
    return 1;
}
