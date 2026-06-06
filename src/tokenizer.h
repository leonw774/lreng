#include "syntax_tree.h"

#ifndef TOKENIZER_H
#define TOKENIZER_H

extern dynarr_token_t tokenize(const char* src, const unsigned long src_len);

#endif