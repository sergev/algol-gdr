//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Output on stdout, and print: its values in the layout of X1's print, one
// blank between them; NEWLINE and NEWPAGE. An integer in decimal, a real in
// 13 digits (%.13g), a Boolean as true or false, a string as it is, bits in
// octal. Separate calls are not separated.
//
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "algol.h"
#include "internal.h"

static int last_char = '\n'; // the last character written
static int separate;         // a value of this print was written

// Text on stdout; on a terminal flushed at once, for prompts and progress.
void algol_out(const char *s)
{
    static int tty = -1;
    size_t n       = strlen(s);
    if (n == 0)
        return;
    fputs(s, stdout);
    last_char = (unsigned char)s[n - 1];
    if (tty < 0)
        tty = isatty(1);
    if (tty)
        fflush(stdout);
}

// Before a message on stderr: the line in progress ended.
void algol_end_line(void)
{
    if (last_char != '\n')
        algol_out("\n");
    fflush(stdout);
}

void algol_print_begin(void)
{
    separate = 0;
}

// A value of print: a blank before all but the first.
static void value(const char *s)
{
    if (separate)
        algol_out(" ");
    algol_out(s);
    separate = 1;
}

void algol_print_integer(int64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, v);
    value(buf);
}

void algol_print_real(double v)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%.13g", v);
    value(buf);
}

void algol_print_boolean(_Bool v)
{
    value(v ? "true" : "false");
}

void algol_print_bits(int64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRIo64, (uint64_t)v);
    value(buf);
}

void algol_print_string(const char *s)
{
    value(s ? s : "");
}

void algol_newline(void)
{
    algol_out("\n");
    separate = 0;
}

void algol_newpage(void)
{
    algol_out("\f");
    separate = 0;
}
