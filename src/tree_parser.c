#include "errormsg.h"
#include "main.h"
#include "token_tree.h"
#include "reserved.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT_PREFIX_MSG                                                      \
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

void
insert_op(token_t* op_token, dynarr_token_t* op_stack, dynarr_token_t* output)
{
    token_t* stack_top = dynarr_token_back(op_stack);
    /* pop stack until top is left bracket or precedence is not lower */
    while (stack_top != NULL && stack_top->type != TOK_LB
           && op_prec_lt(stack_top->name, op_token->name)) {
        dynarr_token_append(output, stack_top);
        dynarr_token_pop(op_stack);
        stack_top = dynarr_token_back(op_stack);
    }
    /* push the op to op stack */
    dynarr_token_append(op_stack, op_token);
}

/* return rearranged tokens in postfix order */
dynarr_token_t
shunting_yard(const dynarr_token_t tokens)
{
    enum EXPECT {
        PREFIXER,
        INFIXER
        /* infixer include infix (binary ops) and postfix (closing bracket) */
    };
    int expectation = PREFIXER;
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
        int is_prefixer = 1;
        token_t* cur_token = dynarr_token_at(&tokens, i);

#ifdef ENABLE_DEBUG_LOG
        if (global_is_enable_debug_log) {
            printf("expect=%s token=", (expectation ? "INFIX" : "PREFIX"));
            token_print(cur_token);
            printf("\n");
        }
#endif

        /* update state */
        if (cur_token->type == TOK_OP) {
            is_prefixer = is_prefixable_op(cur_token->name);
        } else if (cur_token->type == TOK_LB) {
            is_prefixer = 1;
        } else if (cur_token->type == TOK_RB) {
            is_prefixer = 0;
        } else if (cur_token->type == TOK_ID || cur_token->type == TOK_NUM) {
            is_prefixer = 1;
        }

        /** insert function call operator to op stack if expects infixer but
            current token type is prefixer and update state accordingly
         */
        if (expectation == INFIXER && is_prefixer) {
            token_t fcall = { NULL, OP_FCALL, TOK_OP, cur_token->pos };
            insert_op(&fcall, &op_stack, &output);
            is_prefixer = 1;
            expectation = PREFIXER;
        }

        /* check expectation */
        if (is_prefixer && expectation == INFIXER) {
            sprintf(ERR_MSG_BUF, EXPECT_INFIX_MSG, OP_STRS[cur_token->name]);
            throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
        }
        if (!is_prefixer && expectation == PREFIXER) {
            sprintf(ERR_MSG_BUF, EXPECT_PREFIX_MSG, OP_STRS[cur_token->name]);
            throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
        }

        if (cur_token->type == TOK_OP || cur_token->type == TOK_LB) {
            /* is an operator or left bracket */
            /* update expectation */
            if (!is_prefixer) {
                expectation = PREFIXER;
            }
            /* positive sign do nothing, can discard */
            if (cur_token->name == OP_POS) {
                continue;
            }
            insert_op(cur_token, &op_stack, &output);

        } else if (cur_token->type == TOK_RB) {
            /* is a right bracket */
            token_t* stack_top = dynarr_token_back(&op_stack);
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
            /* special operators replacement: function and macro maker */
            if (stack_top->name == OP_LPAREN && cur_token->name == OP_RPAREN) {
                /* do nothing */
            } else if (stack_top->name == OP_LCURLY && cur_token->name == OP_RCURLY) {
                /* function maker */
                token_t fmake = { NULL, OP_FMAKE, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &fmake);
            } else if (stack_top->name == OP_LSQUARE
                       && cur_token->name == OP_RSQUARE) {
                /* macro maker */
                token_t pfmake = { NULL, OP_MMAKE, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &pfmake);
            } else {
                sprintf(
                    ERR_MSG_BUF,
                    "Unmatched closing bracket: Found '%s' to '%s'",
                    OP_STRS[stack_top->name], OP_STRS[cur_token->name]
                );
                throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
            }
        } else {
            /* is identifier or number */
            /* update expectation */
            expectation = INFIXER;
            /* just append to output */
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

// dynarr_token_t
// pratt_parser(const dynarr_token_t tokens)
// {
// }

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
