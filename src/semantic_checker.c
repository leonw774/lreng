#include <stdio.h>
#include <stdlib.h>
#include "operators.h"
#include "errormsg.h"
#include "dynarr.h"
#include "tree.h"
#include "objects.h"
#include "frame.h"

int
semantic_checker(const tree_t tree, const const int is_debug) {
    int i, is_passed = 1;
    unsigned char* id_usage = calloc(
        tree.max_id_name + 1, sizeof(unsigned char));
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        id_usage[i] = (unsigned char) 1;
    }

    frame_t* cur_frame = new_frame(tree.root_index);
    object_t objnull = RESERVED_OBJS[RESERVED_ID_NAME_NULL];

    tree_preorder_iterator_t tree_iter = tree_iter_init(&tree, -1);
    token_t* cur_token_p = tree_iter_get(&tree_iter);
    int cur_depth = -1, cur_index = -1, cur_func_depth = -1;
    dynarr_t func_depth_stack = new_dynarr(sizeof(int));

    if (is_debug) {
        printf("check semantic\n");
    }
    append(&func_depth_stack, &cur_depth);
    while (cur_token_p != NULL) {
        cur_index = *((int*) back(&tree_iter.index_stack));
        cur_depth = *((int*) back(&tree_iter.depth_stack));
        cur_func_depth = *((int*) back(&func_depth_stack));
        if (cur_token_p->type == TOK_OP) {
            if (cur_depth <= cur_func_depth) {
                /* we left a function */
                pop(&func_depth_stack);
                pop_stack(cur_frame);
            }
            /* check identifiers */
            if (cur_token_p->name == OP_ASSIGN) {
                token_t* left_token =
                    &((token_t*) tree.tokens.data)[tree.lefts[cur_index]];
                if (is_debug) {
                    printf(
                        "Line %d, col %d, checking identifier initialization: ",
                        cur_token_p->pos.line,
                        cur_token_p->pos.col
                    );
                    print_token(*left_token);
                    putchar(' ');
                    print_token(*cur_token_p);
                    putchar('\n');
                }
                if (left_token->type != TOK_ID) {
                    is_passed = 0;
                    sprintf(
                        ERR_MSG_BUF,
                        "Left side of %s is not identifier.",
                        OP_STRS[cur_token_p->name]
                    );
                    print_semantic_error(
                        left_token->pos.line,
                        left_token->pos.col,
                        ERR_MSG_BUF
                    );
                }
                else if (frame_get(cur_frame, left_token->name)) {
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
                    frame_set(cur_frame, left_token->name, &objnull);
                    id_usage[left_token->name] = (unsigned char) 1;
                }
            }
            /* walk into a function */
            else if (cur_token_p->name == OP_FDEF) {
                push_stack(cur_frame, cur_index);
                append(&func_depth_stack, &cur_depth);
            }
        }
        else if (cur_token_p->type == TOK_ID) {
            if (is_debug) {
                printf(
                    "Line %d, col %d, checking identifier usage: ",
                    cur_token_p->pos.line,
                    cur_token_p->pos.col
                );
                print_token(*cur_token_p);
                putchar('\n');
            }
            id_usage[cur_token_p->name] = (unsigned char) 1;
        }

        tree_iter_next(&tree_iter);
        cur_token_p = tree_iter_get(&tree_iter);
        fflush(stdout);
    }

    free_tree_iter(&tree_iter);
    free_dynarr(&func_depth_stack);
    pop_stack(cur_frame);
    free_frame(cur_frame, 1);
    free(cur_frame);
    free(id_usage);
    return is_passed;
}
