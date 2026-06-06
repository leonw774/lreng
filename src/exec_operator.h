#include "eval.h"

extern void exec_call(
    context_t context, linecol_t pos, const object_t* call, object_t* arg
);

extern void exec_map(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);

extern void exec_filter(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);

extern void exec_reduce(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);
