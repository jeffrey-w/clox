#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static InterpretResult run();
static void resetStack();
static Value peek(int distance);
static void concatenate();
static bool isFalsey(Value value);
static void runtimeError(const char* format, ...);

void initVM() {
	resetStack();
	initTable(&vm.globals);
	initTable(&vm.strings);
	vm.objects = NULL;
}

void resetStack() {
	vm.stackTop = vm.stack;
}

void freeVM() {
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	freeObjects();
}

InterpretResult interpret(const char* source) {
	Chunk chunk;
	initChunk(&chunk);
	if (!compile(source, &chunk)) {
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}
	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;
	InterpretResult result = run();
	freeChunk(&chunk);
	return result;
}

InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)
	while (true) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("          ");
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif // DEBUG_TRACE_EXECUTION
		uint8_t instruction;
		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT: {
			Value constant = READ_CONSTANT();
			push(constant);
			break;
		}
		case OP_NIL:
			push(NIL_VAL);
			break;
		case OP_TRUE:
			push(BOOL_VAL(true));
			break;
		case OP_FALSE:
			push(BOOL_VAL(false));
			break;
		case OP_POP:
			pop();
			break;
		case OP_GET_GLOBAL: {
			ObjString* name = READ_STRING();
			Value value;
			if (!tableGet(&vm.globals, name, &value)) {
				runtimeError("Undefined variable '%s'.", name->data);
				return INTERPRET_RUNTIME_ERROR;
			}
			push(value);
			break;
		}
		case OP_DEFINE_GLOBAL: {
			ObjString* name = READ_STRING();
			tableSet(&vm.globals, name, peek(0));
			pop();
			break;
		}
		case OP_EQUAL: {
			Value b = pop();
			Value a = pop();
			push(BOOL_VAL(valuesEqual(a, b)));
			break;
		}
		case OP_GREATER:
			BINARY_OP(BOOL_VAL, > );
			break;
		case OP_LESS:
			BINARY_OP(BOOL_VAL, < );
			break;
		case OP_ADD:
			if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
				concatenate();
			}
			else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());
				push(NUMBER_VAL(a + b));
			}
			else { // TODO if one argument is a string, stringify and concatenate the other
				runtimeError("Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		case OP_SUBTRACT:
			BINARY_OP(NUMBER_VAL, -);
			break;
		case OP_MULTIPLY:
			BINARY_OP(NUMBER_VAL, *);
			break;
		case OP_DIVIDE:
			BINARY_OP(NUMBER_VAL, /);
			break;
		case OP_NOT:
			push(BOOL_VAL(isFalsey(pop())));
			break;
		case OP_NEGATE:
			if (!IS_NUMBER(peek(0))) {
				runtimeError("Operand must be a number.");
				return INTERPRET_RUNTIME_ERROR;
			}
			push(NUMBER_VAL(-AS_NUMBER(pop())));
			break;
		case OP_PRINT:
			printValue(pop());
			printf("\n");
			break;
		case OP_RETURN:
			return INTERPRET_OK;
		}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

void push(Value value) {
	*vm.stackTop++ = value;
}

Value pop() {
	return *(--vm.stackTop);
}

Value peek(int distance) {
	return vm.stackTop[-1 - distance];
}

void concatenate() {
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());
	int length = a->length + b->length;
	char* data = ALLOCATE(char, length + 1);
	memcpy(data, a->data, a->length);
	memcpy(data + a->length, b->data, b->length);
	data[length] = '\0';
	ObjString* string = takeString(data, length);
	return push(OBJ_VAL(string));
}

bool isFalsey(Value value) {
	//if (IS_NUMBER(value)) {
	//	return AS_NUMBER(value) == 0;
	//}
	// TODO should 0 == false?
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);
	size_t instruction = vm.ip - vm.chunk->code;
	int line = vm.chunk->lines[instruction]; // TODO prints wrong line number if running from repl
	fprintf(stderr, "[line %d] in script\n", line);
	resetStack();
}
