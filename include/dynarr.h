#ifndef DYNARR_H
#define DYNARR_H

#define DYN_ARR_INIT_CAP 4

typedef struct dynarr {
    unsigned short elem_size; 
    unsigned short size;
    unsigned short cap;
    void* data;
} dynarr_t;

#define dynarr_size sizeof(dynarr_t)

#ifndef MEMCHECK_H

extern dynarr_t new_dynarr(int elem_size);

extern void free_dynarr(dynarr_t* x);

extern void reset_dynarr(dynarr_t* x);

extern dynarr_t copy_dynarr(const dynarr_t* x);

extern char* to_str(dynarr_t* x);

extern void append(dynarr_t* x, const void* const elem);

#endif

extern void pop(dynarr_t* x);

extern void* at(const dynarr_t* x, const unsigned int index);

extern void* back(dynarr_t* x);

extern int concat(dynarr_t* x, dynarr_t* y);

#endif
