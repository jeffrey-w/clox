#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//#define DEBUG_TRACE_EXECUTION
//#define DEBUG_PRINT_CODE
//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC
//#define DEBUG_DIAG_TOOLS
#define UINT8_COUNT (UINT8_MAX + 1)

#define STRCMP(a, b, c, d) \
	(a) == (b) && !memcmp((c), (d), (a))

// TODO use consistent NULL/0 check in control expressions
// TODO hoist "private" globals to implementation files
