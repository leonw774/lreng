#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errormsg.h"
#include "lreng.h"

#define IS_PREFIXER(op) ( \
    op == OP_LCURLY || op == OP_FMAKE || op == OP_LSQUARE || op == OP_MMAKE \
    || op == OP_LPAREN || op == OP_POS || op == OP_NEG || op == OP_NOT \
    || op == OP_GETL || op == OP_GETR \
)

#define EXPECT_PREFIX_MSG \
    "Expect unary operator, number, identifier, or open bracket. Get '%s'"
#define EXPECT_BINARY_MSG \
    "Expect binary operator or closing bracket. Get '%s'"

/* is the precedence o1 < o2 ? */
#define OP_PRECED_LT(o1, o2) \
    (OP_PRECEDENCES[o1] < OP_PRECEDENCES[o2] \
    || (OP_PRECEDENCES[o1] == OP_PRECEDENCES[o2] \
        && strchr(R_ASSOCIATIVE_OPS, o2) == NULL) \
    )

#define IS_UNARY(o)

/* return expression tokens in postfix */
dynarr_t
shunting_yard(const dynarr_t tokens, const int is_debug) {
    enum EXPECT {
        PREFIXER, INFIXER
        /* infixer include infix (binary ops) and postfix (closing bracket) */
    };
    int expectation = PREFIXER;
    dynarr_t output = new_dynarr(sizeof(token_t));
    dynarr_t op_stack = new_dynarr(sizeof(token_t));

#ifdef ENABLE_DEBUG
    if (is_debug) {
        puts("tree_parse");
    }
#endif
    int i, prev_output_count = 0;
    for (i = 0; i < tokens.size; i++) {
        token_t cur_token = ((token_t*) tokens.data)[i];
#ifdef ENABLE_DEBUG
        if (is_debug) {
            printf("expect=%s token=", (expectation ? "INFIX" : "PREFIX"));
            print_token(cur_token);
            puts("");
        }
#endif
        /* is an operator or left bracket */
        if (cur_token.type == TOK_OP || cur_token.type == TOK_LB) {
            /* check expectation */
            if (IS_PREFIXER(cur_token.name)) {
                if (expectation == INFIXER) {
                    sprintf(
                        ERR_MSG_BUF, EXPECT_BINARY_MSG, OP_STRS[cur_token.name]
                    );
                    throw_syntax_error(cur_token.pos, ERR_MSG_BUF);
                }
            }
            else {
                if (expectation == PREFIXER) {
                    sprintf(
                        ERR_MSG_BUF, EXPECT_PREFIX_MSG, OP_STRS[cur_token.name]
                    );
                    throw_syntax_error(cur_token.pos, ERR_MSG_BUF);
                }
                expectation = PREFIXER;
            }

            /* positive sign do nothing, can discard */
            if (cur_token.name == OP_POS) {
                continue;
            }

            /* pop stack until top is left bracket or precedence is not lower */
            token_t* frame_top = back(&op_stack);
            while (frame_top != NULL && frame_top->type != TOK_LB
                && OP_PRECED_LT(
                    frame_top->name, cur_token.name
                )) {
                append(&output, frame_top);
                pop(&op_stack);
                frame_top = back(&op_stack);
            }
            /* push */
            append(&op_stack, &cur_token);
        }
        /* is a right bracket */
        else if (cur_token.type == TOK_RB) {
            /* check expectation */
            if (expectation == PREFIXER) {
                sprintf(
                    ERR_MSG_BUF, EXPECT_PREFIX_MSG, OP_STRS[cur_token.name]
                );
                throw_syntax_error(cur_token.pos, ERR_MSG_BUF);
            }
            expectation = INFIXER;

            /* pop stack until top is left braclet */
            token_t* frame_top = back(&op_stack);
            while (frame_top != NULL && frame_top->type != TOK_LB) {
                append(&output, frame_top);
                pop(&op_stack);
                frame_top = back(&op_stack);
            }

            /* pop out the left bracket */
            pop(&op_stack);

            op_name_enum top_op = frame_top->name,
                cur_op = cur_token.name;
            if (frame_top == NULL) {
                throw_syntax_error(
                    cur_token.pos,
                    "Unmatched closing bracket: Cannot find opening bracket"
                );
            }
            /* special operators */
            else if (top_op == OP_FCALL && cur_op == OP_RPAREN) {
                /* create function call */
                token_t fcall = {NULL, OP_FCALL, TOK_OP, frame_top->pos};
                append(&output, &fcall);
            }
            else if (top_op == OP_LCURLY && cur_op == OP_RCURLY) {
                /* function maker */
                token_t fmake = {NULL, OP_FMAKE, TOK_OP, frame_top->pos};
                append(&output, &fmake);
            }
            else if (top_op == OP_LSQUARE && cur_op == OP_RSQUARE) {
                /* pure function maker */
                token_t pfmake = {NULL, OP_MMAKE, TOK_OP, frame_top->pos};
                append(&output, &pfmake);
            }
            else if (!(top_op == OP_LPAREN && cur_op == OP_RPAREN)) {
                sprintf(
                    ERR_MSG_BUF,
                    "Unmatched closing bracket: Found '%s' to '%s'",
                    OP_STRS[top_op], OP_STRS[cur_op]
                );
                throw_syntax_error(cur_token.pos, ERR_MSG_BUF);
            }
        }
        /* is other */
        else {
            /* check expectation */
            if (expectation == INFIXER) {
                sprintf(ERR_MSG_BUF, EXPECT_BINARY_MSG, cur_token.str);
                throw_syntax_error(cur_token.pos, ERR_MSG_BUF);
            }
            expectation = INFIXER;
            append(&output, &cur_token);
        }
#ifdef ENABLE_DEBUG
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
#endif
    } /* end for */

    /* pop stack to output until stack is empty */
    while (op_stack.size > 0) {
        append(&output, back(&op_stack));
        pop(&op_stack);
    }
    free_dynarr(&op_stack);
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("result=");
        for (i = 0; i < output.size; i++) {
            print_token(((token_t*) output.data)[i]);
            printf(" ");
        }
        puts("");
    }
#endif
    return output;
}

tree_t
tree_parser(const dynarr_t tokens, const int is_debug) {
    int i = 0;
    dynarr_t stack = new_dynarr(sizeof(int));
    /* make empty tree */
    tree_t tree = {
        .tokens = {.data = NULL, .cap = 0, .size = 0, .elem_size = 0},
        .lefts = NULL,
        .rights = NULL,
        .root_index = -1,
        .max_id_name = -1
    };

    /* tokens */
    tree.tokens = shunting_yard(tokens, is_debug);

    /* max_id_name */
    for (i = tokens.size - 1; i >= 0; i--) {
        token_t t = ((token_t*) tokens.data)[i];
        if (t.type != TOK_ID) {
            continue;
        }
        if (tree.max_id_name < t.name) {
            tree.max_id_name = t.name;
        }
    }

    /* lefts, rights and sizes */
    tree.lefts = malloc(tree.tokens.size * sizeof(int));
    tree.rights = malloc(tree.tokens.size * sizeof(int));
    tree.sizes = malloc(tree.tokens.size * sizeof(int));
    memset(tree.lefts, -1, tree.tokens.size * sizeof(int));
    memset(tree.rights, -1, tree.tokens.size * sizeof(int));
    memset(tree.sizes, 0, tree.tokens.size * sizeof(int));
    for (i = 0; i < tree.tokens.size; i++) {
        token_t* cur_token = at(&tree.tokens, i);
#ifdef ENABLE_DEBUG
        if (is_debug) {
            print_token(*cur_token);
        }
#endif
        if (cur_token->type == TOK_OP) {
            int l_index, r_index = -1;
            if (!IS_UNARY_OP[cur_token->name]) {
                r_index = *(int*) back(&stack);
                pop(&stack);
                tree.rights[i] = r_index;
            }
            if (stack.size == 0) {
                throw_syntax_error(
                    cur_token->pos,
                    "Operator has too few operands"
                );
            }
            l_index = *(int*) back(&stack);
            pop(&stack);
            tree.lefts[i] = l_index;
            append(&stack, &i);
#ifdef ENABLE_DEBUG
            if (is_debug) {
                printf(" L=");
                print_token(*(token_t*) at(&tree.tokens, l_index));
                printf(" R=");
                if (r_index != -1) {
                    print_token(*(token_t*) at(&tree.tokens, r_index));
                }
            }
#endif
        }
        else {
            append(&stack, &i);
        }

        tree.sizes[i] = (
            1
            + ((tree.lefts[i] == -1) ? 0 : tree.sizes[tree.lefts[i]])
            + ((tree.rights[i] == -1) ? 0 : tree.sizes[tree.rights[i]])
        );

#ifdef ENABLE_DEBUG
        if (is_debug) {
            putchar('\n');
        }
#endif
    }
    /* check result */
    if (stack.size != 1) {
#ifdef ENABLE_DEBUG
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
#endif
        sprintf(
            ERR_MSG_BUF,
            "Bad syntax somewhere: %d tokens left in stack when parsing tree",
            stack.size
        );
        throw_syntax_error((linecol_t) {0, 0} , ERR_MSG_BUF);
    }

    /* root_index */
    tree.root_index = ((int*) stack.data)[0];

    /* free things */
    free_dynarr(&stack);
#ifdef ENABLE_DEBUG
    if (is_debug) {
        printf("final_tree=\n");
        print_tree(&tree);
    }
#endif
    return tree;
}
