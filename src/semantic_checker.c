#include "dynarr.h"
#include "errormsg.h"
#include "frame.h"
#include "lreng.h"
#include "objects.h"
#include "operators.h"
#include "reserved.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>

int
check_semantic(const tree_t tree)
{
    int i, is_passed = 1;
    unsigned char* id_usage
        = calloc(tree.max_id_name + 1, sizeof(unsigned char));
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        id_usage[i] = (unsigned char)1;
    }

    frame_t* cur_frame = frame_new();
    object_t objnull = RESERVED_OBJS[RESERVED_ID_NAME_NULL];

    tree_preorder_iterator_t tree_iter = tree_iter_init(&tree, -1);
    token_t* cur_token = tree_iter_get(&tree_iter);
    int cur_depth = -1, cur_index = -1, cur_func_depth = -1;
    dynarr_t func_depth_stack = dynarr_new(sizeof(int));
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("check semantic\n");
    }
#endif
    append(&func_depth_stack, &cur_depth);
    while (cur_token != NULL) {
        cur_index = *((int*)back(&tree_iter.index_stack));
        cur_depth = *((int*)back(&tree_iter.depth_stack));
        cur_func_depth = *((int*)back(&func_depth_stack));
        if (cur_token->type == TOK_OP) {
            if (cur_depth <= cur_func_depth) {
                /* we left a function */
                pop(&func_depth_stack);
                stack_pop(cur_frame);
            }
            /* check assign rule */
            if (cur_token->name == OP_ASSIGN) {
                token_t* left_token = at(&tree.tokens, tree.lefts[cur_index]);
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf(
                        "Line %d, col %d, checking identifier initialization: ",
                        cur_token->pos.line, cur_token->pos.col
                    );
                    token_print(left_token);
                    putchar(' ');
                    token_print(cur_token);
                    putchar('\n');
                }
#endif
                if (left_token->type != TOK_ID) {
                    const char* err_msg = "Left side of %s is not identifier.";
                    sprintf(ERR_MSG_BUF, err_msg, OP_STRS[cur_token->name]);
                    print_semantic_error(left_token->pos, ERR_MSG_BUF);
                    is_passed = 0;
                } else if (frame_get(cur_frame, left_token->name)) {
                    const char* err_msg
                        = "Repeated initialization of identifier '%s'";
                    sprintf(ERR_MSG_BUF, err_msg, left_token->str);
                    print_semantic_error(left_token->pos, ERR_MSG_BUF);
                    is_passed = 0;
                } else {
                    frame_set(cur_frame, left_token->name, &objnull);
                    id_usage[left_token->name] = (unsigned char)1;
                }
            }
            // check bind arguemt rule
            else if (cur_token->name == OP_ARG) {
                token_t* left_token = at(&tree.tokens, tree.lefts[cur_index]);
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf(
                        "Line %d, col %d, checking argument binder: ",
                        cur_token->pos.line, cur_token->pos.col
                    );
                    token_print(left_token);
                    putchar(' ');
                    token_print(cur_token);
                    putchar('\n');
                }
#endif
                if (left_token->type != TOK_ID) {
                    const char* err_msg = "Left side of %s is not identifier.";
                    sprintf(ERR_MSG_BUF, err_msg, OP_STRS[cur_token->name]);
                    print_semantic_error(left_token->pos, ERR_MSG_BUF);
                    is_passed = 0;
                }
            }
            /* walk into a function */
            else if (cur_token->name == OP_FMAKE) {
                stack_push(cur_frame, cur_index);
                append(&func_depth_stack, &cur_depth);
            }
        } else if (cur_token->type == TOK_ID) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(
                    "Line %d, col %d, checking identifier usage: ",
                    cur_token->pos.line, cur_token->pos.col
                );
                token_print(cur_token);
                putchar('\n');
                fflush(stdout);
            }
#endif
            id_usage[cur_token->name] = (unsigned char)1;
        }
        tree_iter_next(&tree_iter);
        cur_token = tree_iter_get(&tree_iter);
    }

    tree_iter_free(&tree_iter);
    dynarr_free(&func_depth_stack);
    frame_free(cur_frame);
    free(cur_frame);
    free(id_usage);
    return is_passed;
}
