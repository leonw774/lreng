/* template logic */

#ifndef TYPE
#error "TYPE is not defined"
#endif

#ifdef TYPE_NAME
#define _NAME TYPE_NAME
#else
#define _NAME TYPE
#endif

#define __RENDER(a, b) dynarr_ ## a ## b
#define RENDER(a, b) __RENDER(a, b)

#include <stdlib.h>
#include <string.h>

/* declaration */

#define DYNARR_INIT_CAP 2

typedef struct RENDER(_NAME, ) {
    int size;
    int cap;
    TYPE* data;
} RENDER(_NAME, _t);

#ifndef MEMCHECK_H

/* create a new empty dynamic array */
static inline RENDER(_NAME, _t)
RENDER(_NAME, _new)()
{
    RENDER(_NAME, _t) x;
    x.data = calloc(sizeof(TYPE), DYNARR_INIT_CAP);
    x.size = 0;
    x.cap = DYNARR_INIT_CAP;
    return x;
};

static inline void
RENDER(_NAME, _free)(RENDER(_NAME, _t) * x)
{
    if (x->data != NULL) {
        free(x->data);
        x->data = NULL;
    }
    x->size = x->cap = 0;
};

static inline void
RENDER(_NAME, _reset)(RENDER(_NAME, _t) * x)
{
    RENDER(_NAME, _free)(x);
    *x = RENDER(_NAME, _new)();
}

static inline RENDER(_NAME, _t)
RENDER(_NAME, _copy)(
    const RENDER(_NAME, _t) * x
)
{
    RENDER(_NAME, _t) y;
    y = *x;
    y.data = calloc(y.cap, sizeof(TYPE));
    memcpy(y.data, x->data, sizeof(TYPE) * x->size);
    return y;
}

/* copy the array as a C-string */
static inline char*
RENDER(_NAME, _to_str)(RENDER(_NAME, _t) * x)
{
    if (x->data == NULL) {
        return NULL;
    }
    char* arr;
    int arr_sz = sizeof(TYPE) * x->size;
    arr = malloc(arr_sz + 1);
    // if (arr == NULL) {
    //     return NULL;
    // }
    ((char*)arr)[arr_sz] = '\0';
    memcpy(arr, x->data, arr_sz);
    return arr;
}

static inline void
RENDER(_NAME, _append)(
    RENDER(_NAME, _t) * x, TYPE * elem
)
{
    if (x->data == NULL) {
        return;
    }
    if (x->size == x->cap) {
        x->cap *= 2;
        x->data = realloc(x->data, sizeof(TYPE) * x->cap);
    }
    x->data[x->size] = *elem;
    x->size++;
};

#endif

static inline void
RENDER(_NAME, _pop)(RENDER(_NAME, _t) * x)
{
    x->size--;
}

static inline TYPE*
RENDER(_NAME, _at)(
    const RENDER(_NAME, _t) * x, const unsigned int index
)
{
    return &x->data[index];
}

static inline TYPE*
RENDER(_NAME, _back)(const RENDER(_NAME, _t) * x)
{
    if (x->data == NULL) {
        return NULL;
    }
    if (x->size == 0) {
        return NULL;
    }
    return &x->data[x->size - 1];
}

/* concat y onto x
   x and y's elem size must be the same
   return 1 if concat success, otherwise 0 */
static inline int
RENDER(_NAME, _concat)(
    RENDER(_NAME, _t) * x, RENDER(_NAME, _t) * y
)
{
    if (x->data == NULL || y->data == NULL) {
        return 0;
    }
    if (x->size + y->size > x->cap) {
        x->cap = x->size + y->size;
        x->data = realloc(x->data, sizeof(TYPE) * x->cap);
    }
    if (y->size) {
        memcpy(
            &x->data[x->size],
            y->data,
            y->size * sizeof(TYPE)
        );
        x->size += y->size;
    }
    return 1;
};

#undef _NAME
