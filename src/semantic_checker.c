#include <stdio.h>
#include <stdlib.h>
#include "operators.h"
#include "errormsg.h"
#include "dynarr.h"
#include "tree.h"
#include "objects.h"
#include "frame.h"

int
semantic_checker(const tree_t tree, const unsigned char is_debug) {
    int is_passed = 1;
    unsigned char* id_usage = calloc(tree.max_id_name, sizeof(unsigned char));
    id_usage[RESERVED_ID_NULL] = 1;
    id_usage[RESERVED_ID_INPUT] = 1;
    id_usage[RESERVED_ID_OUTPUT] = 1;

    frame_t main_frame = DEFAULT_FRAME();
    frame_t* cur_frame_ptr = &main_frame;
    dynarr_t frame_stack = new_dynarr(sizeof(frame_t));
    object_t objnull = RESERVED_OBJS[RESERVED_ID_NULL];

    tree_infix_iterater_t tree_infix_iterater = tree_iter_init(tree);
    token_t* cur_tok_ptr = tree_iter_get(&tree_infix_iterater);
    int cur_depth = -1, cur_index = -1, cur_func_depth = -1;
    dynarr_t func_depth_stack = new_dynarr(sizeof(int));
    append(&func_depth_stack, &cur_depth);

    if (is_debug) {
        printf("check semantic\n");
    }
    while (cur_tok_ptr != NULL) {
        cur_index = *((int*) back(&tree_infix_iterater.index_stack));
        cur_depth = *((int*) back(&tree_infix_iterater.depth_stack));
        cur_func_depth = *((int*) back(&func_depth_stack));
        if (cur_tok_ptr->type == TOK_OP) {
            if (cur_depth <= cur_func_depth) {
                /* we left a function */
                pop(&func_depth_stack);
                frame_return(cur_frame_ptr);
            }
            /* check identifiers */
            if (cur_tok_ptr->name == OP_ASSIGN || cur_tok_ptr->name == OP_ARG) {
                token_t* left_token =
                    &((token_t*) tree.tokens.data)[tree.lefts[cur_index]];
                if (is_debug) {
                    printf(
                        "Line %d, col %d, checking identifier initialization: ",
                        cur_tok_ptr->pos.line,
                        cur_tok_ptr->pos.col
                    );
                    print_token(*left_token);
                    putchar(' ');
                    print_token(*cur_tok_ptr);
                    putchar('\n');
                }
                if (left_token->type != TOK_ID) {
                    is_passed = 0;
                    sprintf(
                        ERR_MSG_BUF,
                        "Left side of %s is not identifier.",
                        OP_STRS[cur_tok_ptr->name]
                    );
                    print_semantic_error(
                        left_token->pos.line,
                        left_token->pos.col,
                        ERR_MSG_BUF
                    );
                }
                else if (frame_find(cur_frame_ptr, left_token->name)) {
                    is_passed = 0;
                    sprintf(
                        ERR_MSG_BUF,
                        "Repeated initialization of identifier '%s'",
                        left_token->str
                    );
                    print_semantic_error(
                        left_token->pos.line,
                        left_token->pos.col,
                        ERR_MSG_BUF
                    );
                }
                else {
                    frame_set(cur_frame_ptr, left_token->name, &objnull);
                    id_usage[left_token->name] = 1;
                }
            }
            /* update frame */
            else if (cur_tok_ptr->name == OP_FDEF) {
                frame_t tmp_frame = new_frame(cur_frame_ptr, NULL, -1);
                append(&frame_stack, &tmp_frame);
                cur_frame_ptr = (frame_t*) back(&frame_stack);
                append(&func_depth_stack, &cur_depth);
            }
        }
        else if (cur_tok_ptr->type == TOK_ID) {
            if (is_debug) {
                printf(
                    "Line %d, col %d, checking identifier usage: ",
                    cur_tok_ptr->pos.line,
                    cur_tok_ptr->pos.col
                );
                print_token(*cur_tok_ptr);
                putchar('\n');
            }
            id_usage[cur_tok_ptr->name] = 1;
            /* check if this id is initialized in current frame */
            if (!frame_find(cur_frame_ptr, cur_tok_ptr->name)) {
                is_passed = 0;
                sprintf(
                    ERR_MSG_BUF,
                    "Identifier '%s' used without initialized in the scope",
                    cur_tok_ptr->str
                );
                print_semantic_error(
                    cur_tok_ptr->pos.line,
                    cur_tok_ptr->pos.col,
                    ERR_MSG_BUF
                );
            }
        }

        tree_iter_next(&tree_infix_iterater);
        cur_tok_ptr = tree_iter_get(&tree_infix_iterater);
    }
    return is_passed;
}