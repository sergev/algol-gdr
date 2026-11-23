//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Standard input, shared by read, _read and the IFIP and X1 procedures: a
// few bytes can be put back.
//
#include <stdio.h>
#include <unistd.h>

#include "internal.h"

static unsigned char pushed[8];
static int npushed;
static int stdin_tty = -1;

int algol_in_byte(void)
{
    return npushed > 0 ? pushed[--npushed] : getchar();
}

static void put_back(int c)
{
    if (c != EOF && npushed < (int)sizeof(pushed))
        pushed[npushed++] = (unsigned char)c;
}

int algol_in_tty(void)
{
    if (stdin_tty < 0)
        stdin_tty = isatty(0);
    return stdin_tty;
}

// The next code point; EOF at the end of the input.
int32_t algol_in_symbol(void)
{
    int c = algol_in_byte();
    if (c == EOF || c < 0x80)
        return c;
    int n     = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    int32_t v = c & (0x3f >> n);
    while (n-- > 0) {
        c = algol_in_byte();
        if (c == EOF)
            return EOF;
        v = v << 6 | (c & 0x3f);
    }
    return v;
}

// Put back a code point read by algol_in_symbol.
void algol_in_unget_symbol(int32_t v)
{
    char buf[4];
    int n = 0;
    if (v < 0x80) {
        buf[n++] = (char)v;
    } else if (v < 0x800) {
        buf[n++] = (char)(0xc0 | v >> 6);
        buf[n++] = (char)(0x80 | (v & 0x3f));
    } else {
        buf[n++] = (char)(0xe0 | v >> 12);
        buf[n++] = (char)(0x80 | (v >> 6 & 0x3f));
        buf[n++] = (char)(0x80 | (v & 0x3f));
    }
    while (n > 0)
        put_back((unsigned char)buf[--n]);
}

// The rest of the line dropped.
void algol_in_skip_line(void)
{
    int c;
    while ((c = algol_in_byte()) != EOF && c != '\n')
        continue;
}
