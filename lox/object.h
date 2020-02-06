#pragma once

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))         
#define AS_CSTRING(value)       (AS_STRING(value)->data)

typedef enum {
	OBJ_STRING
} ObjType;

struct sObj {
	ObjType type;
	struct sObj* next;
};

struct sObjString { // TODO take 'const' strings from source
	Obj obj;
	int length;
	char* data; // TODO implement as flexible array member
	uint32_t hash;
};

void printObject(Value value);
ObjString* copyString(const char* string, int length);
ObjString* takeString(char* string, int length);

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
