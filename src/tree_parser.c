#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errormsg.h"
#include "lreng.h"

#define IS_PREFIXABLE(op) \
    (op == OP_FDEF || op == OP_LPAREN || op == OP_LBLOCK\
    || op == OP_POS || op == OP_NEG || op == OP_NOT\
    || op == OP_GETL || op == OP_GETR)

/* is the precedence o1 < o2 ? */
#define OP_PRECED_LT(o1, o2) \
    (OP_PRECEDENCES[o1] < OP_PRECEDENCES[o2] \
    || (OP_PRECEDENCES[o1] == OP_PRECEDENCES[o2] \
        && strchr(R_ASSOCIATIVE_OPS, o2) == NULL) \
    )

#define IS_UNARY(o) (strchr(UNARY_OPS, o) != NULL)

/* return expression tokens in postfix */
dynarr_t
shunting_yard(const dynarr_t tokens, const int is_debug) {
    enum EXPECT {
        PREFIXABLE, BINARY_OPERATOR
    };
    int expectation = PREFIXABLE;
    dynarr_t output = new_dynarr(sizeof(token_t));
    dynarr_t op_stack = new_dynarr(sizeof(token_t));

    if (is_debug) {
        puts("tree_parse");
    }
    int i, prev_output_count = 0;
    for (i = 0; i < tokens.size; i++) {
        token_t cur_token = ((token_t*) tokens.data)[i];
        if (is_debug) {
            printf("expect=%s token=", (expectation ? "BIN_OP" : "PREFIX"));
            print_token(cur_token);
            puts("");
        }
        /* is an operator or left bracket */
        if (cur_token.type == TOK_OP || cur_token.type == TOK_LB) {
            /* check expectation */
            if (IS_PREFIXABLE(cur_token.name)) {
                if (expectation == BINARY_OPERATOR) {
                    sprintf(
                        ERR_MSG_BUF,
                        "Expect binary operator or closing bracket. Get '%s'",
                        OP_STRS[cur_token.name]
                    );
                    throw_syntax_error(
                        cur_token.pos.line,
                        cur_token.pos.col,
                        ERR_MSG_BUF
                    );
                }
            }
            else {
                if (expectation == PREFIXABLE) {
                    sprintf(
                        ERR_MSG_BUF,
                        "Expect unary operator, number, identifier,"
                        "or open bracket. Get '%s'",
                        OP_STRS[cur_token.name]
                    );
                    throw_syntax_error(
                        cur_token.pos.line,
                        cur_token.pos.col,
                        ERR_MSG_BUF
                    );
                }
                expectation = PREFIXABLE;
            }

            /* positive sign do nothing, can discard */
            if (cur_token.name == OP_POS) {
                continue;
            }

            /* pop stack until top is left braclet or precedence is higher */
            token_t* stack_top = back(&op_stack);
            while (stack_top != NULL && stack_top->type != TOK_LB
                && OP_PRECED_LT(
                    stack_top->name, cur_token.name
                )) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = back(&op_stack);
            }
            /* push */
            append(&op_stack, &cur_token);
        }
        /* is a right bracket */
        else if (cur_token.type == TOK_RB) {
            /* check expectation */
            if (expectation == PREFIXABLE) {
                sprintf(
                    ERR_MSG_BUF,
                    "Expect unary operator, number, identifier,"
                    "or open bracket. Get '%s'",
                    cur_token.str
                );
                throw_syntax_error(
                    cur_token.pos.line,
                    cur_token.pos.col,
                    ERR_MSG_BUF
                );
            }
            expectation = BINARY_OPERATOR;

            /* pop stack until top is left braclet */
            token_t* stack_top = back(&op_stack);
            while (stack_top != NULL && stack_top->type != TOK_LB) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = back(&op_stack);
            }

            /* pop out the left bracket */
            pop(&op_stack);

            op_name_enum top_op = stack_top->name,
                cur_op = cur_token.name;
            if (stack_top == NULL) {
                throw_syntax_error(
                    cur_token.pos.line,
                    cur_token.pos.col,
                    "Unmatched closing bracket: Cannot find opening bracket"
                );
            }
            /* special operators */
            else if (top_op == OP_FCALL && cur_op == OP_RPAREN) {
                /* create function call */
                token_t fcall = {NULL, OP_FCALL, TOK_OP, stack_top->pos};
                append(&output, &fcall);
            }
            else if (top_op == OP_LBLOCK && cur_op == OP_RBLOCK) {
                /* function maker */
                token_t fdef = {NULL, OP_FDEF, TOK_OP, stack_top->pos};
                append(&output, &fdef);
            }
            else if (!(top_op == OP_LPAREN && cur_op == OP_RPAREN)) {
                sprintf(
                    ERR_MSG_BUF,
                    "Unmatched closing bracket: Found '%s' to '%s'",
                    OP_STRS[top_op], OP_STRS[cur_op]
                );
                throw_syntax_error(
                    cur_token.pos.line,
                    cur_token.pos.col,
                    ERR_MSG_BUF
                );
            }
        }
        /* is other */
        else {
            /* check expectation */
            if (expectation == BINARY_OPERATOR) {
                sprintf(
                    ERR_MSG_BUF,
                    "Expect binary operator or closing bracket. Get '%s'",
                    cur_token.str
                );
                throw_syntax_error(
                    cur_token.pos.line,
                    cur_token.pos.col,
                    ERR_MSG_BUF
                );
            }
            expectation = BINARY_OPERATOR;
            append(&output, &cur_token);
        }
        if (is_debug) {
            printf("op_stack=");
            int j;
            for (j = 0; j < op_stack.size; j++) {
                print_token(((token_t*) op_stack.data)[j]);
                printf(" ");
            }
            puts("");
            if (prev_output_count != output.size) {
                prev_output_count = output.size;
                printf("new_output=");
                print_token(*(token_t *) back(&output));
                puts("");
            }
        }
    } /* end for */

    /* pop stack to output until stack is empty */
    while (op_stack.size > 0) {
        append(&output, back(&op_stack));
        pop(&op_stack);
    }
    free_dynarr(&op_stack);

    if (is_debug) {
        printf("result=");
        for (i = 0; i < output.size; i++) {
            print_token(((token_t*) output.data)[i]);
            printf(" ");
        }
        puts("");
    }
    return output;
}

#define GET_TOKEN_AT(tree, i) ((token_t*) (tree.tokens.data))[i]

tree_t
tree_parser(const dynarr_t tokens, const int is_debug) {
    int i = 0;
    dynarr_t stack = new_dynarr(sizeof(int));
    tree_t tree = {
        .tokens = shunting_yard(tokens, is_debug),
        .lefts = NULL,
        .rights = NULL,
        .root_index = -1,
        .max_id_name = -1
    };
    tree.lefts = malloc(tree.tokens.size * sizeof(int));
    tree.rights = malloc(tree.tokens.size * sizeof(int));
    memset(tree.lefts, -1, tree.tokens.size * sizeof(int));
    memset(tree.rights, -1, tree.tokens.size * sizeof(int));
    for (i = tokens.size - 1; i >=0; i--) {
        token_t t = ((token_t*) tokens.data)[i];
        if (t.type != TOK_ID) {
            continue;
        }
        if (tree.max_id_name > t.name) {
            tree.max_id_name = t.name;
        }
    }

    for (i = 0; i < tree.tokens.size; i++) {
        token_t cur_token = GET_TOKEN_AT(tree, i);
        if (is_debug) {
            print_token(cur_token);
        }
        if (cur_token.type == TOK_OP) {
            int r_index = -1;
            if (!IS_UNARY(cur_token.name)) {
                r_index = *(int*) back(&stack);
                pop(&stack);
                tree.rights[i] = r_index;
            }
            if (stack.size == 0) {
                throw_syntax_error(
                    cur_token.pos.line,
                    cur_token.pos.col,
                    "Operator has too few operands"
                );
            }
            int l_index = *(int*) back(&stack);
            pop(&stack);
            tree.lefts[i] = l_index;
            append(&stack, &i);
            if (is_debug) {
                printf(" L=");
                print_token(GET_TOKEN_AT(tree, l_index));
                printf(" R=");
                if (r_index != -1) {
                    print_token(GET_TOKEN_AT(tree, r_index));
                }
            }
        }
        else {
            append(&stack, &i);
        }
        if (is_debug) {
            puts("");
        }
    }

    if (stack.size != 1) {
        if (is_debug) {
            int n;
            printf("REMAINED IN STACK:\n");
            for (n = 0; n < stack.size; n++) {
                print_token(
                    ((token_t*)tree.tokens.data)[((int *)stack.data)[n]]
                );
                puts("");
            }
        }
        sprintf(
            ERR_MSG_BUF,
            "Bad syntax somewhere: %d tokens left in stack when parsing tree",
            stack.size
        );
        throw_syntax_error(0, 0, ERR_MSG_BUF);
    }
    tree.root_index = ((int*) stack.data)[0];
    free_dynarr(&stack);
    if (is_debug) {
        printf("final_tree=\n");
        print_tree(&tree);
    }
    return tree;
}
