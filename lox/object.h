#pragma once

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)

typedef enum {
	OBJ_STRING
} ObjType;

struct sObj {
	ObjType type;
};

struct sObjString {
	Obj obj;
	int length;
	char* data;
};

static inline bool isObjectType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
