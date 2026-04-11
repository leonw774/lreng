#include "tree_parser.h"
#include "reserved.h"
#include "syntax_tree.h"
#include "utils/debug_flag.h"
#include "utils/errormsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pratt Parsing */

/**
 * The grammar in BNF
 * // nud: the leftmost token is single token
 * <nud> ::=
 *      | <identifier>
 *      | <number>
 *      | <unary_operator> <expression>
 *      | <left_bracket> <expression> <right_bracket>
 * // lud: the middle node is single token
 * <led> ::= <expression> <binary_operator> <expression>
 * // juxtaposition: no leaf
 * <juxta> ::= <expression> <expression>
 * // the final expression grammar
 * <expression> ::= <nud> | <led> | <juxta>
 */

int
pp_context_advance(pratt_parser_context_t* context)
{
    context->pos++;
    return context->pos;
}

token_t*
pp_context_peek(pratt_parser_context_t* context)
{
    if (context->pos < context->tokens.size) {
        return dynarr_token_at(&context->tokens, context->pos);
    }
    return NULL;
}

void
pp_context_push(pratt_parser_context_t* context, const token_t* token)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("pp_context_push: ");
        token_print(token);
        printf("\n");
    }
#endif
    dynarr_token_append(&context->output, (token_t*)token);
}

int
get_binding_power(const token_t* token)
{
    if (token != NULL) {
        return (token->type == TOK_OP)
            // binding power of op is reverse of precedence and smallest is 1
            ? TIER_COUNT - get_op_precedence(token->code)
            : 0;
    }
    return -1;
}

int
is_expression_starter(const token_t* token)
{
    /* the same allowing condition in nud */
    return token != NULL
        && ((token->type == TOK_OP && is_unary_op(token->code))
            || token->type == TOK_ID || token->type == TOK_NUM
            || token->type == TOK_LB);
}

void
nud(pratt_parser_context_t* context)
{
    token_t* token = pp_context_peek(context);
    int bp = get_binding_power(token);
    pp_context_advance(context);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(" nud: pos=%d cur_token=", context->pos);
        token_print(token);
        printf(", bp=%d\n", bp);
    }
#endif

    if (token->type == TOK_ID) {
        /* is identifier */
        pp_context_push(context, token);
    } else if (token->type == TOK_NUM) {
        /* is number */
        pp_context_push(context, token);
    } else if (token->type == TOK_OP) {
        /* is unary operator */
        if (!is_unary_op(token->code)) {
            sprintf(
                ERR_MSG_BUF,
                "Expect unary operator but get binary operator: '%s'",
                OP_NAMES[token->code]
            );
            throw_syntax_error(token->pos, ERR_MSG_BUF);
        }
        parse_expr(context, bp);
        pp_context_push(context, token);
    } else if (token->type == TOK_LB) {
        /* is left bracket */
        parse_expr(context, bp);
        token_t* closing = pp_context_peek(context);
        if (closing->type != TOK_RB) {
            throw_syntax_error(
                closing->pos,
                "Unmatched open bracket: Cannot find matching closing bracket"
            );
        }
        if (token->code == OP_LPAREN && closing->code == OP_RPAREN) {
            /* do nothing */
        } else if (token->code == OP_LSQUARE && closing->code == OP_RSQUARE) {
            /* macro maker */
            token_t mmake = { NULL, OP_MAKE_MACRO, TOK_OP, token->pos };
            pp_context_push(context, &mmake);
        } else if (token->code == OP_LCURLY && closing->code == OP_RCURLY) {
            /* function maker */
            token_t fmake = { NULL, OP_MAKE_FUNCT, TOK_OP, token->pos };
            pp_context_push(context, &fmake);
        } else {
            sprintf(
                ERR_MSG_BUF,
                "Unmatched open bracket: Cannot find matching closing bracket "
                "for '%s'",
                OP_NAMES[token->code]
            );
            throw_syntax_error(closing->pos, ERR_MSG_BUF);
        }
        pp_context_advance(context);
    } else {
        sprintf(
            ERR_MSG_BUF, "Unexpected token type at beginning of expression: %s",
            OP_NAMES[token->code]
        );
        throw_syntax_error(token->pos, ERR_MSG_BUF);
    }
}

void
led(pratt_parser_context_t* context, token_t* next)
{
    int bp = get_binding_power(next);

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf(" led: pos=%d, next=", context->pos);
        token_print(next);
        printf(", bp=%d\n", bp);
    }
#endif

    if (next->type == TOK_OP && !is_unary_op(next->code)) {
        /* is binary operator */
        if (is_right_associative_op(next->code)) {
            bp--;
        }
        parse_expr(context, bp);
        pp_context_push(context, next);
    }
}

/* build context.output tokens in postfix order */
void
parse_expr(pratt_parser_context_t* context, const int cur_binding_power)
{
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        int i;
        printf("parse_expr start: pos=%d, token_peek=", context->pos);
        token_print(pp_context_peek(context));
        printf(", cur_bp=%d\n", cur_binding_power);
        printf("outputs: ");
        for (i = 0; i < context->output.size; i++) {
            token_print(dynarr_token_at(&context->output, i));
            printf(" ");
        }
        printf("\n");
    }
#endif

    nud(context);

    {
        token_t* next = pp_context_peek(context);
        int next_binding_power = get_binding_power(next);
        token_t call_token = {
            /* the possible implicit OP_CALL token */
            NULL,
            OP_CALL,
            TOK_OP,
            (linecol_t) { .col = 0, .line = 0 },
        };

        while (1) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf("parse_expr loop: pos=%d, next=", context->pos);
                token_print(next);
                printf(", next_bp=%d\n", next_binding_power);
            }
#endif
            /**
             * for the case of juxtaposition, we need to check if the next token
             * is a expresion starting token. if true, set the next token to the
             * implicit OP_CALL
             */
            int is_implicit_call = is_expression_starter(next);
            if (is_implicit_call) {
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf("  implicit call\n");
                }
#endif
                call_token.pos = next->pos;
                next = &call_token;
                next_binding_power = get_binding_power(&call_token);
            }

            /* do led only if next token has greater binding power */
            if (next_binding_power <= cur_binding_power) {
                break;
            }

            if (!is_implicit_call) {
                pp_context_advance(context);
            }

            led(context, next);

            /* update next token */
            next = pp_context_peek(context);
            next_binding_power = get_binding_power(next);
        }
    }

#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("parse_expr end\n");
    }
#endif
}

/* Shunting Yard */

#define EXPECT_PREFIX_MSG                                                      \
    "Expect unary operator, number, identifier, or open bracket. Get '%s'"
#define EXPECT_INFIX_MSG "Expect binary operator or closing bracket. Get '%s'"

/* is the precedence o1 < o2 ? */
static inline unsigned char
op_prec_lt(op_code_enum o1, op_code_enum o2)
{
    /*
    printf(
        "op_prec_lt: %s %d  %s %d\n",
        OP_NAMES[o1], get_op_precedence(o1),
        OP_NAMES[o2], get_op_precedence(o2)
    );
    */
    return (
        get_op_precedence(o1) < get_op_precedence(o2)
        || (get_op_precedence(o1) == get_op_precedence(o2)
            && !is_right_associative_op(o2))
    );
}

void
insert_op(token_t* op_token, dynarr_token_t* op_stack, dynarr_token_t* output)
{
    token_t* stack_top = dynarr_token_back(op_stack);
    /* pop stack until top is left bracket or precedence is not lower */
    while (stack_top != NULL && stack_top->type != TOK_LB
           && op_prec_lt(stack_top->code, op_token->code)) {
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
            is_prefixer = is_prefixable_op(cur_token->code);
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
            token_t call_token = { NULL, OP_CALL, TOK_OP, cur_token->pos };
            insert_op(&call_token, &op_stack, &output);
            is_prefixer = 1;
            expectation = PREFIXER;
        }

        /* check expectation */
        if (is_prefixer && expectation == INFIXER) {
            sprintf(ERR_MSG_BUF, EXPECT_INFIX_MSG, OP_NAMES[cur_token->code]);
            throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
        }
        if (!is_prefixer && expectation == PREFIXER) {
            sprintf(ERR_MSG_BUF, EXPECT_PREFIX_MSG, OP_NAMES[cur_token->code]);
            throw_syntax_error(cur_token->pos, ERR_MSG_BUF);
        }

        if (cur_token->type == TOK_OP || cur_token->type == TOK_LB) {
            /* is an operator or left bracket */
            /* update expectation */
            if (!is_prefixer) {
                expectation = PREFIXER;
            }
            /* positive sign do nothing, can discard */
            if (cur_token->code == OP_POS) {
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
            if (stack_top->code == OP_LPAREN && cur_token->code == OP_RPAREN) {
                /* do nothing */
            } else if (
                stack_top->code == OP_LCURLY && cur_token->code == OP_RCURLY
            ) {
                /* function maker */
                token_t fmake = { NULL, OP_MAKE_FUNCT, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &fmake);
            } else if (
                stack_top->code == OP_LSQUARE && cur_token->code == OP_RSQUARE
            ) {
                /* macro maker */
                token_t mmake = { NULL, OP_MAKE_MACRO, TOK_OP, stack_top->pos };
                dynarr_token_append(&output, &mmake);
            } else {
                sprintf(
                    ERR_MSG_BUF,
                    "Unmatched closing bracket: Found '%s' to '%s'",
                    OP_NAMES[stack_top->code], OP_NAMES[cur_token->code]
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
