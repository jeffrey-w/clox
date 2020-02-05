#include "memory.h"
#include "object.h"

#define ALLOCATE_OBJ(type, objectType) \
	(type*)allocateObject(sizeof(type), objectType);

static Obj* allocateObject(size_t size, ObjType type);
static ObjString* allocateString(char* data, int length);

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
