#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

static void adjustCapacity(Table* table, int capacity);
static Entry* findEntry(Entry* entries, int capacity, ObjString* key);

void initTable(Table* table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table* table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

bool tableSet(Table* table, ObjString* key, Value value) {
	if (table->capacity * TABLE_MAX_LOAD < table->count + 1) {
		int capcity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capcity);
	}
	Entry* entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;
	if (isNewKey) {
		table->count++;
	}
	entry->key = key;
	entry->value = value;
	return isNewKey;
}

void adjustCapacity(Table* table, int capacity) {
	Entry* entries = ALLOCATE(Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}
	table->entries = entries;
	table->capacity = capacity;
}

Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
	uint32_t index = key->hash & (capacity - 1);
	while (true) {
		Entry* entry = &entries[index];
		if (entry->key == key || entry->key == NULL) {
			return entry;
		}
		index = (index + 1) & (capacity - 1);
	}
}
