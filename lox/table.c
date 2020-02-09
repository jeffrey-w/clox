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

bool tableGet(Table* table, ObjString* key, Value* value) {
	if (!table->count) {
		return false;
	}
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (!entry->key) {
		return false;
	}
	*value = entry->value;
	return true;
}

bool tableSet(Table* table, ObjString* key, Value value) {
	if (table->capacity * TABLE_MAX_LOAD < table->count + 1) {
		int capcity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capcity);
	}
	Entry* entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;
	if (isNewKey && IS_NIL(entry->value)) {
		table->count++;
	}
	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
	if (!table->count) {
		return false;
	}
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (!entry->key) {
		return false;
	}
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

void tableAddAll(Table* from, Table* to) {
	for (int i = 0; i < from->capacity; i++) {
		Entry* entry = &from->entries[i];
		if (entry->key) {
			tableSet(to, entry->key, entry->value);
		}
	}
}

void adjustCapacity(Table* table, int capacity) {
	Entry* entries = ALLOCATE(Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}
	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		Entry* entry = &table->entries[i];
		if (entry->key == NULL) {
			continue;
		}
		Entry* dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}
	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
	uint32_t index = key->hash & (capacity - 1);
	Entry* tombstone = NULL;
	while (true) {
		Entry* entry = &entries[index];
		if (!entry->key) {
			if (IS_NIL(entry->value)) {
				return tombstone ? tombstone : entry;
			}
			else {
				if (!tombstone) {
					tombstone = entry;
				}
			}
		}
		else {
			if (entry->key == key) {
				return entry;
			}
		}
		index = (index + 1) & (capacity - 1);
	}
}
