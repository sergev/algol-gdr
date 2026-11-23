//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "card.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "golden.h"
#include "source.h"

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

TEST(card, words)
{
    Card card = make_card("'eop'");
    EXPECT_EQ(card[0], 01164251724023440u); // 'EOP' and a blank
    EXPECT_EQ(card[1], 01002004010020040u);
    EXPECT_EQ(card[13], 01002000000000000u);
}

// UTF-8 text to ISO words; 0200-0277 alone are format escapes.
TEST(card, iso_words)
{
    EXPECT_EQ(iso_word("HELLO,"), 02204251423047454u);
    EXPECT_EQ(iso_word("′**′"), 01162505211600000u);
    auto words = iso_words("TWICE \200′\202");
    ASSERT_EQ(words.size(), 2u);
    EXPECT_EQ(words[0], 02505351120642440u);
    EXPECT_EQ(words[1], 04002360200000000u);
}
