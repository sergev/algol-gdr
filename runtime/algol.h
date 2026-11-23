//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The run-time library of algc: what the generated code calls. See
// doc/codegen.md for the data representation.
//
#ifndef ALGOL_RUNTIME_H
#define ALGOL_RUNTIME_H

#include <stdint.h>

// The program, made by algc.
void algol_main(void);

// Errors at run time: a message on stderr, exit status 1.
_Noreturn void algol_error(const char *fmt, ...);

// The end of the program: the print line flushed.
_Noreturn void algol_stop(void);

// PRINT.
void algol_print_integer(int64_t v);
void algol_print_real(double v);
void algol_print_boolean(_Bool v);
void algol_print_bits(int64_t v);
void algol_print_string(const char *s);
void algol_newline(void);
void algol_newpage(void);

// Exponentiation.
int64_t algol_ipow(int64_t base, int64_t exp);
double algol_rpow(double base, int64_t exp);
double algol_power(double x, double y);
int64_t algol_bits_shift(int64_t bits, int64_t n);

// Standard functions with a domain.
double algol_sqrt(double x);
double algol_ln(double x);

// Arrays: descriptors of int64 words (doc/codegen.md).
void algol_array_alloc(int64_t *desc, int64_t dims);
void algol_array_free(int64_t *desc);
void algol_check_index(const int64_t *desc, int64_t dim, int64_t i);
int64_t *algol_array_copy(const int64_t *src, int64_t dims);
void algol_print_array(const int64_t *desc, int64_t type);

#endif
