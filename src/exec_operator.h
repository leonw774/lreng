#include "eval.h"

extern void exec_call(
    context_t context, linecol_t pos, const object_t* call, object_t* arg
);

extern void exec_frame_set_from_pair(
    context_t context, linecol_t pos, const int assignee_index,
    const object_t* pair
);

#if DEPRECATED

    extern void exec_map(
        context_t context, linecol_t pos, const object_t* call, object_t* pair
    );

extern void exec_filter(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);

extern void exec_reduce(
    context_t context, linecol_t pos, const object_t* call, object_t* pair
);

#endif
