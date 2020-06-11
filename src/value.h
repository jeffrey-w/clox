#pragma once

#include "common.h"

typedef struct sObj Obj;
typedef struct sObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT          ((uint64_t)0x8000000000000000)
#define QNAN              ((uint64_t)0x7ffc000000000000)

#define TAG_NIL           1
#define TAG_FALSE         2
#define TAG_TRUE          3

#define IS_BOOL(value)    (((value) & FALSE_VAL) == FALSE_VAL)
#define IS_NIL(value)     ((value) == NIL_VAL)
#define IS_NUMBER(value)  (((value) & QNAN) != QNAN)
#define IS_OBJ(value)     (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))
#define AS_BOOL(value)    ((value) == TRUE_VAL)
#define AS_NUMBER(value)  valueToNum(value)
#define AS_OBJ(value)     ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN))
#define BOOL_VAL(value)   ((value) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL         ((Value)(uint64_t)(QNAN | FALSE_TAG)
#define TRUE_VAL          ((Value)(uint64_t)(QNAN | TRUE_TAG)
#define NIL_VAL           ((Value)(uint64_t)(QNAN | TAG_NIL)
#define NUMBER_VAL(value) numToValue(value)
#define OBJ_VAL(object)   ((Value)(SIGN_BIT | QNAN | (uint64_t)(object)))

typedef uint64_t Value;

typedef union {
    uint64_t bits;
    double num;
} DoubleUnion;

static inline double valueToNum(Value value) {
    DoubleUnion data;
    data.bits = value;
    return data.num;
}

static inline Value numToValue(double num) {
    DoubleUnion data;
    data.num = num;
    return data.bits;
}

#else

#define IS_BOOL(value)    ((value).type == VAL_BOOL)  
#define IS_NIL(value)     ((value).type == VAL_NIL)   
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)   
#define AS_BOOL(value)    ((value).as.boolean)                       
#define AS_NUMBER(value)  ((value).as.number)  
#define AS_OBJ(value)     ((value).as.obj)
#define BOOL_VAL(value)   ((Value){ VAL_BOOL, { .boolean = value } }) 
#define NIL_VAL           ((Value){ VAL_NIL, { .number = 0 } })       
#define NUMBER_VAL(value) ((Value){ VAL_NUMBER, { .number = value } })
#define OBJ_VAL(object)   ((Value){ VAL_OBJ, { .obj = (Obj*)object } })

typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ
} ValueType;

typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;
} Value;

#endif

typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;

void initValueArray(ValueArray*);
void freeValueArray(ValueArray*);
void writeValueArray(ValueArray*, Value);
bool valuesEqual(Value, Value);
void printValue(Value);
ObjString* valueToString(Value);
bool isInteger(Value);
