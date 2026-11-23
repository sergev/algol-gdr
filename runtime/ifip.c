//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The input and output procedures of the IFIP modified report. Channel 0 is
// standard input, 1 standard output. A number is written in a form that
// inreal or ininteger reads back, then a terminator: a blank. A symbol is a
// character (a code point); insymbol gives its place in the string from 1,
// or minus its code when it is not there.
//
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algol.h"
#include "internal.h"

static void input(int64_t channel)
{
    if (channel != 0)
        algol_error("No input channel %lld", (long long)channel);
}

static void output(int64_t channel)
{
    if (channel != 1)
        algol_error("No output channel %lld", (long long)channel);
}

int64_t algol_ininteger(int64_t channel)
{
    input(channel);
    return algol_read_integer();
}

double algol_inreal(int64_t channel)
{
    input(channel);
    return algol_read_real();
}

// The next code point of standard input.
static int32_t getsym(void)
{
    int c = getchar();
    if (c == EOF)
        algol_error("End of the input");
    int n     = c < 0x80 ? 0 : c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    int32_t v = n == 0 ? c : c & (0x3f >> n);
    while (n-- > 0) {
        c = getchar();
        if (c == EOF)
            algol_error("End of the input");
        v = v << 6 | (c & 0x3f);
    }
    return v;
}

// The code point at s, and s past it.
static int32_t decode(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int n                  = *p < 0x80 ? 0 : *p < 0xe0 ? 1 : *p < 0xf0 ? 2 : 3;
    int32_t v              = n == 0 ? *p : *p & (0x3f >> n);
    for (p++; n-- > 0 && *p; p++)
        v = v << 6 | (*p & 0x3f);
    *s = (const char *)p;
    return v;
}

int64_t algol_insymbol(int64_t channel, const char *str)
{
    input(channel);
    fflush(stdout);
    int32_t c = getsym();
    int64_t k = 1;
    for (const char *s = str ? str : ""; *s; k++)
        if (decode(&s) == c)
            return k;
    return -c;
}

void algol_outterminator(int64_t channel)
{
    output(channel);
    algol_out(" ");
}

void algol_outinteger(int64_t channel, int64_t v)
{
    char buf[32];
    output(channel);
    snprintf(buf, sizeof(buf), "%" PRId64, v);
    algol_out(buf);
    algol_outterminator(channel);
}

// The fewest digits, 15 to 17, that read back the same.
void algol_outreal(int64_t channel, double v)
{
    char buf[48];
    output(channel);
    for (int digits = 15; digits <= 17; digits++) {
        snprintf(buf, sizeof(buf), "%.*g", digits, v);
        if (strtod(buf, NULL) == v)
            break;
    }
    algol_out(buf);
    algol_outterminator(channel);
}

void algol_outstring(int64_t channel, const char *s)
{
    output(channel);
    algol_out(s ? s : "");
}

// Symbol n of the string, or the code -n.
void algol_outsymbol(int64_t channel, const char *str, int64_t n)
{
    output(channel);
    if (n < 0) {
        char buf[8] = { 0 };
        int32_t c   = (int32_t)-n;
        if (c < 0x80) {
            buf[0] = (char)c;
        } else if (c < 0x800) {
            buf[0] = (char)(0xc0 | c >> 6);
            buf[1] = (char)(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            buf[0] = (char)(0xe0 | c >> 12);
            buf[1] = (char)(0x80 | (c >> 6 & 0x3f));
            buf[2] = (char)(0x80 | (c & 0x3f));
        } else {
            buf[0] = (char)(0xf0 | c >> 18);
            buf[1] = (char)(0x80 | (c >> 12 & 0x3f));
            buf[2] = (char)(0x80 | (c >> 6 & 0x3f));
            buf[3] = (char)(0x80 | (c & 0x3f));
        }
        algol_out(buf);
        return;
    }
    if (n < 1 || n > algol_string_length(str))
        algol_error("No symbol %lld in the string of outsymbol", (long long)n);
    char *sym = algol_string_slice(str, n, n);
    algol_out(sym);
    free(sym);
}

_Noreturn void algol_fault(const char *s, double r)
{
    algol_error("%s %.15g", s ? s : "", r);
}
