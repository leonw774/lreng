#include "token.h"
#include "tree.h"

dyn_arr tokenize(const char* src, const int src_len, const unsigned char is_debug);

tree tree_parser(const dyn_arr tokens, const unsigned char is_debug);
