#include <math.h>
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

#define DEFAULT_NEXT_GC 0x100000

static void resetStack();
static void initEnv();
static void defineNative(const char*, NativeFn);
static InterpretResult run();
static Value peek(int);
static void concatenate();
static bool isFalsey(Value);
static ObjUpvalue* captureUpvalue(Value*);
static void closeUpvalues(Value*);
static void defineMethod(ObjString*);
static bool bindMethod(ObjClass*, ObjString*);
static bool callValue(Value, int);
static bool call(ObjClosure*, int);
static bool invoke(ObjString*, int);
static bool invokeFromClass(ObjClass*, ObjString*, int);
static void runtimeError(const char*, ...);

void initVM() {
	vm.bytesAllocated = 0;
	vm.nextGC = DEFAULT_NEXT_GC;
	vm.objects = NULL;
	vm.grayCount = 0;
	vm.grayCapacity = 0;
	vm.grayStack = NULL;
	resetStack();
	initTable(&vm.globals);
	initTable(&vm.strings);
	initEnv();
}

void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
	vm.openUpvalues = NULL;
}

void initEnv() {
	vm.initString = copyString("init", 4); // TODO avoid magic constants
	defineNative("clock", clockNative);
	defineNative("scan", scanNative);
	defineNative("sin", sinNative);
#ifdef DEBUG_DIAG_TOOLS
	defineNative("bytes_allocated", bytesAllocated);
	defineNative("next_gc", nextGC);
	defineNative("gc", gc);
	defineNative("print_stack", printStack);
	defineNative("print_globals", printGlobals);
	defineNative("print_strings", printStrings);
#endif // DEBUG_DEV_TOOLS
}

void defineNative(const char* name, NativeFn function) {
	push(OBJ_VAL(copyString(name, (int)strlen(name))));
	push(OBJ_VAL(newNative(function)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
}

void freeVM() {
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	freeObjects();
	vm.initString = NULL;
}

InterpretResult interpret(const char* source) {
	ObjFunction* function = compile(source);
	if (!function) {
		return INTERPRET_COMPILE_ERROR;
	}
	push(OBJ_VAL(function));
	ObjClosure* closure = newClosure(function);
	pop();
	push(OBJ_VAL(closure));
	callValue(OBJ_VAL(closure), 0);
	return run();
}

InterpretResult run() {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];
	// TODO store *frame->ip in a local register variable
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
	(frame->ip += 2, (uint16_t)((frame->ip[-2] << 8 | frame->ip[-1])))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
		disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
		case OP_GET_UPVALUE: {
			uint8_t slot = READ_BYTE();
			push(*frame->closure->upvalues[slot]->location);
			break;
		}
		case OP_SET_UPVALUE: {
			uint8_t slot = READ_BYTE();
			*frame->closure->upvalues[slot]->location = peek(0);
			break;
		}
		case OP_GET_PROPERTY: {
			ObjString* name = READ_STRING();
			if (IS_STRING(peek(0)) || IS_ARRAY(peek(0))) {
				if (!strcmp(name->data, "length")) {
					Value obj = pop(); // Obj.
					push(NUMBER_VAL(IS_STRING(obj) ? AS_STRING(obj)->length : AS_ARRAY(obj)->count));
				}
				else {
					runtimeError("%s have no property '%s'.", IS_STRING(peek(0)) ? "Strings" : "Arrays", name->data);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			if (!IS_INSTANCE(peek(0))) {
				runtimeError("Only instances have properties.");
				return INTERPRET_RUNTIME_ERROR;
			}
			ObjInstance* instance = AS_INSTANCE(peek(0));
			Value value;
			if (tableGet(&instance->fields, name, &value)) {
				pop(); // Instance.
				push(value);
				break;
			}
			if (!bindMethod(instance->cls, name)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_SET_PROPERTY: {
			if (!IS_INSTANCE(peek(1))) {
				runtimeError("Only instances have fields.");
				return INTERPRET_RUNTIME_ERROR;
			}
			ObjInstance* instance = AS_INSTANCE(peek(1));
			tableSet(&instance->fields, READ_STRING(), peek(0));
			Value value = pop();
			pop();
			push(value);
			break;
		}
		case OP_GET_INDEX: {
			if (!IS_ARRAY(peek(1))) {
				runtimeError("Can only index into arrays.");
				return INTERPRET_RUNTIME_ERROR;
			}
			if (!isInteger(peek(0))) {
				runtimeError("Index must be a nonnegative integer.");
				return INTERPRET_RUNTIME_ERROR;
			}
			int index = (int)AS_NUMBER(peek(0));
			ObjArray* array = AS_ARRAY(peek(1));
			if (index < 0 || index + 1 > array->count) {
				runtimeError("Index out of bounds: %d", index);
				return INTERPRET_RUNTIME_ERROR;
			}
			pop();
			pop();
			push(array->values[index]);
			break;
		}
		case OP_SET_INDEX: {
			if (!IS_ARRAY(peek(2))) {
				runtimeError("Can only index into arrays.");
				return INTERPRET_RUNTIME_ERROR;
			}
			if (!isInteger(peek(1))) {
				runtimeError("Index must be a nonnegative integer.");
				return INTERPRET_RUNTIME_ERROR;
			}
			int index = (int)AS_NUMBER(peek(1));
			ObjArray* array = AS_ARRAY(peek(2));
			if (index < 0 || index > array->count) {
				runtimeError("Index out of bounds: %d", index);
				return INTERPRET_RUNTIME_ERROR;
			}
			if (array->capacity < array->count + 1) {
				int oldCapacity = array->capacity;
				array->capacity = GROW_CAPACITY(oldCapacity);
				array->values = GROW_ARRAY(array->values, Value, oldCapacity, array->capacity);
			}
			if (index == array->count) {
				array->count++;
			}
			array->values[index] = pop();
			pop();
			pop();
			push(array->values[index]);
			break;
		}
		case OP_GET_SUPER: {
			ObjString* name = READ_STRING();
			ObjClass* superclass = AS_CLASS(pop());
			if (!bindMethod(superclass, name)) {
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
			if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
				vm.stackTop[-1] = OBJ_VAL(valueToString(peek(0)));
				vm.stackTop[-2] = OBJ_VAL(valueToString(peek(1)));
				concatenate();
			}
			else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());
				push(NUMBER_VAL(a + b));
			}
			else {
				runtimeError("Operands must be two numbers or at least one must be a string.");
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
		case OP_EXPONENTIATE:
			if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
				runtimeError("Operands must be numbers.");
				return INTERPRET_RUNTIME_ERROR;
			}
			double b = AS_NUMBER(pop());
			double a = AS_NUMBER(pop());
			push(NUMBER_VAL(pow(a, b)));
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
		case OP_INVOKE: {
			ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			if (!invoke(method, argCount)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}
		case OP_SUPER_INVOKE: {
			ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			ObjClass* superclass = AS_CLASS(pop());
			if (!invokeFromClass(superclass, method, argCount)) {
				return INTERPRET_RUNTIME_ERROR;
			}
			frame = &vm.frames[vm.frameCount - 1];
			break;
		}
		case OP_CLOSURE: {
			ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
			ObjClosure* closure = newClosure(function);
			push(OBJ_VAL(closure));
			for (int i = 0; i < closure->upvalueCount; i++) {
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();
				if (isLocal) {
					closure->upvalues[i] = captureUpvalue(frame->slots + index);
				}
				else {
					closure->upvalues[i] = frame->closure->upvalues[index];
				}
			}
			break;
		}
		case OP_CLOSE_UPVALUE:
			closeUpvalues(vm.stackTop - 1);
			pop();
			break;
		case OP_RETURN: {
			Value result = pop();
			closeUpvalues(frame->slots);
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
		case OP_CLASS:
			push(OBJ_VAL(newClass(READ_STRING())));
			break;
		case OP_INHERIT: {
			Value superclass = peek(1);
			ObjClass* subclass = AS_CLASS(peek(0));
			if (!IS_CLASS(superclass)) {
				runtimeError("Superclass must be a class.");
				return INTERPRET_RUNTIME_ERROR;
			}
			tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
			pop();
			break;
		}
		case OP_METHOD:
			defineMethod(READ_STRING());
			break;
		case OP_ARRAY: {
			int length = READ_BYTE();
			ObjArray* array = newArray();
			while (array->count < length) {
				if (array->capacity < array->count + 1) {
					int oldCapacity = array->capacity;
					array->capacity = GROW_CAPACITY(oldCapacity);
					array->values = GROW_ARRAY(array->values, Value, oldCapacity, array->capacity);
				}
				array->values[array->count++] = peek(length - array->count - 1);
			}
			for (int i = 0; i < length; i++) {
				pop();
			}
			push(OBJ_VAL(array));
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
	ObjString* b = AS_STRING(peek(0));
	ObjString* a = AS_STRING(peek(1));
	int length = a->length + b->length;
	char* data = ALLOCATE(char, length + 1);
	memcpy(data, a->data, a->length);
	memcpy(data + a->length, b->data, b->length);
	data[length] = '\0';
	ObjString* string = takeString(data, length);
	pop();
	pop();
	push(OBJ_VAL(string));
}

bool isFalsey(Value value) { // TODO move this to value.c
    if (IS_BOOL(value)) {
        return !AS_BOOL(value);
    }
    else if (IS_NIL(value)) {
        return true;
    }
    else if (IS_NUMBER(value)) {
        return !AS_NUMBER(value);
    }
    else if (IS_OBJ(value)) { 
		switch (AS_OBJ(value)->type) {
		case OBJ_STRING:
			return !strcmp(AS_CSTRING(value), "");
		case OBJ_NATIVE:
		case OBJ_FUNCTION:
		case OBJ_CLOSURE:
		case OBJ_CLASS:
		case OBJ_BOUND_METHOD:
		case OBJ_INSTANCE:
			return false;
		case OBJ_ARRAY:
			return !AS_ARRAY(value)->count;
		default:
			break;
		}
    }
    else {
        return true; // TODO need internal error logic
    }
}

ObjUpvalue* captureUpvalue(Value* local) {
	ObjUpvalue* prev = NULL;
	ObjUpvalue* upvalue = vm.openUpvalues;
	ObjUpvalue* created;
	while (upvalue && upvalue->location > local) {
		prev = upvalue;
		upvalue = upvalue->next;
	}
	if (upvalue && upvalue->location == local) {
		return upvalue;
	}
	created = newUpvalue(local);
	created->next = upvalue;
	if (!prev) {
		vm.openUpvalues = created;
	}
	else {
		prev->next = created;
	}
	return created;
}

void closeUpvalues(Value* last) {
	while (vm.openUpvalues && vm.openUpvalues->location >= last) {
		ObjUpvalue* upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.openUpvalues = upvalue->next;
	}
}

void defineMethod(ObjString* name) {
	Value method = peek(0);
	ObjClass* cls = AS_CLASS(peek(1));
	tableSet(&cls->methods, name, method);
	pop();
}

bool bindMethod(ObjClass* cls, ObjString* name) {
	Value method;
	if (!tableGet(&cls->methods, name, &method)) {
		runtimeError("Undefined property '%s'.", name->data);
		return false;
	}
	ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
	pop();
	push(OBJ_VAL(bound));
	return true;
}

bool callValue(Value callee, int argCount) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
		case OBJ_NATIVE: {
			NativeFn native = AS_NATIVE(callee);
			// TODO add support for catching runtime errors
			Value result = native(argCount, vm.stackTop - argCount);
			vm.stackTop -= argCount + 1;
			push(result);
			return true;
		}
		case OBJ_CLOSURE:
			return call(AS_CLOSURE(callee), argCount);
		case OBJ_CLASS: {
			Value initializer;
			ObjClass* cls = AS_CLASS(callee);
			vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(cls));
			if (tableGet(&cls->methods, vm.initString, &initializer)) {
				return call(AS_CLOSURE(initializer), argCount);
			}
			else if (argCount != 0) {
				runtimeError("Expected 0 arguments but got %d.", argCount);
			}
			return true;
		}
		case OBJ_BOUND_METHOD: {
			ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
			vm.stackTop[-argCount - 1] = bound->receiver;
			return call(bound->method, argCount);
		}
		default:
			// Non-callable object type.
			break;
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

bool call(ObjClosure* closure, int argCount) {
	if (argCount != closure->function->arity) {
		runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
		return false;
	}
	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow.");
		return false;
	}
	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

bool invoke(ObjString* name, int argCount) {
	Value receiver = peek(argCount);
	if (!IS_INSTANCE(receiver)) {
		runtimeError("Only instances have methods.");
		return false;
	}
	Value value;
	ObjInstance* instance = AS_INSTANCE(receiver);
	if (tableGet(&instance->fields, name, &value)) {
		vm.stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}
	return invokeFromClass(instance->cls, name, argCount);
}

bool invokeFromClass(ObjClass* cls, ObjString* name, int argCount) {
	Value method;
	if (!tableGet(&cls->methods, name, &method)) {
		runtimeError("Undefined property '%s'.", name->data);
		return false;
	}
	return call(AS_CLOSURE(method), argCount);
}


void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);
	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->closure->function;
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
