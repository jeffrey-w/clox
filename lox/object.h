#pragma once

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))         
#define AS_CSTRING(value)       (AS_STRING(value)->data)
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))

typedef enum {
	OBJ_STRING,
	OBJ_FUNCTION
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

typedef struct {
	Obj obj;
	int arity;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

void printObject(Value);
ObjString* copyString(const char*, int);
ObjString* takeString(char*, int);
ObjFunction* newFunction();

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
