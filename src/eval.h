#include "objects.h"
#include "frame.h"
#include "syntax_tree.h"
#include <stdint.h>

#ifndef EVAL_H
#define EVAL_H

typedef struct registers {
    uint32_t arg; /* extendable argument */
    uint32_t insp; /* instruction pointer */
    uint16_t errf; /* error flag */
} registers_t;

#define TYPE registers_t
#define TYPE_NAME registers
#include "utils/dynarr.tmpl.h"
#undef TYPE_NAME
#undef TYPE

typedef struct context {
    const syntax_tree_t* tree;
    dynarr_frame_t* frame_stack;
    dynarr_registers_t* regs_stack;
    dynarr_object_ptr_t* object_stack;
} context_t;

void eval(context_t context);

extern void eval_root(const syntax_tree_t* tree);

#endif
