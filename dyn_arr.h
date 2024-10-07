#include<stdlib.h>
#include<string.h>

typedef struct dyn_arr {
    void* data;
    int elem_size; 
    int count;
    int cap;
} dyn_arr;

dyn_arr
new_dyn_arr(int elem_size);

void
free_dyn_arr(dyn_arr* x);

void
reset_dyn_arr(dyn_arr* x);

void*
to_arr(dyn_arr* x, unsigned char is_str);

void
append(dyn_arr* x, void* elem);

void*
at(dyn_arr* x, int index);

int
concat(dyn_arr* x, dyn_arr* y);
