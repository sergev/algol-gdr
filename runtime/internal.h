//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Shared inside the runtime.
//
#ifndef ALGOL_INTERNAL_H
#define ALGOL_INTERNAL_H

#include <stdint.h>

// Text on stdout (print.c); flushed at once on a terminal.
void algol_out(const char *s);

// Before a message: the line in progress ended, stdout flushed.
void algol_end_line(void);

// Element types of arrays, as ast::Type.
enum { T_INTEGER, T_REAL, T_BOOLEAN, T_STRING, T_BITS };

// Element i of an array descriptor as an int64 cell.
#define ARRAY_DATA(desc) ((int64_t *)(intptr_t)(desc)[0])

#endif
