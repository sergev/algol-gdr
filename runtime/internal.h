//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Shared inside the runtime.
//
#ifndef ALGOL_INTERNAL_H
#define ALGOL_INTERNAL_H

#include <stdint.h>

// Print the line in progress, if any.
void algol_flush_output(void);

// Element types of arrays, as ast::Type.
enum { T_INTEGER, T_REAL, T_BOOLEAN, T_STRING, T_BITS };

// Element i of an array descriptor as an int64 cell.
#define ARRAY_DATA(desc) ((int64_t *)(intptr_t)(desc)[0])

#endif
