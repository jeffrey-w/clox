#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "natives.h"
#include "vm.h"

Value clockNative(int argCount, Value* args) {
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

Value scanNative(int argCount, Value* args) { // TODO this is not robust (e.g. fails if input is typed too fast)
	size_t size = 0, len = 0;
	char* buffer = NULL;
	do {
		if (size < len + 1) {
			int old = size;
			int size = GROW_CAPACITY(old);
			buffer = GROW_ARRAY(buffer, char, old, size);
		}
		buffer[len++] = fgetc(stdin);
	} while(buffer[len - 1] != '\n');
	buffer[len - 1] = '\0';
	return OBJ_VAL(takeString(buffer, len - 1));
}

Value sinNative(int argCount, Value* args) {
	// TODO if (argCount != 1 || !IS_NUMBER(args[0])) runtime error?
	return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

Value bytesAllocated(int argCount, Value* args) {
	return NUMBER_VAL(vm.bytesAllocated);
}

Value nextGC(int argCount, Value* args) {
	return NUMBER_VAL(vm.nextGC);
}

Value gc(int argCount, Value* args) {
	collectGarbage();
	return NIL_VAL;
}
