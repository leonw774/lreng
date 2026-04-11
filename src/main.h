#include "syntax_tree.h"
#include "frame.h"

typedef struct context {
    const syntax_tree_t* tree;
    frame_t* cur_frame;
    int call_depth;
} context_t;

extern object_t* eval(context_t context, const int entry_index);

extern dynarr_token_t tokenize(const char* src, const unsigned long src_len);
