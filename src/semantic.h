#include "frame.h"
#include "syntax_tree.h"

int check_assign_rule(
    const syntax_tree_t* tree, frame_t* frame, const int index
);

int check_bind_arg_rule(
    const syntax_tree_t* tree, const int index
);