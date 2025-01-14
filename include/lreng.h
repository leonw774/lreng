#include "token.h"
#include "tree.h"
#include "frame.h"

dynarr_t tokenize(
    const char* src,
    const int src_len,
    const int is_debug
);

tree_t tree_parser(
    const dynarr_t tokens,
    const int is_debug
);

int semantic_checker(
    const tree_t tree,
    const int is_debug
);

object_or_error_t eval_tree(
    const tree_t* tree,
    frame_t* cur_frame,
    const int entry_index,
    const int is_debug
);
