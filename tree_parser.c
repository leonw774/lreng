#include "errors.h"
#include "lreng.h"

#define IS_PREFIXABLE(op) \
    (op == OP_FDEF || op == OP_LPAREN || op == OP_LBLOCK\
    || op == OP_POS || op == OP_NEG || op == OP_NOT\
    || op == OP_GETL || op == OP_GETR)

/* is the precedence o1 < o2 ? */
#define OP_PRECED_LT(o1, o2) \
    (OP_PRECEDENCES[o1] < OP_PRECEDENCES[o2] \
    || (OP_PRECEDENCES[o1] == OP_PRECEDENCES[o2] \
        && strchr(R_ASSOCIATIVE, o2) == NULL) \
    )


/* return expression tokens in postfix */
dyn_arr
shunting_yard(const dyn_arr tokens, const unsigned char is_debug) {
    enum EXPECT {
        PREFIXABLE, BINARY_OPERATOR
    };
    int expectation = PREFIXABLE;
    dyn_arr output = new_dyn_arr(sizeof(token));
    dyn_arr op_stack = new_dyn_arr(sizeof(token));

    if (is_debug) {
        puts("tree_parse");
    }
    int i, prev_output_count = 0;
    for (i = 0; i < tokens.count; i++) {
        token cur_token = ((token*) tokens.data)[i];
        if (is_debug) {
            printf("expect=%s token=", (expectation ? "BIN_OP" : "PREFIX"));
            print_token(cur_token);
            puts("");
        }
        // is an operator or left bracket
        if (cur_token.type == TOK_OP || cur_token.type == TOK_LB) {
            // check expectation
            if (IS_PREFIXABLE(cur_token.payload.op)) {
                if (expectation == BINARY_OPERATOR) {
                    sprintf(
                        SYNTAX_ERR_MSG,
                        "Expect binary operator or closing bracket. Get '%s'",
                        OP_STRS[cur_token.payload.op]
                    );
                    throw_syntax_error(
                        cur_token.db_info.line,
                        cur_token.db_info.col,
                        SYNTAX_ERR_MSG
                    );
                }
            }
            else {
                if (expectation == PREFIXABLE) {
                    sprintf(
                        SYNTAX_ERR_MSG,
                        "Expect unary operator, number, identifier,"
                        "or open bracket. Get '%s'",
                        OP_STRS[cur_token.payload.op]
                    );
                    throw_syntax_error(
                        cur_token.db_info.line,
                        cur_token.db_info.col,
                        SYNTAX_ERR_MSG
                    );
                }
                expectation = PREFIXABLE;
            }

            /* pop stack until top is left braclet or precedence is higher */
            token* stack_top = (token*) back(&op_stack);
            while (stack_top != NULL && stack_top->type != TOK_LB
                && OP_PRECED_LT(
                    stack_top->payload.op, cur_token.payload.op
                )) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = (token*) back(&op_stack);
            }
            /* push */
            append(&op_stack, &cur_token);
        }
        // is a right bracket
        else if (cur_token.type == TOK_RB) {
            /* check expectation */
            if (expectation == PREFIXABLE) {
                sprintf(
                    SYNTAX_ERR_MSG,
                    "Expect unary operator, number, identifier,"
                    "or open bracket. Get '%s'",
                    cur_token.payload.str
                );
                throw_syntax_error(
                    cur_token.db_info.line,
                    cur_token.db_info.col,
                    SYNTAX_ERR_MSG
                );
            }
            expectation = BINARY_OPERATOR;

            /* pop stack until top is left braclet */
            token* stack_top = (token*) back(&op_stack);
            while (stack_top != NULL && stack_top->type != TOK_LB) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = (token*) back(&op_stack);
            }

            /* pop out the left bracket */
            pop(&op_stack);

            op_enum top_op = stack_top->payload.op,
                cur_op = cur_token.payload.op;
            if (stack_top == NULL) {
                throw_syntax_error(
                    cur_token.db_info.line,
                    cur_token.db_info.col,
                    "Unmatched closing bracket: Cannot find opening bracket"
                );
            }
            /* special operators */
            else if (top_op == OP_FCALL && cur_op == OP_RPAREN) {
                /* function call */
                append(
                    &output,
                    &(token) {{.op = OP_FCALL}, TOK_OP, stack_top->db_info}
                );
            }
            else if (top_op == OP_LBLOCK && cur_op == OP_RBLOCK) {
                /* function maker */
                append(
                    &output,
                    &(token) {{.op = OP_FDEF}, TOK_OP, stack_top->db_info}
                );
            }
            else if (!(top_op == OP_LPAREN && cur_op == OP_RPAREN)) {
                sprintf(
                    SYNTAX_ERR_MSG,
                    "Unmatched closing bracket: Found '%s' to '%s'",
                    OP_STRS[top_op], OP_STRS[cur_op]
                );
                throw_syntax_error(
                    cur_token.db_info.line,
                    cur_token.db_info.col,
                    SYNTAX_ERR_MSG
                );
            }
        }
        // is other
        else {
            // check expectation
            if (expectation == BINARY_OPERATOR) {
                sprintf(
                    SYNTAX_ERR_MSG,
                    "Expect binary operator or closing bracket. Get '%s'",
                    cur_token.payload.str
                );
                throw_syntax_error(
                    cur_token.db_info.line,
                    cur_token.db_info.col,
                    SYNTAX_ERR_MSG
                );
            }
            expectation = BINARY_OPERATOR;
            append(&output, &cur_token);
        }
        if (is_debug) {
            printf("op_stack=");
            int j;
            for (j = 0; j < op_stack.count; j++) {
                print_token(((token*) op_stack.data)[j]);
                printf(" ");
            }
            puts("");
            if (prev_output_count != output.count) {
                prev_output_count = output.count;
                printf("new_output=");
                print_token(*(token *) back(&output));
                puts("");
            }
        }
    }
    // end for
    if (is_debug) {
        printf("output=");
        for (i = 0; i < output.count; i++) {
            print_token(((token*) output.data)[i]);
            printf(" ");
        }
        puts("");
    }
    return output;
}

tree
tree_parser(const dyn_arr tokens, const unsigned char is_debug) {
    dyn_arr postfix_tokens = shunting_yard(tokens, is_debug);
    dyn_arr stack = new_dyn_arr(sizeof(tree));

    /* TODO: build tree from postfix */

    if (stack.count != 1) {
        sprintf(
            SYNTAX_ERR_MSG,
            "Bad syntax somewhere: %d nodes left in stack when parsing tree",
            stack.count
        );
        throw_syntax_error(0, 0, SYNTAX_ERR_MSG);
    }
    tree root = *((tree*) stack.data);
    return root;
}
