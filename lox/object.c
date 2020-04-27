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
static ObjString* functionToString(ObjFunction*);

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
	case OBJ_BOUND_METHOD:
		printFunction(AS_BOUND_METHOD(value)->method->function);
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

ObjString* toString(Value value) {
	ObjString* string = NULL;
	switch (OBJ_TYPE(value)) {
	case OBJ_STRING:
		string = AS_STRING(value);
		break;
	case OBJ_NATIVE:
		string = takeString("<native fn>", 11);
		break;
	case OBJ_FUNCTION:
	case OBJ_CLOSURE:
	case OBJ_BOUND_METHOD: {
		ObjFunction * function;
		if (IS_FUNCTION(value)) {
			function = AS_FUNCTION(value);
		}
		else if (IS_CLOSURE(value)) {
			function = AS_CLOSURE(value)->function;
		}
		else {
			function = AS_BOUND_METHOD(value)->method->function;
		}
		string = functionToString(function);
		break;
	}
	case OBJ_CLASS:
		string = AS_CLASS(value)->name;
		break;
	case OBJ_INSTANCE: {
		ObjInstance* instance = AS_INSTANCE(value);
		int length = instance->cls->name->length + 9;
		char* data = ALLOCATE(char, length + 1);
		memcpy(data, instance->cls->name->data, length - 9);
		memcpy(data + length - 9, " instance", 9);
		data[length] = '\0';
		string = takeString(data, length);
		break;
	}
	default:
		break; // TODO need internal error logic
	}
	return string;
}

ObjString* functionToString(ObjFunction* function) {
	ObjString* string = NULL;
	if (!function->name) {
		string = takeString("<script>", 8);
	}
	else {
		int length = function->name->length + 5;
		char* data = ALLOCATE(char, length + 1);
		memcpy(data, "<fn ", 4);
		memcpy(data + 4, function->name->data, length - 1);
		memcpy(data + length - 1, ">", 1);
		data[length] = '\0';
		string = takeString(data, length);
	}
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
