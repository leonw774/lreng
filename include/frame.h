#include "dynarr.h"
#include "objects.h"

typedef struct frame {
    struct frame* parent;
    dynarr_t objects; /* type: object */
    dynarr_t names; /* type: int */
    int size; /* number of name-object pairs */
} frame_t;

extern const frame_t DEFAULT_FRAME();

extern frame_t new_frame(frame_t* parent, object_t* init_obj, int init_name);

extern object_t* frame_get(const frame_t* f, const int name);

extern object_t* frame_find(const frame_t* f, const int name);

extern void frame_set(frame_t* f, const int name, const object_t* obj);

extern void frame_return(frame_t* f);