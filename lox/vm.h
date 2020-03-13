#pragma once

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
	ObjClosure* closure;
	uint8_t* ip;
	Value* slots;
} CallFrame;

typedef struct {
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	Value stack[STACK_MAX]; // TODO need to handle stack overflow
	Value* stackTop;
	Table globals;
	Table strings;
	ObjString* initString;
	ObjUpvalue* openUpvalues;
	size_t bytesAllocated;
	size_t nextGC;
	Obj* objects;
	int grayCount;
	int grayCapacity;
	Obj** grayStack;
} VM;

void initVM();
void freeVM();

VM vm;

InterpretResult interpret(const char*);
void push(Value);
Value pop();
