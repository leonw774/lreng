#include "objects.h"
#include "syntax_tree.h"
#include <stdint.h>

#ifndef EVAL_H
#define EVAL_H

typedef struct context {
    const syntax_tree_t* tree;
    frame_t* cur_frame;
    dynarr_object_ptr_t* stack;
    int call_depth;
} context_t;

typedef struct registers {
    uint32_t arg; /* extendable argument */
    uint32_t insp; /* instruction pointer */
    object_t* reto; /* return object */
} registers_t;

#define TYPE registers_t
#define TYPE_NAME registers
#include "utils/dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

object_t* eval(context_t context, const int entry_index);

extern void eval_root(const syntax_tree_t* tree);

#endif
