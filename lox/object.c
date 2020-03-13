#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define INITIAL_HASH 2166136261u
#define HASH_SCALE 16777619

#define ALLOCATE_OBJ(type, objectType) \
	(type*)allocateObject(sizeof(type), objectType);

static Obj* allocateObject(size_t, ObjType);
static char* printType(ObjType);
static void printFunction(ObjFunction*);
static uint32_t hashString(const char*, int);
static ObjString* allocateString(char*, int, uint32_t);

Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	object->isMarked = false;
	object->next = vm.objects;
	vm.objects = object;
#ifdef DEBUG_LOG_GC
	printf("%p allocate %ld bytes for %s\n", (void*)object, size, printType(type));
#endif // DEBUG_LOG_GC
	return object;
}

char* printType(ObjType type) {
	switch (type) {
	case OBJ_STRING:
		return "string";
	case OBJ_UPVALUE:
		return "upvalue";
	case OBJ_NATIVE:
		return "native";
	case OBJ_FUNCTION:
		return "function";
	case OBJ_CLOSURE:
		return "closure";
	case OBJ_CLASS:
		return "class";
	case OBJ_BOUND_METHOD:
		return "bound method";
	case OBJ_INSTANCE:
		return "instance";
	default:
		return "unknown object";
	}
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	//case OBJ_UPVALUE:
	//	printf("upvalue");
	//	break; TODO
	case OBJ_NATIVE:
		printf("<native fn>");
		break;
	case OBJ_FUNCTION:
		printFunction(AS_FUNCTION(value));
		break;
	case OBJ_CLOSURE:
		printFunction(AS_CLOSURE(value)->function);
		break;
	case OBJ_CLASS:
		printf("%s", AS_CLASS(value)->name->data);
		break;
	case OBJ_INSTANCE:
		printf("%s instance", AS_INSTANCE(value)->cls->name->data);
		break;
	default:
		break; // TODO need internal error logic
	}
}

void printFunction(ObjFunction* function) {
	if (!function->name) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->data);
}

ObjString* copyString(const char* string, int length) {
	uint32_t hash = hashString(string, length);
	ObjString* interned = tableFindString(&vm.strings, string, length, hash);
	if (interned) {
		return interned;
	}
	char* data = ALLOCATE(char, length + 1);
	memcpy(data, string, length);
	data[length] = '\0';
	return allocateString(data, length, hash);
}

ObjString* takeString(char* string, int length) {
	uint32_t hash = hashString(string, length);
	ObjString* interned = tableFindString(&vm.strings, string, length, hash);
	if (interned) {
		FREE_ARRAY(char, string, length + 1);
		return interned;
	}
	return allocateString(string, length, hash);
}


uint32_t hashString(const char* key, int length) {
	uint32_t hash = INITIAL_HASH;
	for (int i = 0; i < length; i++) {
		hash ^= key[i];
		hash *= HASH_SCALE;
	}
	return hash;
}

ObjString* allocateString(char* data, int length, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->data = data;
	string->hash = hash;
	push(OBJ_VAL(string));
	tableSet(&vm.strings, string, NIL_VAL);
	pop();
	return string;
}

ObjUpvalue* newUpvalue(Value* location) {
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->location = location;
	upvalue->closed = NIL_VAL;
	upvalue->next = NULL;
	return upvalue;
}

ObjNative* newNative(NativeFn function) {
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
	native->function = function;
	return native;
}

ObjFunction* newFunction() {
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

ObjClosure* newClosure(ObjFunction* function) {
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}
	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

ObjClass* newClass(ObjString* name) {
	ObjClass* cls = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
	cls->name = name;
	initTable(&cls->methods);
	return cls;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
	ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

ObjInstance* newInstance(ObjClass* cls) {
	ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
	instance->cls = cls;
	initTable(&instance->fields);
	return instance;
}
