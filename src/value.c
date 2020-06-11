#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "ryu/ryu.h"
#include "value.h"

static bool cmpNumber(Value, Value);

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
#ifdef NAN_BOXING
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return cmpNumber(a, b);
    }
    return a == b;
#else
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
	case VAL_BOOL:
		return AS_BOOL(a) == AS_BOOL(b);
	case VAL_NIL:
		return true;
	case VAL_NUMBER:
		return cmpNumber(a, b);
	case VAL_OBJ: {
		return AS_OBJ(a) == AS_OBJ(b);
	}
	default:s
		return false; // TODO need internal error logic
	}
#endif
}

bool cmpNumber(Value a, Value b) {
    if (isnan(AS_NUMBER(a))) {
        return false;
    }
    return fabs(AS_NUMBER(a) - AS_NUMBER(b)) < DBL_EPSILON;

void printValue(Value value) {
    if (IS_BOOL(value)) {                       
        printf(AS_BOOL(value) ? "true" : "false");
    }
    else if (IS_NIL(value)) {                 
        printf("nil");                            
    }
    else if (IS_NUMBER(value)) {              
        printf("%g", AS_NUMBER(value));           
    }
    else if (IS_OBJ(value)) {                 
        printObject(value);                       
    }
    else {
        return; // TODO need internal error logic
}

ObjString* valueToString(Value value) {
	ObjString* string = NULL;
    if (IS_BOOL(value)) {
        if (AS_BOOL(value)) {
            string = takeString("true", 4);
        }
        else {
            string = takeString("false", 5);
        }
    else if (IS_NIL(value)) {
        string = takeString("nil", 3);
    }
    else if (IS_NUMBER(value)) {
        uint32_t precision = 0;
        if (!isInteger(value)) {
            precision = DBL_DIG; // TODO find better method to calculate precision
        }
        char* data = d2fixed(AS_NUMBER(value), precision);
        string = takeString(data, strlen(data)); // TODO don't use strlen
    }
    else if (IS_OBJ(value)) {
        string = objectToString(value);
    }
    return string;
}

bool isInteger(Value value) {
	if (IS_NUMBER(value)) {
		double d = AS_NUMBER(value);
		if (isfinite(d)) {
			return floor(fabs(d)) == fabs(d);
		}
	}
	return false;
}
