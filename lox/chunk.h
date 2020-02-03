#pragma once

#include "common.h"
#include "value.h"

typedef enum {
	OP_CONSTANT,
	// TODO OP_CONSTANT_LONG
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NEGATE,
	OP_RETURN
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	int* lines; // TODO use run-length encoding
	ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value byte);
