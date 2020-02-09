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

static Obj* allocateObject(size_t size, ObjType type);
static ObjString* allocateString(char* data, int length, uint32_t hash);
static uint32_t hashString(const char* key, int length);

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
	default:
		break; // TODO need internal error logic
	}
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

Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	object->next = vm.objects;
	vm.objects = object;
	return object;
}

ObjString* allocateString(char* data, int length, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->data = data;
	string->hash = hash;
	tableSet(&vm.strings, string, NIL_VAL);
	return string;
}
