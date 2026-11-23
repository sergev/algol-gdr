//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Output of PRINT: printf on stdout, in the fields of the original (15
// columns for an integer, 20 for a real, 6 for a Boolean).
//
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "algol.h"
#include "internal.h"

static int last_char = '\n'; // the last character written

static void out(const char *s)
{
    size_t n = strlen(s);
    if (n == 0)
        return;
    fputs(s, stdout);
    last_char = (unsigned char)s[n - 1];
}

void algol_flush_output(void)
{
    if (last_char != '\n')
        out("\n");
    fflush(stdout);
}

void algol_print_integer(int64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%14" PRId64 " ", v);
    out(buf);
}

void algol_print_real(double v)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "% .11e   ", v);
    out(buf);
}

void algol_print_boolean(_Bool v)
{
    out(v ? "true  " : "false ");
}

void algol_print_bits(int64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%022" PRIo64 " ", (uint64_t)v);
    out(buf);
}

void algol_print_string(const char *s)
{
    out(s);
}

void algol_newline(void)
{
    out("\n");
}

void algol_newpage(void)
{
    out("\f");
}
