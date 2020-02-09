#pragma once

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
	Chunk* chunk;
	uint8_t* ip;
	Value stack[STACK_MAX]; // TODO need to handle stack overflow
	Value* stackTop;
	Table globals;
	Table strings;
	Obj* objects;
} VM;

void initVM();
void freeVM();

VM vm;

InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
