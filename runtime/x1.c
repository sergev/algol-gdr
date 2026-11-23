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
