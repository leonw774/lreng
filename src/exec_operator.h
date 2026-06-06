#include "eval.h"

extern object_t* exec_call(
    context_t context, linecol_t pos, const object_t* call, object_t* arg
);

extern object_t* exec_map(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);

extern object_t* exec_filter(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);

extern object_t* exec_reduce(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);
