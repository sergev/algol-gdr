//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The library of the Electrologica X1, as x1sim has it
// (x1-algol-compiler/simulator): output in the fixed and floating formats,
// symbols by their X1 codes.
//
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "algol.h"
#include "internal.h"

// Symbols by their X1 code (x1-algol-compiler/doc/algol-encoding.md).
static const char *const x1_symbols[128] = {
    "0", "1", "2", "3", "4", "5", "6",  "7",  "8", "9",  "a", "b", "c",  "d", "e", "f", //
    "g", "h", "i", "j", "k", "l", "m",  "n",  "o", "p",  "q", "r", "s",  "t", "u", "v", //
    "w", "x", "y", "z", "⎕", "A", "B",  "C",  "D", "E",  "F", "G", "H",  "I", "J", "K", //
    "L", "M", "N", "O", "P", "Q", "R",  "S",  "T", "U",  "V", "W", "X",  "Y", "Z", "⎕", //
    "+", "-", "×", "/", "÷", "↑", ">",  "≥",  "=", "≤",  "<", "≠", "¬",  "∧", "∨", "⊃", //
    "≡", "⎕", "⎕", "⎕", "⎕", "⎕", "⎕",  ",",  ".", "⏨",  ":", ";", ":=", " ", "⎕", "⎕", //
    "⎕", "⎕", "(", ")", "[", "]", "`",  "'",  "⎕", "⎕",  "⎕", "⎕", "⎕",  "⎕", "⎕", "⎕", //
    "⎕", "⎕", "⎕", "⎕", "⎕", "⎕", "\t", "\n", "⎕", "\"", "?", "⎕", "⎕",  "⎕", "⎕", "⎕", //
};

// Output to a terminal is flushed after each call: prompts and progress.
static void out(const char *s)
{
    static int tty = -1;
    algol_print_string(s);
    if (tty < 0)
        tty = isatty(1);
    if (tty)
        fflush(stdout);
}

void algol_x1_printtext(const char *s)
{
    out(s ? s : "");
}

void algol_x1_prsym(int64_t code)
{
    out(x1_symbols[code & 0177]);
}

void algol_x1_space(int64_t n)
{
    for (; n > 0; n--)
        algol_print_string(" ");
    out("");
}

void algol_x1_tab(void)
{
    out("\t");
}

void algol_x1_nlcr(void)
{
    out("\n");
}

// print: an integer in decimal, a real in 13 digits.
void algol_x1_print_integer(int64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRId64, v);
    out(buf);
}

void algol_x1_print_real(double v)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%.13Lg", (long double)v);
    out(buf);
}

// The exact decimal digits of x > 0, as x = 0.d1d2… × 10^exp; their count.
// Rounding is done on these, half up, so every target prints alike.
enum { MAX_DIGITS = 1200 };
static int exact_digits(double x, char *digits, int *exp)
{
    char buf[MAX_DIGITS + 32];
    int e;
    frexp(x, &e);
    int precision = 20 + abs(e - 53); // enough for the whole expansion
    snprintf(buf, sizeof(buf), "%.*e", precision, x);
    int n         = 0;
    const char *p = buf;
    for (; *p != 'e'; p++)
        if (*p != '.')
            digits[n++] = *p;
    *exp = atoi(p + 1) + 1;
    while (n > 1 && digits[n - 1] == '0')
        n--;
    return n;
}

// Digit k of 0.d1d2…, from 1; zero past the end.
static int digit_at(const char *digits, int count, int k)
{
    return k >= 1 && k <= count ? digits[k - 1] - '0' : 0;
}

// The kept digits rounded half up by the next one; 1 when it carries out.
static int round_up(char *kept, int n, int next)
{
    if (next < 5)
        return 0;
    for (int k = n - 1; k >= 0; k--) {
        if (kept[k] < 9) {
            kept[k]++;
            return 0;
        }
        kept[k] = 0;
    }
    return 1;
}

// FLOT(n, m, x): sign, point, n digits, ⏨, the sign of the exponent and m
// digits of it, leading zeros as blanks; a blank. FLOT(13, 3, x) when n or
// m is out of range, or the exponent does not fit.
void algol_x1_flot(int64_t n, int64_t m, double x)
{
    char buf[64], *p                  = buf;
    char digits[MAX_DIGITS], kept[16] = { 0 };
    if (n < 1 || n > 13 || m < 1 || m > 3)
        n = 13, m = 3;
    char sign = '+';
    if (signbit(x)) {
        sign = '-';
        x    = -x;
    }
    int exponent = 0;
    int count    = x == 0 ? 0 : exact_digits(x, digits, &exponent);
    // As x1sim: with m = 2 the exponent goes to -99, but only to 9.
    if ((m == 1 && (exponent > 9 || exponent < -9)) || (m == 2 && (exponent > 9 || exponent < -99)))
        n = 13, m = 3;
    for (int k = 0; k < n; k++)
        kept[k] = (char)digit_at(digits, count, k + 1);
    if (round_up(kept, (int)n, digit_at(digits, count, (int)n + 1))) {
        kept[0] = 1;
        exponent++;
    }
    *p++ = sign;
    *p++ = '.';
    for (int k = 0; k < n; k++)
        *p++ = (char)('0' + kept[k]);
    p += sprintf(p, "⏨%c", exponent < 0 ? '-' : '+');
    if (exponent < 0)
        exponent = -exponent;
    int zeros = 1; // blanks for the leading zeros
    for (int k = (int)m - 1; k >= 0; k--) {
        int digit = exponent / (k == 2 ? 100 : k == 1 ? 10 : 1) % 10;
        if (k > 0 && digit == 0 && zeros) {
            *p++ = ' ';
        } else {
            *p++  = (char)('0' + digit);
            zeros = 0;
        }
    }
    *p++ = ' ';
    *p   = 0;
    out(buf);
}

// FIXT(n, m, x) and ABSFIXT: n digits before the point, m after, leading
// zeros as blanks; before them a blank, or the sign with FIXT; a blank
// after. FLOT(13, 3, x) when the number or the format does not fit.
static void fixed(int64_t n, int64_t m, double x0, int need_sign)
{
    char buf[64], *p                  = buf;
    char digits[MAX_DIGITS], kept[24] = { 0 };
    double x  = x0;
    char sign = need_sign ? '+' : ' ';
    if (signbit(x)) {
        if (need_sign)
            sign = '-';
        x = -x;
    }
    if (n < 0 || m < 0 || n + m > 21 || n + m < 1) {
        algol_x1_flot(13, 3, x0);
        return;
    }
    int exponent = 0;
    int count    = x == 0 ? 0 : exact_digits(x, digits, &exponent);
    // Digit k of the field has the weight 10^(n-1-k): digit exponent-n+1+k.
    int first = exponent - (int)n + 1;
    for (int k = 0; k < n + m; k++)
        kept[k] = (char)digit_at(digits, count, first + k);
    if (x != 0 && (exponent > n ||
                   round_up(kept, (int)(n + m), digit_at(digits, count, first + (int)(n + m))))) {
        algol_x1_flot(13, 3, x0);
        return;
    }
    *p++       = sign;
    int blanks = 1; // leading zeros as blanks
    for (int k = 0; k < n + m; k++) {
        if (k + 1 == n)
            blanks = 0;
        if (k == n && m > 0)
            *p++ = '.';
        if (kept[k] == 0 && blanks) {
            *p++ = ' ';
        } else {
            *p++   = (char)('0' + kept[k]);
            blanks = 0;
        }
    }
    *p++ = ' ';
    *p   = 0;
    out(buf);
}

void algol_x1_fixt(int64_t n, int64_t m, double x)
{
    fixed(n, m, x, 1);
}

void algol_x1_absfixt(int64_t n, int64_t m, double x)
{
    fixed(n, m, x, 0);
}

//
// Input from stdin. A few bytes can be put back.
//
static unsigned char pushed[8];
static int npushed;
static int stdin_tty = -1;

static int next_byte(void)
{
    return npushed > 0 ? pushed[--npushed] : getchar();
}

static void put_back(int c)
{
    if (c != EOF && npushed < (int)sizeof(pushed))
        pushed[npushed++] = (unsigned char)c;
}

static int interactive(void)
{
    if (stdin_tty < 0)
        stdin_tty = isatty(0);
    return stdin_tty;
}

// The next code point; EOF at the end of the input.
static int32_t next_symbol(void)
{
    int c = next_byte();
    if (c == EOF || c < 0x80)
        return c;
    int n     = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    int32_t v = c & (0x3f >> n);
    while (n-- > 0) {
        c = next_byte();
        if (c == EOF)
            return EOF;
        v = v << 6 | (c & 0x3f);
    }
    return v;
}

// Put back a code point read by next_symbol.
static void put_back_symbol(int32_t v)
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
static void skip_line(void)
{
    int c;
    while ((c = next_byte()) != EOF && c != '\n')
        continue;
}

// A number as C++ streams read it: blanks skipped; sign, digits, point,
// exponent with E, e or ⏨. 0 when there is none.
static int scan_number(double *v)
{
    char buf[128];
    int n = 0;
    int32_t c;
    while ((c = next_symbol()) == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v')
        continue;
    if (c == EOF)
        algol_error("No input");
    if (c == '+' || c == '-') {
        buf[n++] = (char)c;
        c        = next_symbol();
    }
    int digits = 0;
    for (; c >= '0' && c <= '9' && n < 100; c = next_symbol(), digits++)
        buf[n++] = (char)c;
    if (c == '.') {
        buf[n++] = '.';
        for (c = next_symbol(); c >= '0' && c <= '9' && n < 100; c = next_symbol(), digits++)
            buf[n++] = (char)c;
    }
    if (digits > 0 && (c == 'e' || c == 'E' || c == 0x23e8)) {
        buf[n++] = 'e';
        c        = next_symbol();
        if (c == '+' || c == '-') {
            buf[n++] = (char)c;
            c        = next_symbol();
        }
        int exp_digits = 0;
        for (; c >= '0' && c <= '9' && n < 120; c = next_symbol(), exp_digits++)
            buf[n++] = (char)c;
        if (exp_digits == 0)
            digits = 0;
    }
    // As x1sim: the symbol after a number is gone; one that is not a number
    // stays.
    if (c != EOF && digits == 0)
        put_back_symbol(c);
    if (digits == 0)
        return 0;
    buf[n] = 0;
    *v     = strtod(buf, NULL);
    return 1;
}

// read: the next number. On a terminal it asks, and asks again after a bad
// one; otherwise a bad number stops the program.
double algol_x1_read(void)
{
    for (;;) {
        if (interactive())
            out("Enter number: ");
        fflush(stdout);
        double v;
        if (scan_number(&v))
            return v;
        if (!interactive())
            algol_error("Bad input");
        out("Bad input, try again.\n");
        skip_line();
    }
}

// RESYM: the X1 code of the next symbol; 255 at the end of the input.
int64_t algol_x1_resym(void)
{
    fflush(stdout);
    for (;;) {
        int32_t c = next_symbol();
        if (c == EOF)
            return 0377;
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'Z')
            return c - 'A' + 37;
        switch (c) {
        case '\r':
            continue;
        case '\t':
            return 118;
        case '\n':
            return 119;
        case '*':
            return 66;
        case '@':
            return 89;
        case '|':
            return 162;
        case '_':
            return 163;
        }
        for (int k = 64; k < 128; k++)
            if (k != 92 && k != 118 && k != 119 && strcmp(x1_symbols[k], "⎕") != 0) {
                int32_t s = 0;
                // The code point of a symbol of the table.
                const unsigned char *p = (const unsigned char *)x1_symbols[k];
                int len                = *p < 0x80 ? 0 : *p < 0xe0 ? 1 : *p < 0xf0 ? 2 : 3;
                s                      = len == 0 ? *p : *p & (0x3f >> len);
                for (p++; len-- > 0; p++)
                    s = s << 6 | (*p & 0x3f);
                if (s == c)
                    return k;
            }
        algol_error("Bad input character %#o", (unsigned)c);
    }
}

// XEEN(mask): the console switches, read once as a line of binary digits,
// masked.
int64_t algol_x1_xeen(int64_t mask)
{
    static int valid;
    static int64_t switches;
    while (!valid) {
        if (interactive())
            out("\nEnter console switches (up to 27 binary digits)\n> ");
        fflush(stdout);
        char line[256];
        int n = 0, c;
        while ((c = next_byte()) != EOF && c != '\n')
            if (n < (int)sizeof(line) - 1)
                line[n++] = (char)c;
        line[n] = 0;
        if (c == EOF && n == 0)
            algol_error("No input");
        char *end;
        long long v = strtoll(line, &end, 2);
        if (end != line) {
            switches = v;
            valid    = 1;
        } else if (!interactive()) {
            algol_error("Bad input");
        } else {
            out("Bad input, try again.\n");
        }
    }
    return switches & mask;
}

// RANDOM: SplitMix64, 53 bits in [0, 1); SETRANDOM seeds it with the bits of
// a real.
static uint64_t random_state = 0x853c49e6748fea9bULL;

double algol_x1_random(void)
{
    uint64_t z = (random_state += 0x9e3779b97f4a7c15ULL);
    z          = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z          = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z ^= z >> 31;
    return (double)(z >> 11) * 0x1.0p-53;
}

void algol_x1_setrandom(double seed)
{
    memcpy(&random_state, &seed, sizeof(random_state));
}

// TIMEOFDAY: local seconds since midnight; $ALGC_TIMEOFDAY instead when set.
double algol_x1_timeofday(void)
{
    const char *env = getenv("ALGC_TIMEOFDAY");
    if (env)
        return strtod(env, NULL);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t clock = tv.tv_sec;
    struct tm tm;
    localtime_r(&clock, &tm);
    return (tm.tm_hour * 60.0 + tm.tm_min) * 60.0 + tm.tm_sec + tv.tv_usec * 1e-6;
}
