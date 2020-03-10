#pragma once

#include "object.h"

#define DEFAULT_CAPACITY 8

#define ALLOCATE(type, count) \
	(type*)reallocate(NULL, 0, sizeof(type) * (count));

#define FREE(type, pointer) \
	reallocate(pointer, sizeof(type), 0);

#define GROW_CAPACITY(capacity) \
	((capacity) < DEFAULT_CAPACITY ? DEFAULT_CAPACITY : (capacity) << 1)

#define GROW_ARRAY(previous, type, oldCount, count) \
	(type*)reallocate(previous, sizeof(type) * (oldCount), sizeof(type) * (count))

#define FREE_ARRAY(type, pointer, oldCount) \
	reallocate(pointer, sizeof(type) * (oldCount), 0)

void* reallocate(void*, size_t, size_t);
void collectGarbage();
void freeObjects();
