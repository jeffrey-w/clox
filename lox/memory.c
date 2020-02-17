#include <stdlib.h>

#include "common.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

static void freeObject(Obj*);

void* reallocate(void* previous, size_t oldSize, size_t newSize) {
	if (newSize == 0) {
		free(previous);
		return NULL;
	}
	return realloc(previous, newSize);
}

void freeObjects() {
	Obj* object = vm.objects;
	while (object != NULL) {
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}
}

void freeObject(Obj* object) {
	switch (object->type) {
	case OBJ_STRING: {
		ObjString* string = (ObjString*)object;
		FREE_ARRAY(char, string->data, string->length + 1);
		FREE(ObjString, object);
		break;
	}
	case OBJ_UPVALUE:
		FREE(ObjUpvalue, object);
		break;
	case OBJ_NATIVE:
		FREE(ObjNative, object);
		break;
	case OBJ_FUNCTION: {
		ObjFunction* function = (ObjFunction*)object;
		freeChunk(&function->chunk);
		FREE(ObjFunction, object);
		break;
	}
	case OBJ_CLOSURE:
		FREE(ObjClosure, object);
	default:
		break; // TODO need internal error logic
	}
}
