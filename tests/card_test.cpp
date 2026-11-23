//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "card.h"

#include <gtest/gtest.h>

TEST(card, koi7)
{
    EXPECT_EQ(line_to_koi7("'begin' x := 1"), "'BEGIN' X := 1");
    EXPECT_EQ(line_to_koi7("Привет"), "pPiBET");
    EXPECT_EQ(line_to_koi7("a ≠ b ≤ c ≥ d"), "A \030 B \016 C \017 D");
    EXPECT_EQ(line_to_koi7("¬a ∧ b ∨ c ⊃ d ≡ e"), "\037A & B \036 C \034 D \035 E");
    EXPECT_EQ(line_to_koi7("1.5⏨-3 2@4 x×y x↑2"), "1.5\027-3 2@4 X\006Y X\0262");
    EXPECT_EQ(line_to_koi7("′BEGIN′"), "'BEGIN'");
    EXPECT_EQ(line_to_koi7(std::string(100, 'A')).size(), CARD_COLUMNS);
}
