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

// A goto to a label of another function, and a bad switch index.
_Noreturn void algol_goto(void **frame, int64_t index);
_Noreturn void algol_switch_error(int64_t index);

// PRINT.
void algol_print_integer(int64_t v);
void algol_print_real(double v);
void algol_print_boolean(_Bool v);
void algol_print_bits(int64_t v);
void algol_print_string(const char *s);
void algol_newline(void);
void algol_newpage(void);

// _write with a format.
void algol_format_begin(int64_t unit, const char *format);
void algol_format_end(void);
void algol_encode_begin(const char *format);
char *algol_encode_end(int64_t count);
void algol_fread_begin(int64_t unit, const char *format);
void algol_decode_begin(const char *format, const char *string);
void algol_fread_end(void);
int64_t algol_fread_integer(void);
double algol_fread_real(void);
_Bool algol_fread_boolean(void);
char *algol_fread_string(void);
void algol_fread_array(int64_t *desc, int64_t type);
void algol_rewind(int64_t unit);
void algol_backspace(int64_t unit);
void algol_endfile(int64_t unit);
void algol_format_integer(int64_t v);
void algol_format_real(double v);
void algol_format_boolean(_Bool v);
void algol_format_string(const char *s);
void algol_format_array(const int64_t *desc, int64_t type);

// READ: values from standard input.
int64_t algol_read_integer(void);
double algol_read_real(void);
_Bool algol_read_boolean(void);
char *algol_read_string(void);
void algol_read_array(int64_t *desc, int64_t type);

// The IFIP procedures (ifip.c).
int64_t algol_ininteger(int64_t channel);
double algol_inreal(int64_t channel);
int64_t algol_insymbol(int64_t channel, const char *str);
void algol_outinteger(int64_t channel, int64_t v);
void algol_outreal(int64_t channel, double v);
void algol_outstring(int64_t channel, const char *s);
void algol_outsymbol(int64_t channel, const char *str, int64_t n);
void algol_outterminator(int64_t channel);
_Noreturn void algol_fault(const char *s, double r);

// Integer division, exponentiation.
int64_t algol_idiv(int64_t a, int64_t b);
int64_t algol_ipow(int64_t base, int64_t exp);
double algol_rpow(double base, int64_t exp);
double algol_power(double x, double y);
int64_t algol_bits_shift(int64_t bits, int64_t n);

// Standard functions with a domain.
double algol_sqrt(double x);
double algol_ln(double x);

// Strings: UTF-8, indices of characters from 1.
int64_t algol_string_length(const char *s);
int64_t algol_string_compare(const char *a, const char *b);
char *algol_string_concat(const char *a, const char *b);
char *algol_string_slice(const char *s, int64_t first, int64_t last);
char *algol_string_element(const char *s, int64_t i);

// Arrays: descriptors of int64 words: data, count, dims, bounds (doc/codegen.md).
void algol_array_alloc(int64_t *desc, int64_t dims);
void algol_array_free(int64_t *desc);
void algol_array_alloc_own(int64_t *desc, int64_t dims);
int64_t algol_array_mark(void);
void algol_array_unwind(int64_t mark);
void algol_check_index(const int64_t *desc, int64_t dim, int64_t i);
int64_t *algol_array_copy(const int64_t *src, int64_t dims);
void algol_array_release(int64_t *desc);
void algol_array_bind(int64_t *desc, int64_t dims, int64_t *data);
void algol_print_array(const int64_t *desc, int64_t type);

#endif
