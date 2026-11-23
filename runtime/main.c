//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "algol.h"
#include "internal.h"

void algol_error(const char *fmt, ...)
{
    va_list ap;
    algol_flush_output();
    fflush(stdout);
    fputs("Error: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void algol_stop(void)
{
    algol_flush_output();
    exit(0);
}

int main(void)
{
    algol_main();
    algol_stop();
}
