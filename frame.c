#include "token.h"
#include "frame.h"
#include "objects.h"

const frame_t DEFAULT_FRAME() {
    frame_t f;
    f.parent = NULL;
    f.objects = new_dynarr(sizeof(object_t));
    f.names = new_dynarr(sizeof(int));
    f.size = RESERVED_ID_NUM;
    int i;
    for (i = 0; i < RESERVED_ID_NUM; i++) {
        append(&f.objects, &RESERVED_OBJS[i]);
        append(&f.names, &i);
    }
    return f;
}

frame_t new_frame(frame_t* parent, object_t* init_obj, int* init_name);

frame_t frame_get_name(frame_t* f, int name);

frame_t frame_set_name(frame_t* f, int name, object_t* obj);
