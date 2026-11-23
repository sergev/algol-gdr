//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
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

// A goto to a label of another function: frame is the label's frame; its
// slot 1 holds the jmp_buf, slot 2 gets the label's number.
void algol_goto(void **frame, int64_t index)
{
    if (!frame[1])
        algol_error("A goto to a label that cannot be reached");
    frame[2] = (void *)(intptr_t)index;
    _longjmp(*(jmp_buf *)frame[1], 1);
}

void algol_switch_error(int64_t index)
{
    algol_error("Switch index %lld out of range", (long long)index);
}
