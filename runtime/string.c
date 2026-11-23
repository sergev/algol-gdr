//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Strings: NUL-terminated UTF-8, never changed in place. Indices count
// characters (code points) from 1. A string never assigned (null) is empty.
// New strings come from malloc and are never freed.
//
#include <stdlib.h>
#include <string.h>

#include "algol.h"

static const char *str(const char *s)
{
    return s ? s : "";
}

// The byte offset of character i (from 1) of s; the end for i past it.
static size_t offset(const char *s, int64_t i)
{
    size_t pos = 0;
    for (int64_t k = 1; k < i && s[pos]; k++) {
        pos++;
        while ((s[pos] & 0xc0) == 0x80)
            pos++;
    }
    return pos;
}

int64_t algol_string_length(const char *s)
{
    int64_t n = 0;
    for (s = str(s); *s; s++)
        if ((*s & 0xc0) != 0x80)
            n++;
    return n;
}

int64_t algol_string_compare(const char *a, const char *b)
{
    int r = strcmp(str(a), str(b));
    return r < 0 ? -1 : r > 0;
}

char *algol_string_concat(const char *a, const char *b)
{
    a         = str(a);
    b         = str(b);
    size_t na = strlen(a), nb = strlen(b);
    char *r = malloc(na + nb + 1);
    if (!r)
        algol_error("No memory for a string");
    memcpy(r, a, na);
    memcpy(r + na, b, nb + 1);
    return r;
}

// Characters first..last of s.
char *algol_string_slice(const char *s, int64_t first, int64_t last)
{
    s           = str(s);
    int64_t len = algol_string_length(s);
    if (first < 1 || last > len || first > last + 1)
        algol_error("String index [%lld:%lld] out of range 1..%lld", (long long)first,
                    (long long)last, (long long)len);
    size_t from = offset(s, first), to = offset(s, last + 1);
    char *r = malloc(to - from + 1);
    if (!r)
        algol_error("No memory for a string");
    memcpy(r, s + from, to - from);
    r[to - from] = 0;
    return r;
}

char *algol_string_element(const char *s, int64_t i)
{
    return algol_string_slice(s, i, i);
}
