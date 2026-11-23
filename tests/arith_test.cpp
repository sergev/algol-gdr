//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "arith.h"

#include <gtest/gtest.h>

// Small integers as normalized BESM-6 reals.
static Word real(int n)
{
    int e     = 65;
    int64_t m = int64_t(n) << 39;
    if (n == 0)
        return 0;
    while (m >= (int64_t(1) << 40) || m < -(int64_t(1) << 40)) {
        m >>= 1;
        e++;
    }
    return Word(e) << 41 | (Word(m) & 0'0037'7777'7777'7777);
}

TEST(arith, basic)
{
    EXPECT_EQ(real(1), 04050000000000000u);
    Arith a(real(1));
    a.add(real(1));
    EXPECT_EQ(a.acc, real(2));
    a.mul(real(3));
    EXPECT_EQ(a.acc, real(6));
    a.div(real(3));
    EXPECT_EQ(a.acc, real(2));
    a.sub(real(5));
    EXPECT_EQ(a.acc, real(-3));
    a.rsub(real(4));
    EXPECT_EQ(a.acc, real(7));
}

TEST(arith, integers)
{
    // Integers of the compiler: exponent 064 (2^-12) unnormalized, mode 3.
    Arith a(06400000000000012, Arith::NORM_DISABLE | Arith::ROUND_DISABLE);
    a.add(06400000000000005);
    EXPECT_EQ(a.acc, 06400000000000017u);
}

TEST(arith, overflow)
{
    Arith a(07750000000000000);
    EXPECT_THROW(a.mul(07750000000000000), ArithOverflow);
    Arith b(real(1));
    EXPECT_THROW(b.div(0), std::domain_error);
}
