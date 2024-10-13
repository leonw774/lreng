#include "dynarr.h"
#include "objects.h"

typedef struct frame {
    struct frame* parent;
    dynarr_t objects;
    dynarr_t names;
    int size; /* number of name-object pairs */
} frame_t;

extern const frame_t DEFAULT_FRAME();

extern frame_t new_frame(frame_t* parent, object_t* init_obj, int* init_name);

extern frame_t frame_get_id(frame_t* f, int id);

extern frame_t frame_set_id(frame_t* f, int id, object_t* obj);
