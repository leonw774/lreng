#include "dynarr.h"
#include "frame.h"
#include "token_tree.h"

typedef struct context {
    const token_tree_t* tree;
    frame_t* cur_frame;
    int call_depth;
} context_t;

extern object_t* eval(context_t context, const int entry_index);

extern dynarr_t tokenize(const char* src, const unsigned long src_len);

extern token_tree_t parse_tokens_to_tree(const dynarr_t tokens);

extern int check_semantic(const token_tree_t tree);

extern int global_is_enable_debug_log;
