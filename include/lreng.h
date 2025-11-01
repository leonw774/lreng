#include "frame.h"
#include "token.h"
#include "arena.h"
#include "token_tree.h"

#ifndef LRENG_H
#define LRENG_H

extern int global_is_enable_debug_log;

typedef struct context {
    const token_tree_t* tree;
    frame_t* cur_frame;
} context_t;

dynarr_t tokenize(const char* src, const unsigned long src_len);

token_tree_t tree_parse(const dynarr_t tokens);

int check_semantic(const token_tree_t tree);

object_t* eval(context_t context, const int entry_index);

#endif