#include "frame.h"
#include "token.h"
#include "tree.h"

typedef struct context {
    const tree_t* tree;
    frame_t* cur_frame;
    const int is_debug;
} context_t;

dynarr_t
tokenize(const char* src, const unsigned long src_len, const int is_debug);

tree_t tree_parse(const dynarr_t tokens, const int is_debug);

int check_semantic(const tree_t tree, const int is_debug);

object_t* eval(context_t context, const int entry_index);
