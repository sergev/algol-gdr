//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "source.h"

#include <gtest/gtest.h>

TEST(source, valid_utf8)
{
    EXPECT_EQ(find_invalid_utf8(""), std::string::npos);
    EXPECT_EQ(find_invalid_utf8("'BEGIN' X := A × B ÷ C ↑ 2 ≠ 0 'END'"), std::string::npos);
    EXPECT_EQ(find_invalid_utf8("1.5⏨3"), std::string::npos);
    EXPECT_EQ(find_invalid_utf8("Алгол"), std::string::npos);
}

TEST(source, invalid_utf8)
{
    EXPECT_EQ(find_invalid_utf8("ab\xff"), 2u);
    EXPECT_EQ(find_invalid_utf8("a\xc3"), 1u);        // truncated
    EXPECT_EQ(find_invalid_utf8("\xc0\x80"), 0u);     // overlong
    EXPECT_EQ(find_invalid_utf8("\xed\xa0\x80"), 0u); // surrogate
    EXPECT_EQ(find_invalid_utf8("x\xe2\x88z"), 1u);   // bad continuation
}
