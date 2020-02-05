#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
	(type*)allocateObject(sizeof(type), objectType);

static Obj* allocateObject(size_t size, ObjType type);
static ObjString* allocateString(char* data, int length);

void printObject(Value value) {
	switch (value.type) {
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
	default:
		break; // TODO need internal error logic
	}
}

ObjString* copyString(const char* string, int length) {
	char* data = ALLOCATE(char, length + 1);
	memcpy(data, string, length);
	data[length] = '\0';
	return allocateString(data, length);
}

Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	return object;
}

ObjString* allocateString(char* data, int length) {
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->data = data;
	return string;
}
