//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// _write, _print and _punch with a Fortran format: ''(I3, F7.2, 10HTEXT, 2X, A, /)''.
// Edits: Iw, Fw.d, Ew.d, Gw.d (as E), Aw, Lw, nX, nH…, '…', /, repeat counts and
// groups. When the items outrun the format it starts again with a new line;
// the statement ends its line. No carriage control: every column is printed.
//
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algol.h"
#include "internal.h"

enum { MAX_EDITS = 1024 };

typedef struct {
    char kind; // I F E A L X H /
    int w, d;
    const char *text; // H: its characters
} Edit;

static Edit edits[MAX_EDITS];
static int nedits, pos;
static char *line;
static size_t line_len, line_cap;

static void put(const char *s, size_t n)
{
    if (line_len + n + 1 > line_cap) {
        line_cap = (line_len + n + 1) * 2;
        line     = realloc(line, line_cap);
        if (!line)
            algol_error("No memory for output");
    }
    memcpy(line + line_len, s, n);
    line_len += n;
}

static void new_line(void)
{
    put("\n", 1);
    line[line_len] = 0; // put() leaves room
    algol_print_string(line);
    line_len = 0;
}

static int number(const char **p)
{
    int n = 0;
    while (isdigit((unsigned char)**p))
        n = n * 10 + *(*p)++ - '0';
    return n;
}

static void add(Edit e)
{
    if (nedits >= MAX_EDITS)
        algol_error("Format too long");
    edits[nedits++] = e;
}

// A list up to ')' or the end; groups expanded.
static void parse(const char **p)
{
    for (;;) {
        while (**p == ' ' || **p == ',')
            (*p)++;
        char c = **p;
        if (c == 0)
            return;
        if (c == ')') {
            (*p)++;
            return;
        }
        int has_n = isdigit((unsigned char)c);
        int n     = has_n ? number(p) : 1;
        c         = toupper((unsigned char)**p);
        if (c == 0)
            algol_error("Bad format");
        (*p)++;
        Edit e = { c, 0, 0, NULL };
        switch (c) {
        case '(': {
            int start = nedits;
            parse(p);
            int end = nedits;
            for (int k = 1; k < n; k++)
                for (int i = start; i < end; i++)
                    add(edits[i]);
            continue;
        }
        case 'H':
            e.text = *p;
            e.w    = n;
            for (int k = 0; k < n && **p; k++)
                (*p)++;
            add(e);
            continue;
        case '\'': {
            e.kind = 'H';
            e.text = *p;
            while (**p && **p != '\'')
                (*p)++;
            e.w = (int)(*p - e.text);
            if (**p)
                (*p)++;
            add(e);
            continue;
        }
        case 'X':
            e.w = n;
            add(e);
            continue;
        case '/':
            for (int k = 0; k < n; k++)
                add(e);
            continue;
        case 'I':
        case 'F':
        case 'E':
        case 'G':
        case 'A':
        case 'L':
            e.kind = c == 'G' ? 'E' : c;
            e.w    = number(p);
            if (**p == '.') {
                (*p)++;
                e.d = number(p);
            }
            for (int k = 0; k < n; k++)
                add(e);
            continue;
        default:
            algol_error("Bad format at '%c'", c);
        }
    }
}

// The edits up to the next data edit; at the end of the format, a new line
// and the format again (when data is waiting).
static const Edit *next_data(int data)
{
    int wrapped = 0;
    for (;;) {
        if (pos >= nedits) {
            if (!data || wrapped)
                return NULL;
            new_line();
            pos     = 0;
            wrapped = 1;
            continue;
        }
        const Edit *e = &edits[pos];
        switch (e->kind) {
        case 'H':
            put(e->text, e->w);
            break;
        case 'X':
            for (int k = 0; k < e->w; k++)
                put(" ", 1);
            break;
        case '/':
            new_line();
            break;
        default:
            if (!data)
                return NULL;
            pos++;
            return e;
        }
        pos++;
    }
}

// A field of width w, right-justified; asterisks when it does not fit.
static void field(const char *s, int w)
{
    size_t n    = strlen(s);
    int64_t len = algol_string_length(s); // characters
    if (w == 0) {
        put(s, n);
        return;
    }
    if (len > w) {
        for (int k = 0; k < w; k++)
            put("*", 1);
        return;
    }
    for (int64_t k = len; k < w; k++)
        put(" ", 1);
    put(s, n);
}

void algol_format_begin(const char *format)
{
    const char *p = format ? format : "";
    while (*p == ' ')
        p++;
    if (*p == '(')
        p++;
    nedits = pos = 0;
    parse(&p);
    line_len = 0;
}

void algol_format_end(void)
{
    next_data(0);
    new_line();
}

static void real_field(const Edit *e, double v)
{
    char buf[64];
    if (e->kind == 'I') {
        snprintf(buf, sizeof(buf), "%lld", (long long)llround(v));
    } else if (e->kind == 'E') {
        // 0.ddddE±xx
        int ex   = 0;
        double m = fabs(v);
        if (m != 0) {
            ex = (int)floor(log10(m)) + 1;
            m /= pow(10, ex);
            if (m + 0.5 * pow(10, -e->d) >= 1) {
                m /= 10;
                ex++;
            }
        }
        snprintf(buf, sizeof(buf), "%s0.%0*.0fE%+03d", v < 0 ? "-" : "", e->d, m * pow(10, e->d),
                 ex);
    } else {
        snprintf(buf, sizeof(buf), "%.*f", e->d, v);
    }
    field(buf, e->w);
}

void algol_format_integer(int64_t v)
{
    const Edit *e = next_data(1);
    char buf[32];
    if (!e)
        algol_error("Format without data edits");
    if (e->kind == 'F' || e->kind == 'E') {
        real_field(e, (double)v);
        return;
    }
    snprintf(buf, sizeof(buf), "%lld", (long long)v);
    field(buf, e->w);
}

void algol_format_real(double v)
{
    const Edit *e = next_data(1);
    if (!e)
        algol_error("Format without data edits");
    real_field(e, v);
}

void algol_format_boolean(_Bool v)
{
    const Edit *e = next_data(1);
    if (!e)
        algol_error("Format without data edits");
    field(v ? "T" : "F", e->w);
}

void algol_format_string(const char *s)
{
    const Edit *e = next_data(1);
    if (!e)
        algol_error("Format without data edits");
    s = s ? s : "";
    if (e->kind == 'A' && e->w > 0 && (int)algol_string_length(s) > e->w) {
        char *head = algol_string_slice(s, 1, e->w);
        put(head, strlen(head));
        free(head);
        return;
    }
    field(s, e->kind == 'A' ? e->w : 0);
}

void algol_format_array(const int64_t *desc, int64_t type)
{
    const int64_t *data = ARRAY_DATA(desc);
    for (int64_t i = 0; i < desc[1]; i++) {
        switch (type) {
        case T_INTEGER:
        case T_BITS:
            algol_format_integer(data[i]);
            break;
        case T_REAL: {
            double v;
            memcpy(&v, &data[i], 8);
            algol_format_real(v);
            break;
        }
        case T_BOOLEAN:
            algol_format_boolean(*(const unsigned char *)&data[i] != 0);
            break;
        default:
            algol_format_string((const char *)(intptr_t)data[i]);
        }
    }
}
