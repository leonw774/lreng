#include "errormsg.h"
#include "frame.h"
#include "main.h"
#include "objects.h"
#include "operators.h"
#include "reserved.h"
#include "token_tree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int
check_semantic(const token_tree_t tree)
{
    int i, is_passed = 1;
    uint8_t* id_usage = calloc(tree.max_id_name + 1, sizeof(uint8_t));
    assert(id_usage != NULL);
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        id_usage[i] = (uint8_t)1;
    }

    frame_t* cur_frame = frame_new(NULL);
    object_t objnull = RESERVED_OBJS[RESERVED_ID_NAME_NULL];

    tree_preorder_iterator_t tree_iter = token_tree_iter_init(&tree, -1);
    token_t* cur_token = token_tree_iter_get(&tree_iter);
    int cur_depth = -1, cur_index = -1, cur_func_depth = -1;
    dynarr_int_t func_depth_stack = dynarr_int_new();
#ifdef ENABLE_DEBUG_LOG
    if (global_is_enable_debug_log) {
        printf("check semantic\n");
    }
#endif
    dynarr_int_append(&func_depth_stack, &cur_depth);
    while (cur_token != NULL) {
        cur_index = *dynarr_int_back(&tree_iter.index_stack);
        cur_depth = *dynarr_int_back(&tree_iter.depth_stack);
        cur_func_depth = *dynarr_int_back(&func_depth_stack);
        if (cur_token->type == TOK_OP) {
            /* if we left a function */
            if (cur_depth <= cur_func_depth) {
                dynarr_int_pop(&func_depth_stack);
                frame_return(cur_frame);
            }
            /* check assign rule */
            if (cur_token->name == OP_ASSIGN) {
                token_t* left_token
                    = dynarr_token_at(&tree.tokens, tree.lefts[cur_index]);
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf(
                        "Line %d, col %d, checking identifier initialization: ",
                        cur_token->pos.line, cur_token->pos.col
                    );
                    token_print(left_token);
                    printf(' ');
                    token_print(cur_token);
                    printf('\n');
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
                    id_usage[left_token->name] = (uint8_t)1;
                }
            }
            // check bind arguemt rule
            else if (cur_token->name == OP_ARG) {
                token_t* left_token
                    = dynarr_token_at(&tree.tokens, tree.lefts[cur_index]);
#ifdef ENABLE_DEBUG_LOG
                if (global_is_enable_debug_log) {
                    printf(
                        "Line %d, col %d, checking argument binder: ",
                        cur_token->pos.line, cur_token->pos.col
                    );
                    token_print(left_token);
                    printf(' ');
                    token_print(cur_token);
                    printf('\n');
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
                frame_call(cur_frame, cur_index);
                dynarr_int_append(&func_depth_stack, &cur_depth);
            }
        } else if (cur_token->type == TOK_ID) {
#ifdef ENABLE_DEBUG_LOG
            if (global_is_enable_debug_log) {
                printf(
                    "Line %d, col %d, checking identifier usage: ",
                    cur_token->pos.line, cur_token->pos.col
                );
                token_print(cur_token);
                printf('\n');
                fflush(stdout);
            }
#endif
            id_usage[cur_token->name] = (uint8_t)1;
        }
        token_tree_iter_next(&tree_iter);
        cur_token = token_tree_iter_get(&tree_iter);
    }

    token_tree_iter_free(&tree_iter);
    dynarr_int_free(&func_depth_stack);
    frame_free(cur_frame);
    free(cur_frame);
    free(id_usage);
    return is_passed;
}
