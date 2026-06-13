#include "frame.h"
#include "syntax_tree.h"

extern int
check_assign_rule(const syntax_tree_t* tree, frame_t* frame, const int index);

extern int
check_bind_arg_rule(const syntax_tree_t* tree, frame_t* frame, const int index);

extern int
check_id_init_rule(const syntax_tree_t* tree, frame_t* frame, const int index);