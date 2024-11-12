#include "token.h"
#include "tree.h"
#include "frame.h"

dynarr_t tokenize(
    const char* src,
    const int src_len,
    const unsigned char is_debug
);

tree_t tree_parser(
    const dynarr_t tokens,
    const unsigned char is_debug
);

int semantic_checker(
    const tree_t tree,
    const unsigned char is_debug
);

object_or_error_t eval_tree(
    tree_t* tree,
    const int entry_index,
    const frame_t* cur_frame,
    const unsigned char is_debug
);