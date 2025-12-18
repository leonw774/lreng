#include "objects.h"

#ifndef RESERVED_H
#define RESERVED_H

typedef enum reserved_id_name {
    RESERVED_ID_NAME_NULL,
    RESERVED_ID_NAME_INPUT,
    RESERVED_ID_NAME_OUTPUT,
    RESERVED_ID_NAME_ERROR,
    RESERVED_ID_NAME_IS_NUMBER,
    RESERVED_ID_NAME_IS_CALLABLE,
    RESERVED_ID_NAME_IS_PAIR
} reserved_id_name_enum;

#define RESERVED_ID_NUM RESERVED_ID_NAME_IS_PAIR + 1

extern const char* RESERVED_IDS[RESERVED_ID_NUM];

extern const object_t RESERVED_OBJS[RESERVED_ID_NUM];
extern const object_t* NULL_OBJECT_PTR;

#endif
