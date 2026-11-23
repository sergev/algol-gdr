//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Input and output with a Fortran format: ''(I3, F7.2, 10HTEXT, 2X, A, /)''.
// Edits: Iw, Fw.d, Ew.d, Gw.d (as E), Aw, Lw, nX, nH…, '…', /, repeat counts and
// groups. When the items outrun the format it starts again with a new record;
// a statement ends its record. No carriage control: every column is printed.
//
// Output goes to standard output (_print, _punch, unit 54), a unit file
// (_write), or a string (_encode); input comes from standard input (unit
// 54), a unit file (_read) or a string (_decode). On input a data edit of
// width 0 takes the next word of the record.
//
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

// The record being written or read.
static char *line;
static size_t line_len, line_cap, line_pos;

// Where it goes or comes from.
static FILE *file;    // a unit file, or null for standard input and output
static int to_string; // _encode
static char *text;    // _encode: what is written so far
static size_t text_len;
static const char *source; // _decode: the rest of the string

static void put(const char *s, size_t n)
{
    if (line_len + n + 1 > line_cap) {
        line_cap = (line_len + n + 1) * 2;
        line     = realloc(line, line_cap);
        if (!line)
            algol_error("No memory for a record");
    }
    memcpy(line + line_len, s, n);
    line_len += n;
    line[line_len] = 0;
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

static void start(const char *format)
{
    const char *p = format ? format : "";
    while (*p == ' ')
        p++;
    if (*p == '(')
        p++;
    nedits = pos = 0;
    parse(&p);
    line_len = line_pos = 0;
    put("", 0);
}

//
// Units: 54 is the terminal; n is the file $ALGC_UNITn, else fort.n.
//
enum { MAX_UNITS = 100, TERMINAL = 54 };

static FILE *units[MAX_UNITS];

static FILE *unit_file(int64_t unit, int writing)
{
    if (unit < 0 || unit >= MAX_UNITS)
        algol_error("No unit %lld", (long long)unit);
    if (units[unit])
        return units[unit];
    char var[32], name[32];
    snprintf(var, sizeof(var), "ALGC_UNIT%d", (int)unit);
    snprintf(name, sizeof(name), "fort.%d", (int)unit);
    const char *path = getenv(var) ? getenv(var) : name;
    units[unit]      = fopen(path, writing ? "w+" : "r+");
    if (!units[unit])
        algol_error("Cannot open %s for unit %lld", path, (long long)unit);
    return units[unit];
}

// The file of a unit, or null for the terminal.
static FILE *unit_of(int64_t unit, int writing)
{
    return unit < 0 || unit == TERMINAL ? NULL : unit_file(unit, writing);
}

void algol_rewind(int64_t unit)
{
    if (unit != TERMINAL && unit >= 0 && unit < MAX_UNITS && units[unit])
        rewind(units[unit]);
}

// To the start of the record before the current position.
void algol_backspace(int64_t unit)
{
    if (unit == TERMINAL || unit < 0 || unit >= MAX_UNITS || !units[unit])
        return;
    FILE *f  = units[unit];
    long off = ftell(f);
    if (off <= 0)
        return;
    off--; // the end of the last record
    while (off > 0) {
        fseek(f, off - 1, SEEK_SET);
        if (getc(f) == '\n')
            break;
        off--;
    }
    fseek(f, off, SEEK_SET);
}

// The file ends at the current position.
void algol_endfile(int64_t unit)
{
    if (unit == TERMINAL || unit < 0 || unit >= MAX_UNITS || !units[unit])
        return;
    FILE *f = units[unit];
    fflush(f);
    if (ftruncate(fileno(f), ftell(f)) != 0)
        algol_error("Cannot end the file of unit %lld", (long long)unit);
}

//
// Output.
//
static void new_line(void)
{
    if (to_string) {
        // Records of a string are separated by '\n'.
        text = realloc(text, text_len + line_len + 2);
        if (!text)
            algol_error("No memory for a string");
        if (text_len)
            text[text_len++] = '\n';
        memcpy(text + text_len, line, line_len);
        text_len += line_len;
        text[text_len] = 0;
    } else if (file) {
        fwrite(line, 1, line_len, file);
        putc('\n', file);
    } else {
        put("\n", 1);
        algol_print_string(line);
    }
    line_len = 0;
    put("", 0);
}

// The edits up to the next data edit; at the end of the format, a new record
// and the format again (when data is waiting). record starts a record.
static const Edit *next_edit(int data, void (*record)(void))
{
    int wrapped = 0;
    for (;;) {
        if (pos >= nedits) {
            if (!data || wrapped)
                return NULL;
            record();
            pos     = 0;
            wrapped = 1;
            continue;
        }
        const Edit *e = &edits[pos];
        switch (e->kind) {
        case 'H':
            if (record == new_line)
                put(e->text, e->w);
            else
                line_pos += e->w;
            break;
        case 'X':
            if (record == new_line)
                for (int k = 0; k < e->w; k++)
                    put(" ", 1);
            else
                line_pos += e->w;
            break;
        case '/':
            record();
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

static const Edit *next_data(void)
{
    const Edit *e = next_edit(1, new_line);
    if (!e)
        algol_error("Format without data edits");
    return e;
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

void algol_format_begin(int64_t unit, const char *format)
{
    file      = unit_of(unit, 1);
    to_string = 0;
    start(format);
}

void algol_format_end(void)
{
    next_edit(0, new_line);
    new_line();
    if (file)
        fflush(file);
}

void algol_encode_begin(const char *format)
{
    file      = NULL;
    to_string = 1;
    free(text);
    text     = NULL;
    text_len = 0;
    start(format);
}

// The text, count characters: padded with blanks or cut.
char *algol_encode_end(int64_t count)
{
    next_edit(0, new_line);
    new_line();
    to_string     = 0;
    const char *s = text ? text : "";
    int64_t n     = algol_string_length(s);
    char *r;
    if (n >= count) {
        r = algol_string_slice(s, 1, count);
    } else {
        size_t len = strlen(s);
        r          = malloc(len + (count - n) + 1);
        if (!r)
            algol_error("No memory for a string");
        memcpy(r, s, len);
        memset(r + len, ' ', count - n);
        r[len + count - n] = 0;
    }
    free(text);
    text     = NULL;
    text_len = 0;
    return r;
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
    const Edit *e = next_data();
    char buf[32];
    if (e->kind == 'F' || e->kind == 'E') {
        real_field(e, (double)v);
        return;
    }
    snprintf(buf, sizeof(buf), "%lld", (long long)v);
    field(buf, e->w);
}

void algol_format_real(double v)
{
    real_field(next_data(), v);
}

void algol_format_boolean(_Bool v)
{
    const Edit *e = next_data();
    field(v ? "T" : "F", e->w);
}

void algol_format_string(const char *s)
{
    const Edit *e = next_data();
    s             = s ? s : "";
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

//
// Input: records from the terminal, a unit file or a string.
//
static int at_end; // no record could be read

static void next_record(void)
{
    line_len = line_pos = 0;
    put("", 0);
    if (source) {
        // _decode: the records of the string.
        const char *nl = strchr(source, '\n');
        size_t n       = nl ? (size_t)(nl - source) : strlen(source);
        put(source, n);
        source = nl ? nl + 1 : source + n;
        return;
    }
    FILE *f = file ? file : stdin;
    if (!file)
        fflush(stdout);
    int c;
    at_end = 1;
    while ((c = getc(f)) != EOF) {
        at_end = 0;
        if (c == '\n')
            break;
        char ch = (char)c;
        put(&ch, 1);
    }
}

// The characters of a field of width w, or the next word for w = 0.
static char *take(int w)
{
    static char buf[1024];
    size_t n = 0;
    if (w == 0) {
        while (line_pos < line_len && isspace((unsigned char)line[line_pos]))
            line_pos++;
        if (line_pos >= line_len && at_end)
            algol_error("End of the input");
        while (line_pos < line_len && !isspace((unsigned char)line[line_pos]) &&
               n + 1 < sizeof(buf))
            buf[n++] = line[line_pos++];
    } else {
        for (int k = 0; k < w && n + 1 < sizeof(buf); k++)
            buf[n++] = line_pos < line_len ? line[line_pos++] : ' ';
    }
    buf[n] = 0;
    return buf;
}

static const Edit *next_input(void)
{
    const Edit *e = next_edit(1, next_record);
    if (!e)
        algol_error("Format without data edits");
    return e;
}

// Blanks out of a numeric field.
static char *squeeze(char *s)
{
    char *d = s;
    for (const char *p = s; *p; p++)
        if (*p != ' ')
            *d++ = *p;
    *d = 0;
    return s;
}

// A real; without a point, the last d digits of the mantissa are the
// fraction.
static double read_real(const Edit *e)
{
    char *s = squeeze(take(e->w));
    if (!*s)
        return 0;
    char *end;
    double v = strtod(s, &end);
    if (*end)
        algol_error("Not a number in the input: %s", s);
    if (!strchr(s, '.') && e->d > 0) {
        char *x = strpbrk(s, "eEdD+-");
        if (x == s && (*x == '+' || *x == '-'))
            x = strpbrk(s + 1, "eEdD+-");
        double m = strtod(s, NULL);
        double p = x ? pow(10, strtol(x + (toupper((unsigned char)*x) == 'E' ||
                                           toupper((unsigned char)*x) == 'D'),
                                      NULL, 10))
                     : 1;
        v        = m / pow(10, e->d) * p;
    }
    return v;
}

static int64_t read_integer(const Edit *e)
{
    if (e->kind == 'F' || e->kind == 'E')
        return llround(read_real(e));
    char *s = squeeze(take(e->w));
    if (!*s)
        return 0;
    char *end;
    long long v = strtoll(s, &end, 10);
    if (*end)
        algol_error("Not an integer in the input: %s", s);
    return v;
}

void algol_fread_begin(int64_t unit, const char *format)
{
    file   = unit_of(unit, 0);
    source = NULL;
    start(format);
    next_record();
}

// The first count characters of string, or all of it when shorter.
void algol_decode_begin(const char *format, const char *string, int64_t count)
{
    string = string ? string : "";
    if (count < algol_string_length(string))
        string = algol_string_slice(string, 1, count < 0 ? 0 : count);
    file   = NULL;
    source = string;
    start(format);
    next_record();
}

void algol_fread_end(void)
{
    source = NULL;
}

int64_t algol_fread_integer(void)
{
    return read_integer(next_input());
}

double algol_fread_real(void)
{
    const Edit *e = next_input();
    if (e->kind == 'I')
        return (double)read_integer(e);
    return read_real(e);
}

_Bool algol_fread_boolean(void)
{
    char *s = squeeze(take(next_input()->w));
    if (*s == '.')
        s++;
    char c = (char)toupper((unsigned char)*s);
    if (c != 'T' && c != 'F')
        algol_error("Not a Boolean in the input: %s", s);
    return c == 'T';
}

char *algol_fread_string(void)
{
    const Edit *e = next_input();
    const char *s = e->w ? take(e->w) : line + line_pos;
    char *r       = malloc(strlen(s) + 1);
    if (!r)
        algol_error("No memory for a string");
    strcpy(r, s);
    if (!e->w)
        line_pos = line_len;
    return r;
}

void algol_fread_array(int64_t *desc, int64_t type)
{
    int64_t *data = ARRAY_DATA(desc);
    for (int64_t i = 0; i < desc[1]; i++) {
        switch (type) {
        case T_INTEGER:
        case T_BITS:
            data[i] = algol_fread_integer();
            break;
        case T_REAL: {
            double v = algol_fread_real();
            memcpy(&data[i], &v, 8);
            break;
        }
        case T_BOOLEAN:
            data[i] = algol_fread_boolean();
            break;
        default:
            data[i] = (int64_t)(intptr_t)algol_fread_string();
        }
    }
}
