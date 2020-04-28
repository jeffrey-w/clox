#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "ryu/ryu.h"
#include "value.h"

void initValueArray(ValueArray* array) {
	array->capacity = 0;
	array->count = 0;
	array->values = NULL;
}

void freeValueArray(ValueArray* array) {
	FREE_ARRAY(Value, array->values, array->capacity);
	initValueArray(array);
}

void writeValueArray(ValueArray* array, Value value) {
	if (array->capacity < array->count + 1) {
		int oldCapacity = array->capacity;
		array->capacity = GROW_CAPACITY(oldCapacity);
		array->values = GROW_ARRAY(array->values, Value, oldCapacity, array->capacity);
	}
	array->values[array->count++] = value;
}

bool valuesEqual(Value a, Value b) {
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
	case VAL_BOOL:
		return AS_BOOL(a) == AS_BOOL(b);
	case VAL_NIL:
		return true;
	case VAL_NUMBER:
		return AS_NUMBER(a) == AS_NUMBER(b); // TODO double comparison
	case VAL_OBJ: {
		return AS_OBJ(a) == AS_OBJ(b);
	}
	default:
		return false; // TODO need internal error logic
	}
}

void printValue(Value value) {
	switch (value.type) {
	case VAL_BOOL:
		printf(AS_BOOL(value) ? "true" : "false");
		break;
	case VAL_NIL:
		printf("nil");
		break;
	case VAL_NUMBER:
		printf("%g", AS_NUMBER(value));
		break;
	case VAL_OBJ:
		printObject(value);
		break;
	default:
		return; // TODO need internal error logic
	}
}

ObjString* valueToString(Value value) {
	ObjString* string = NULL;
	switch (value.type) {
	case VAL_BOOL:
		if (AS_BOOL(value)) {
			string = takeString("true", 4);
		}
		else {
			string = takeString("false", 5);
		}
		break;
	case VAL_NIL:
		string = takeString("nil", 3);
		break;
	case VAL_NUMBER: {
		char* data = d2s(AS_NUMBER(value));
		string = takeString(data, strlen(data)); // TODO don't use strlen
		break;
	}
	case VAL_OBJ: {
		string = objectToString(value);
		break;
	}
	default:
		break; // TODO need internal error logic
	}
	return string;
}
