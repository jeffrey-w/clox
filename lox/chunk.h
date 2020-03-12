#pragma once

#include "common.h"
#include "value.h"

typedef enum {
	OP_CONSTANT,
	// TODO OP_CONSTANT_LONG
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_POP,
	// TODO OP_POP_N
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_GET_GLOBAL,
	OP_DEFINE_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_NEGATE,
	OP_PRINT,
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_LOOP,
	OP_CALL,
	OP_CLOSURE,
	OP_CLOSE_UPVALUE,
	OP_RETURN,
	OP_CLASS
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	int* lines; // TODO use run-length encoding
	ValueArray constants;
} Chunk;

void initChunk(Chunk*);
void freeChunk(Chunk*);
void writeChunk(Chunk*, uint8_t, int);
int addConstant(Chunk*, Value);
