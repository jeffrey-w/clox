#pragma once

#include <string.h>

#include "memory.h"
#include "value.h"

Value clockNative(int, Value*);
Value scanNative(int, Value*);
Value sinNative(int, Value*);
Value bytesAllocated(int, Value*);
Value nextGC(int, Value*);
Value gc(int, Value*);
