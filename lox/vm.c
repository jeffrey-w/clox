#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "natives.h"
#include "value.h"
#include "vm.h"

static InterpretResult run();
static void resetStack();
static Value peek(int);
static void concatenate();
static bool isFalsey(Value);
static bool callValue(Value, int);
static bool call(ObjFunction*, int);
static void defineNative(const char*, NativeFn);
static void runtimeError(const char*, ...);

void initVM() {
	resetStack();
	initTable(&vm.globals);
	initTable(&vm.strings);
	defineNative("clock", clockNative);
	vm.objects = NULL;
}

void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
}

void freeVM() {
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	freeObjects();
}

InterpretResult interpret(const char* source) {
	ObjFunction* function = compile(source);
	if (!function) {
		return INTERPRET_COMPILE_ERROR;
	}
	push(OBJ_VAL(function));
	callValue(OBJ_VAL(function), 0);
	return run();
}

InterpretResult run() {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
	(frame->ip += 2, (uint16_t)((frame->ip[-2] << 8 | frame->ip[-1])))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
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
		disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
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
		case OP_GET_LOCAL: {
			uint8_t slot = READ_BYTE();
			push(frame->slots[slot]);
			break;
		}
		case OP_SET_LOCAL: {
			uint8_t slot = READ_BYTE();
			frame->slots[slot] = peek(0);
			break;
		}
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
		case OP_SET_GLOBAL: {
			ObjString* name = READ_STRING();
			if (tableSet(&vm.globals, name, peek(0))) {
				tableDelete(&vm.globals, name);
				runtimeError("Undefined variable '%s'", name->data);
				return INTERPRET_RUNTIME_ERROR;
			}
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
		case OP_JUMP: {
			uint8_t offset = READ_SHORT();
			frame->ip += offset;
			break;
		}
		case OP_JUMP_IF_FALSE: {
			uint8_t offset = READ_SHORT();
			if (isFalsey(peek(0))) {
				frame->ip += offset;
			}
			break;
		}
		case OP_LOOP: {
			uint16_t offset = READ_SHORT();
			frame->ip -= offset;
			break;
		}
		case OP_CALL: {
			int argCount = READ_BYTE();
			if (!callValue(peek(argCount), argCount)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}
		case OP_RETURN: {
			Value result = pop();
			vm.frameCount--;
			if (!vm.frameCount) {
				pop();
				return INTERPRET_OK;
			}
			vm.stackTop = frame->slots;
			push(result);
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}
		}
	}
#undef READ_BYTE
#undef READ_SHORT
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
	push(OBJ_VAL(string));
}

bool isFalsey(Value value) {
	//if (IS_NUMBER(value)) {
	//	return AS_NUMBER(value) == 0;
	//}
	// TODO should 0 == false?
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool callValue(Value callee, int argCount) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
		case OBJ_NATIVE: {
			NativeFn native = AS_NATIVE(callee);
			Value result = native(argCount, vm.stackTop - argCount);
			vm.stackTop -= argCount + 1;
			push(result);
			return true;
		}
		case OBJ_FUNCTION:
			return call(AS_FUNCTION(callee), argCount);
		default:
			// Non-callable object type.
			break;
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

bool call(ObjFunction* function, int argCount) {
	if (argCount != function->arity) {
		runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
		return false;
	}
	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow.");
		return false;
	}
	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

void defineNative(const char* name, NativeFn function) {
	push(OBJ_VAL(copyString(name, (int)strlen(name))));
	push(OBJ_VAL(newNative(function)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
}

void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);
	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->function;
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
		if (!function->name) {
			fprintf(stderr, "script\n");
		}
		else {
			fprintf(stderr, "%s()\n", function->name->data);
		}
	}
	resetStack();
}
