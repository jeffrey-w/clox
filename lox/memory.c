#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif // DEBUG_LOG_GC

static void markRoots();
static void traceReferences();
static void blackenObject(Obj*);
static void markArray(ValueArray*);
static void sweep();
static void freeObject(Obj*);

void* reallocate(void* previous, size_t oldSize, size_t newSize) {
	if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
		collectGarbage();
#endif // DEBUG_STRESS_GC
	}
	if (newSize == 0) {
		free(previous);
		return NULL;
	}
	return realloc(previous, newSize);
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif // DEBUG_LOG_GC
	markRoots();
	traceReferences();
	sweep();
#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
#endif // DEBUG_LOG_GC
}

void markRoots() {
	for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
		markValue(*slot);
	}
	for (int i = 0; i < vm.frameCount; i++) {
		markObject((Obj*)vm.frames[i].closure);
	}
	for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue; upvalue = upvalue->next) {
		markObject((Obj*)upvalue);
	}
	markTable(&vm.globals);
	markCompilerRoots();
}

void markValue(Value value) {
	if (!IS_OBJ(value)) {
		return;
	}
	markObject(AS_OBJ(value));
}

void markObject(Obj* object) {
	if (!object || object->isMarked) {
		return;
	}
#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif // DEBUG_LOG_GC
	object->isMarked = true;
	if (vm.grayCapacity < vm.grayCount + 1) {
		vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
		vm.grayStack = realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
	}
	vm.grayStack[vm.grayCount++] = object; // TODO do not add strings and natives to the gray stack
}

void traceReferences() {
	while (vm.grayCount > 0) {
		Obj* object = vm.grayStack[--vm.grayCount];
		blackenObject(object);
	}
}

void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif // DEBUG_LOG_GC
	switch (object->type) {
	case OBJ_STRING:
	case OBJ_NATIVE:
		break;
	case OBJ_UPVALUE:
		markValue(((ObjUpvalue*)object)->closed);
		break;
	case OBJ_FUNCTION: {
		ObjFunction* function = (ObjFunction*)object;
		markObject((Obj*)function->name);
		markArray(&function->chunk.constants);
		break;
	}
	case OBJ_CLOSURE: {
		ObjClosure* closure = (ObjClosure*)object;
		markObject((Obj*)closure->function);
		for (int i = 0; i < closure->upvalueCount; i++) {
			markObject((Obj*)closure->upvalues[i]);
		}
		break;
	}
	}
}

void markArray(ValueArray* array) {
	for (int i = 0; i < array->count; i++) {
		markValue(array->values[i]);
	}
}

void sweep() {
	Obj* previous = NULL;
	Obj* object = vm.objects;
	while (object) {
		if (object->isMarked) {
			object->isMarked = false;
			previous = object;
			object = object->next;
		}
		else {
			Obj* unreached = object;
			object = object->next;
			if (previous) {
				previous->next = object;
			}
			else {
				vm.objects = object;
			}
			freeObject(unreached);
		}
	}
}

void freeObjects() {
	Obj* object = vm.objects;
	while (object != NULL) {
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}
	free(vm.grayStack);
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
	case OBJ_CLOSURE: {
		ObjClosure* closure = (ObjClosure*)object;
		FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
		FREE(ObjClosure, object);
	}
	default:
		break; // TODO need internal error logic
	}
}
