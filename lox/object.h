#pragma once

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)

typedef enum {
	OBJ_STRING
} ObjType;

struct sObj {
	ObjType type;
};
