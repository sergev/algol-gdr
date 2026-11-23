//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// read(…) and _read: values from standard input, separated by white space; a
// format, if any, is not used. A real may have ⏨ for E.
//
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algol.h"
#include "internal.h"

// The next word of the input.
static char *word(void)
{
    static char buf[1024];
    int c;
    size_t n = 0;
    fflush(stdout);
    while ((c = algol_in_byte()) != EOF && isspace(c))
        continue;
    if (c == EOF)
        algol_error("End of the input");
    while (c != EOF && !isspace(c)) {
        if (n + 1 < sizeof(buf))
            buf[n++] = (char)c;
        c = algol_in_byte();
    }
    buf[n] = 0;
    return buf;
}

int64_t algol_read_integer(void)
{
    char *end, *w = word();
    long long v = strtoll(w, &end, 10);
    if (*end)
        algol_error("Not an integer in the input: %s", w);
    return v;
}

double algol_read_real(void)
{
    char *end, *w = word();
    for (char *p; (p = strstr(w, "⏨")) != NULL;) {
        *p = 'e';
        memmove(p + 1, p + 3, strlen(p + 3) + 1);
    }
    double v = strtod(w, &end);
    if (*end)
        algol_error("Not a number in the input: %s", w);
    return v;
}

_Bool algol_read_boolean(void)
{
    char *w = word();
    for (char *p = w; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    if (strcmp(w, "true") == 0)
        return 1;
    if (strcmp(w, "false") == 0)
        return 0;
    algol_error("Not a Boolean in the input: %s", w);
}

char *algol_read_string(void)
{
    char *w = word();
    char *s = malloc(strlen(w) + 1);
    if (!s)
        algol_error("No memory for a string");
    return strcpy(s, w);
}

void algol_read_array(int64_t *desc, int64_t type)
{
    int64_t *data = ARRAY_DATA(desc);
    for (int64_t i = 0; i < desc[1]; i++) {
        switch (type) {
        case T_INTEGER:
        case T_BITS:
            data[i] = algol_read_integer();
            break;
        case T_REAL: {
            double v = algol_read_real();
            memcpy(&data[i], &v, 8);
            break;
        }
        case T_BOOLEAN:
            data[i] = algol_read_boolean();
            break;
        default:
            data[i] = (int64_t)(intptr_t)algol_read_string();
        }
    }
}
