//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Output of PRINT, as the original's A/DRUCK: a line of 126 columns after
// a blank carriage-control column.
//
#include <stdio.h>
#include <string.h>

#include "algol.h"
#include "internal.h"

enum { LINE_WIDTH = 126 };

static char line[4 * LINE_WIDTH + 64]; // UTF-8 bytes
static int line_bytes;                 // used in line[]
static int column;                     // columns used, 0..126

void algol_flush_output(void)
{
    if (column > 0 || line_bytes > 0) {
        while (line_bytes > 0 && line[line_bytes - 1] == ' ')
            line_bytes--;
        fputc(' ', stdout);
        fwrite(line, 1, line_bytes, stdout);
        fputc('\n', stdout);
    }
    line_bytes = 0;
    column     = 0;
}

// One character, of n bytes; a full line is printed first.
static void put_char(const char *s, int n)
{
    if (column >= LINE_WIDTH)
        algol_flush_output();
    memcpy(line + line_bytes, s, n);
    line_bytes += n;
    column++;
}

void algol_print_string(const char *s)
{
    while (*s) {
        int n           = 1;
        unsigned char c = *s;
        if (c >= 0xf0)
            n = 4;
        else if (c >= 0xe0)
            n = 3;
        else if (c >= 0xc0)
            n = 2;
        put_char(s, n);
        s += n;
    }
}
