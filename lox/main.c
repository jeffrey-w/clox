#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

#define REPL 1
#define SOURCE 2
#define LINE_MAX 1024
#define READ "rb"
#define EX_USAGE 64
#define EX_DATAERR 65
#define EX_SOFTWARE 70
#define EX_IOERR 74

static void repl();
static void runFile(const char* path);
static char* readFile(const char* path);

int main(int argc, const char* argv[]) {
	initVM();
	if (argc == REPL) {
		repl();
	}
	else if (argc == SOURCE) {
		runFile(argv[1]);
	}
	else {
		fprintf(stderr, "Usage: lox [path]\n");
		exit(EX_USAGE);
	}
	freeVM();
	return 0;
}

void repl() {
	char line[LINE_MAX];
	while (true) {
		printf("> ");
		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		interpret(line);
	}
}

void runFile(const char* path) {
	char* source = readFile(path);
	InterpretResult result = interpret(source);
	free(source);
	if (result == INTERPRET_COMPILE_ERROR) {
		exit(EX_DATAERR);
	}
	if (result == INTERPRET_RUNTIME_ERROR) {
		exit(EX_SOFTWARE);
	}
}

char* readFile(const char* path) {
	FILE* file = fopen(path, READ);
	if (!file) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(EX_IOERR);
	}
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);
	char* buffer = (char*)malloc(fileSize + 1);
	if (!buffer) {
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(EX_IOERR);
	}
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(EX_IOERR);
	}
	buffer[bytesRead] = '\0';
	fclose(file);
	return buffer;
}
