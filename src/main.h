#include "syntax_tree.h"
#include "frame.h"

typedef struct context {
    const syntax_tree_t* tree;
    frame_t* cur_frame;
    dynarr_object_ptr_t* stack;
    int call_depth;
} context_t;

extern void eval_root(const syntax_tree_t* tree);

extern dynarr_token_t tokenize(const char* src, const unsigned long src_len);
