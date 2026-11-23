//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Exponentiation, after the original's A/IHOCHI, A/RHOCHI and POWER.
//
#include <math.h>

#include "algol.h"

// integer ↑ integer: repeated multiplication.
int64_t algol_ipow(int64_t base, int64_t exp)
{
    if (exp < 0)
        algol_error("Negative exponent %lld of an integer", (long long)exp);
    if (base == 0 && exp == 0)
        algol_error("0 ↑ 0");
    int64_t r = 1;
    while (exp > 0) {
        if (exp & 1)
            r *= base;
        base *= base;
        exp >>= 1;
    }
    return r;
}

// real ↑ integer: repeated multiplication; a negative exponent divides
// into 1.0.
double algol_rpow(double base, int64_t exp)
{
    if (base == 0 && exp == 0)
        algol_error("0 ↑ 0");
    unsigned long long n = exp < 0 ? -(unsigned long long)exp : (unsigned long long)exp;
    double r = 1.0, b = base;
    while (n > 0) {
        if (n & 1)
            r *= b;
        b *= b;
        n >>= 1;
    }
    return exp < 0 ? 1.0 / r : r;
}

// any ↑ real: x↑0 = 1; an integral exponent of a negative base takes its
// sign from the parity; otherwise exp(b·ln a).
double algol_power(double x, double y)
{
    if (y == 0) {
        if (x == 0)
            algol_error("0 ↑ 0");
        return 1.0;
    }
    if (x < 0) {
        if (y != floor(y))
            algol_error("Negative number %g to a fractional power", x);
        double r = exp(y * log(-x));
        return fmod(y, 2.0) != 0 ? -r : r;
    }
    if (x == 0) {
        if (y < 0)
            algol_error("0 to a negative power");
        return 0;
    }
    return exp(y * log(x));
}

// bits ↑ n: shifted left by n, right when n < 0.
int64_t algol_bits_shift(int64_t bits, int64_t n)
{
    uint64_t b = (uint64_t)bits;
    if (n >= 64 || n <= -64)
        return 0;
    return (int64_t)(n >= 0 ? b << n : b >> -n);
}

double algol_sqrt(double x)
{
    if (x < 0)
        algol_error("Square root of a negative number %g", x);
    return sqrt(x);
}

double algol_ln(double x)
{
    if (x <= 0)
        algol_error("Logarithm of a number not above 0: %g", x);
    return log(x);
}
