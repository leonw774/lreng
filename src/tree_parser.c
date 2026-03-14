#include "errormsg.h"
#include "main.h"
#include "token_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT_TYPE_NAME_MSG                                                      \
    "Expect unary operator, number, identifier, or open bracket. Get '%s'"
#define EXPECT_INFIX_MSG "Expect binary operator or closing bracket. Get '%s'"

/* is the precedence o1 < o2 ? */
static inline unsigned char
op_prec_lt(op_name_enum o1, op_name_enum o2)
{
    /*
    printf(
        "op_prec_lt: %s %d  %s %d\n",
        OP_STRS[o1], get_op_precedence(o1),
        OP_STRS[o2], get_op_precedence(o2)
    );
    */
    return (
        get_op_precedence(o1) < get_op_precedence(o2)
        || (get_op_precedence(o1) == get_op_precedence(o2)
            && !is_right_associative_op(o2))
    );
}

/* FUTURE WORK: rewrite parsing with pratt parser */

/* return rearranged tokens in postfix order */
dynarr_token_t
shunting_yard(const dynarr_token_t tokens)
{
    enum EXPECT {
        TYPE_NAMEER,
        INFIXER
        /* infixer include infix (binary ops) and postfix (closing bracket) */
    };
    int expectation = TYPE_NAMEER;
    int i;
    dynarr_token_t output = dynarr_token_new();
    dynarr_token_t op_stack = dynarr_token_new();

#ifdef ENABLE_DEBUG_LOG
    int prev_output_count = 0;
    if (global_is_enable_debug_log) {
        printf("shunting_yard\n");
    }
#endif

    for (i = 0; i < tokens.size; i++) {
        token_t* cur_token = dynarr_token_at(&tokens, i);
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("expect=%s token=", (expectation ? "INFIX" : "TYPE_NAME"));
            token_print(cur_token);
            printf("\n");
        }
#endif
        if (cur_token->type == TOK_OP || cur_token->type == TOK_LB) {
            /* is an operator or left bracket */
            int is_prefixable = is_prefixable_op(cur_token->name);
            token_t* stack_top = dynarr_token_back(&op_stack);
            /* check expectation */
            if (is_prefixable && expectation == INFIXER) {
                sprintf(
                    ERR_MSG_BUF, EXPECT_INFIX_MSG, OP_STRS[cur_token->name]
                );
                throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
            }
            if (!is_prefixable && expectation == TYPE_NAMEER) {
                sprintf(
                    ERR_MSG_BUF, EXPECT_TYPE_NAME_MSG, OP_STRS[cur_token->name]
                );
                throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
            }
            /* update expectation */
            if (!is_prefixable) {
                expectation = TYPE_NAMEER;
            }
            /* positive sign do nothing, can discard */
            if (cur_token->name == OP_POS) {
                continue;
            }
            /* pop stack until top is left bracket or precedence is not lower */
            while (stack_top != NULL && stack_top->type != TOK_LB
                   && op_prec_lt(stack_top->name, cur_token->name)) {
                dynarr_token_append(&output, stack_top);
                dynarr_token_pop(&op_stack);
                stack_top = dynarr_token_back(&op_stack);
            }
            /* push the op to op stack */
            dynarr_token_append(&op_stack, cur_token);
        } else if (cur_token->type == TOK_RB) {
            /* is a right bracket */
            token_t* stack_top = dynarr_token_back(&op_stack);
            op_name_enum top_op, cur_op;
            /* check expectation */
            if (expectation == TYPE_NAMEER) {
                sprintf(
                    ERR_MSG_BUF, EXPECT_TYPE_NAME_MSG, OP_STRS[cur_token->name]
                );
                throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
            }
            /* update expectation */
            expectation = INFIXER;
            /* pop stack until top is left braclet */
            while (stack_top != NULL && stack_top->type != TOK_LB) {
                dynarr_token_append(&output, stack_top);
                dynarr_token_pop(&op_stack);
                stack_top = dynarr_token_back(&op_stack);
            }
            /* check bad case */
            if (stack_top == NULL) {
                throw_syntax_error(
                    cur_token->pos,
                    "Unmatched closing bracket: Cannot find opening bracket"
                );
            }
            /* pop out the left bracket */
            dynarr_token_pop(&op_stack);
            top_op = stack_top->name;
            cur_op = cur_token->name;
            /* special operators replacement */
            if (top_op == OP_FCALL && cur_op == OP_RPAREN) {
                /* create function call */
                token_t fcall = { NULL, OP_FCALL, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &fcall);
            } else if (top_op == OP_LCURLY && cur_op == OP_RCURLY) {
                /* function maker */
                token_t fmake = { NULL, OP_FMAKE, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &fmake);
            } else if (top_op == OP_LSQUARE && cur_op == OP_RSQUARE) {
                /* pure function maker */
                token_t pfmake = { NULL, OP_MMAKE, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &pfmake);
            } else if (!(top_op == OP_LPAREN && cur_op == OP_RPAREN)) {
                sprintf(
                    ERR_MSG_BUF,
                    "Unmatched closing bracket: Found '%s' to '%s'",
                    OP_STRS[top_op], OP_STRS[cur_op]
                );
                throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
            }
        } else {
            /* is other */
            /* check expectation */
            if (expectation == INFIXER) {
                sprintf(ERR_MSG_BUF, EXPECT_INFIX_MSG, cur_token->str);
                throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
            }
            /* update expectation */
            expectation = INFIXER;
            /* just append */
            dynarr_token_append(&output, cur_token);
        }
#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("op_stack=");
            int j;
            for (j = 0; j < op_stack.size; j++) {
                token_print(dynarr_token_at(&op_stack, j));
                printf(" ");
            }
            printf("\n");
            if (prev_output_count != output.size) {
                prev_output_count = output.size;
                printf("new_output=");
                token_print(dynarr_token_back(&output));
                printf("\n");
            }
        }
#endif
    } /* end for */

    /* pop stack to output until stack is empty */
    while (op_stack.size > 0) {
        dynarr_token_append(&output, dynarr_token_back(&op_stack));
        dynarr_token_pop(&op_stack);
    }
    dynarr_token_free(&op_stack);
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("result=");
        for (i = 0; i < output.size; i++) {
            token_print(dynarr_token_at(&output, i));
            printf(" ");
        }
        printf("\n");
    }
#endif
    return output;
}

token_tree_t
parse_tokens_to_tree(const dynarr_token_t tokens)
{
    /* tokens */
    token_tree_t tree = token_tree_create(shunting_yard(tokens));
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("final_tree=\n");
        token_tree_print(&tree);
    }
#endif
    return tree;
}
