#include "token.h"
#include "tree.h"

dynarr_t tokenize(const char* src, const int src_len, const unsigned char is_debug);

tree_t tree_parser(const dynarr_t tokens, const unsigned char is_debug);

int semantic_checker(const tree_t tree, const unsigned char is_debug);