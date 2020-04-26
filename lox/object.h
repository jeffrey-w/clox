#pragma once

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)  
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value)         isObjType(value, OBJ_CLASS)
#define IS_BOUND_METHOD(value)  isObjType(value, OBJ_BOUND_METHOD)
#define IS_INSTANCE(value)      isObjType(value, OBJ_INSTANCE)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))        
#define AS_CSTRING(value)       (AS_STRING(value)->data)
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJ(value))->function)
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value)) 
#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))
#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJ(value))   

typedef enum {
	OBJ_STRING,
	OBJ_UPVALUE,
	OBJ_NATIVE,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_CLASS,
	OBJ_BOUND_METHOD,
	OBJ_INSTANCE
} ObjType;

struct sObj {
	ObjType type;
	bool isMarked;
	struct sObj* next;
};

struct sObjString { // TODO take 'const' strings from source
	Obj obj;
	int length;
	char* data; // TODO implement as flexible array member
	uint32_t hash;
};

typedef struct sObjUpvalue {
	Obj obj;
	Value* location;
	Value closed;
	struct sObjUpvalue* next;
} ObjUpvalue;

typedef Value (*NativeFn)(int, Value*);

typedef struct {
	Obj obj;
	NativeFn function;
} ObjNative;

typedef struct {
	Obj obj;
	int arity;
	int upvalueCount;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

typedef struct {
	Obj obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	int upvalueCount;
} ObjClosure;

typedef struct sObjClass {
	Obj obj;
	ObjString* name;
	Table methods;
} ObjClass;

typedef struct {
	Obj obj;
	Value receiver;
	ObjClosure* method;
} ObjBoundMethod;

typedef struct {
	Obj obj;
	ObjClass* cls;
	Table fields;
} ObjInstance;

void printObject(Value);
ObjString* copyString(const char*, int);
ObjString* takeString(char*, int);
ObjString* toString(Value);
ObjUpvalue* newUpvalue(Value*);
ObjNative* newNative(NativeFn);
ObjFunction* newFunction();
ObjClosure* newClosure(ObjFunction*);
ObjClass* newClass(ObjString*);
ObjBoundMethod* newBoundMethod(Value, ObjClosure*);
ObjInstance* newInstance(ObjClass*);

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
