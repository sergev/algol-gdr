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

// Standard input (input.c): bytes, code points (EOF at the end), a code
// point put back; whether it is a terminal.
int algol_in_byte(void);
int32_t algol_in_symbol(void);
void algol_in_unget_symbol(int32_t c);
void algol_in_skip_line(void);
int algol_in_tty(void);

// Element types of arrays, as ast::Type.
enum { T_INTEGER, T_REAL, T_BOOLEAN, T_STRING, T_BITS };

// Element i of an array descriptor as an int64 cell.
#define ARRAY_DATA(desc) ((int64_t *)(intptr_t)(desc)[0])

#endif
