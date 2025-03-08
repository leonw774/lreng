#include "token.h"
#include "tree.h"
#include "frame.h"

dynarr_t tokenize(
    const char* src,
    const unsigned long src_len,
    const int is_debug
);

tree_t tree_parse(
    const dynarr_t tokens,
    const int is_debug
);

int semantic_checker(
    const tree_t tree,
    const int is_debug
);

object_t* eval(
    const tree_t* tree,
    frame_t* cur_frame,
    const int entry_index,
    const int is_debug
);
