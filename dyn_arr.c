#include "dyn_arr.h"

#define DYN_ARR_CAP_ALIGN 16 /* 4 bit alignment */

/* create a new empty dyn_arr */
dyn_arr
new_dyn_arr(int elem_size) {
    dyn_arr x;
    x.data = malloc(elem_size * 4);
    memset(x.data, 0, elem_size * 4);
    x.elem_size = elem_size;
    x.count = 0;
    x.cap = 4;
    return x;
};

void
free_dyn_arr(dyn_arr* x) {
    if (x->data != NULL && x->count != 0) {
        free(x->data);
        x->count = x->cap = 0;
    } 
};

void
reset_dyn_arr(dyn_arr* x) {
    free_dyn_arr(x);
    *x = new_dyn_arr(x->elem_size);
}

void*
/* create a simple arr */
to_arr(dyn_arr* x, unsigned char is_str) {
    if (x->data == NULL) return NULL;
    int count = is_str ? x->count + 1 : x->count;
    void* arr = malloc(x->elem_size * count);
    memcpy(arr, x->data, x->elem_size * count);
    return arr;
}

void
append(dyn_arr* x, void* elem) {
    if (x->data == NULL) return;
    if (x->count == x->cap - 1) { /* keep last zeros in case it is string */
        x->cap = (x->cap > DYN_ARR_CAP_ALIGN)
            ? (x->cap + DYN_ARR_CAP_ALIGN)
            : (x->cap * 2);
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = malloc(new_cap_byte_sz);
        memset(tmp_mem, 0, new_cap_byte_sz);
        memcpy(tmp_mem, x->data, x->elem_size * x->count);
        free(x->data);
        x->data = tmp_mem;
    }
    memcpy(x->data + x->count * x->elem_size, elem, x->elem_size);
    x->count += 1;
};

void*
at(dyn_arr* x, int index) {
    if (x->data == NULL || index >= x->cap) {
        return NULL;
    }
    return x->data + index * x->elem_size;
}

/* concat y onto x
   x and y's elem size must be the same
   return 1 if concat seccess, otherwise 0 */
int
concat(dyn_arr* x, dyn_arr* y) {
    if (x->data == NULL || y->data == NULL) {
        return 0;
    }
    if (x->elem_size == y->elem_size) {
        return 0;
    }
    if (x->count + y->count > x->cap - 1) {
        /* add 1 to add the last zeros in case it is string */
        x->cap = ((x->count + y->count) / DYN_ARR_CAP_ALIGN + 1)
            * DYN_ARR_CAP_ALIGN;
        int new_cap_byte_sz = x->elem_size * x->cap;
        void* tmp_mem = malloc(new_cap_byte_sz);
        memset(tmp_mem, 0, new_cap_byte_sz);
        memcpy(tmp_mem, x->data, new_cap_byte_sz);
        free(x->data);
        x->data = tmp_mem;
    }
    memcpy(
        x->data + x->count * x->elem_size,
        y->data,
        y->count * y->elem_size
    );
    x->count += y->count;
    return 1;
};