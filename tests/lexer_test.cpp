//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The lexer: numbers, strings, keywords, comments, _common, errors.
//
#include "lexer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>

#include "diag.h"

namespace {

std::vector<Token> tokens(const std::string &text, std::string *messages = nullptr)
{
    std::ostringstream out;
    Diagnostics diag("x.alg", out);
    Lexer lexer(text, diag);
    std::vector<Token> list;
    for (;;) {
        Token t = lexer.next();
        list.push_back(t);
        if (t.sym == Sym::EOP || t.sym == Sym::END_OF_FILE)
            break;
    }
    if (messages)
        *messages = out.str();
    return list;
}

std::vector<Sym> syms(const std::string &text)
{
    auto list = tokens(text);
    std::vector<Sym> s(list.size());
    std::transform(list.begin(), list.end(), s.begin(), [](const Token &t) { return t.sym; });
    return s;
}

} // namespace

TEST(Lexer, numbers)
{
    // Blanks do not separate digits.
    auto t = tokens("1 2, 1.5, .25, 1⏨3, ⏨-2, 3.,_eop");
    ASSERT_EQ(t.size(), 13u);
    EXPECT_EQ(t[0].sym, Sym::INTEGER_CONST);
    EXPECT_EQ(t[0].ival, 12);
    EXPECT_EQ(t[2].rval, 1.5);
    EXPECT_EQ(t[4].rval, 0.25);
    EXPECT_EQ(t[6].rval, 1000.0);
    EXPECT_EQ(t[8].rval, 0.01);
    EXPECT_EQ(t[10].sym, Sym::INTEGER_CONST);
    EXPECT_EQ(t[11].sym, Sym::SEMICOLON);
    EXPECT_EQ(t[12].sym, Sym::EOP);
}

TEST(Lexer, strings)
{
    // `…' nested, the inner quotes kept; ‘…’ for a lone apostrophe.
    std::string messages;
    auto t = tokens("`A b' `x `y' z' ‘'’ `'\n_eop", &messages);
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[0].text, "A b");
    EXPECT_EQ(t[1].text, "x `y' z");
    EXPECT_EQ(t[2].text, "'");
    EXPECT_EQ(t[3].text, "");
    EXPECT_EQ(messages, "");

    tokens("X := `ab\ncd", &messages);
    EXPECT_EQ(messages, "x.alg:1: error: Unterminated string\n");
}

// The strings of Algol-GDR, deprecated: one warning.
TEST(Lexer, old_strings)
{
    std::string messages;
    auto t = tokens("''A b'' '('Ж')' ''X'101'Y'21750'''\n_eop", &messages);
    ASSERT_EQ(t.size(), 4u);
    EXPECT_EQ(t[0].text, "A b");
    EXPECT_EQ(t[1].text, "Ж");
    EXPECT_EQ(t[2].text, "XAY⏨");
    EXPECT_EQ(messages, "x.alg:1:1: warning: Strings in '' are deprecated: write `text'\n");
}

TEST(Lexer, keywords_and_names)
{
    EXPECT_EQ(syms("_begin _integer ALPHA1; _end _eop"),
              (std::vector<Sym>{ Sym::BEGIN, Sym::INTEGER, Sym::IDENT, Sym::SEMICOLON, Sym::END,
                                 Sym::EOP }));
    EXPECT_EQ(syms("_begin _Integer x _END _eop"),
              (std::vector<Sym>{ Sym::BEGIN, Sym::INTEGER, Sym::IDENT, Sym::END, Sym::EOP }));
    // A blank ends a keyword, not a name; '_' starts a keyword.
    auto k = tokens("_real_array noun phrase[1:2]_eop");
    ASSERT_EQ(k.size(), 9u);
    EXPECT_EQ(k[0].sym, Sym::REAL);
    EXPECT_EQ(k[1].sym, Sym::ARRAY);
    EXPECT_EQ(k[2].text, "nounphrase");
    EXPECT_EQ(k[8].sym, Sym::EOP);
    auto t = tokens("AbcDefGhij, Счёт1 _eop");
    EXPECT_EQ(t[0].text, "AbcDefGhij");
    EXPECT_EQ(t[2].text, "Счёт1");
    EXPECT_EQ(
        syms("A := B ** C (/ D /) _le _notequal _eop"),
        (std::vector<Sym>{ Sym::IDENT, Sym::ASSIGN, Sym::IDENT, Sym::POWER, Sym::IDENT,
                           Sym::LBRACKET, Sym::IDENT, Sym::RBRACKET, Sym::LE, Sym::NE, Sym::EOP }));
    EXPECT_EQ(syms("¬A ∨ B ∧ C ⊃ D ≡ E ≤ F ≥ G ≠ H × I ↑ J _eop"),
              syms("_not A _or B _and C _impl D _equiv E\n"
                   "_le F _ge G _ne H * I _power J _eop"));
    EXPECT_EQ(syms("A ÷ B _div C _eop"), (std::vector<Sym>{ Sym::IDENT, Sym::INTDIV, Sym::IDENT,
                                                            Sym::INTDIV, Sym::IDENT, Sym::EOP }));
}

TEST(Lexer, comment_and_after_end)
{
    const std::string text = "_begin _comment ANY TEXT; X _end OF IT; _end + _eop";
    std::string messages;
    tokens(text, &messages);
    EXPECT_EQ(syms(text), (std::vector<Sym>{ Sym::BEGIN, Sym::IDENT, Sym::END, Sym::SEMICOLON,
                                             Sym::END, Sym::EOP }));
    EXPECT_EQ(messages,
              "x.alg:1: warning: Text after _end is skipped up to ';', _end, _else or _eop\n");
}

TEST(Lexer, common)
{
    auto t = tokens("_common /ABC/ X, Y(10), Z(2,3); _eop");
    ASSERT_EQ(t[0].sym, Sym::COMMON);
    const CommonBlock &b = *t[0].common;
    EXPECT_EQ(b.name, "ABC");
    ASSERT_EQ(b.elements.size(), 3u);
    EXPECT_EQ(b.elements[0].name, "X");
    EXPECT_EQ(b.elements[0].size, 1);
    EXPECT_EQ(b.elements[2].dims, (std::vector<int>{ 2, 3 }));
    EXPECT_EQ(b.elements[2].size, 6);
    EXPECT_EQ(t[1].sym, Sym::EOP);
}

TEST(Lexer, errors)
{
    std::string messages;
    auto t = tokens("A % _foo 'begin' 1.X\n''S'9'' _eop", &messages);
    EXPECT_EQ(messages,
              "x.alg:1:3: error: Illegal character '%'\n"
              "x.alg:1:5: error: Unknown keyword _foo\n"
              "x.alg:1:10: error: Write a keyword as _begin, not in quotes\n"
              "x.alg:1:18: error: Malformed number\n"
              "x.alg:2:1: warning: Strings in '' are deprecated: write `text'\n"
              "x.alg:2:1: error: Bad character code in a string\n");
    EXPECT_EQ(t.back().sym, Sym::END_OF_FILE);
}

// The publication form: underlined keywords, `…' strings nested.
TEST(Lexer, publication)
{
    EXPECT_EQ(syms("b̲e̲g̲i̲n̲ x := ab d̲o̲ g̲o̲ t̲o̲ L; i̲f̲ a ≤ b t̲h̲e̲n̲ e̲n̲d̲"),
              syms("_begin x := ab _do _goto L; _if a _le b _then _end"));
    auto t = tokens("b̲e̲g̲i̲n̲ P(`Hello `Algol'', ‘a’) e̲n̲d̲");
    ASSERT_GE(t.size(), 6u);
    EXPECT_EQ(t[3].text, "Hello `Algol'");
    EXPECT_EQ(t[5].text, "a");
    auto n = tokens("b̲e̲g̲i̲n̲ collect garbage s̲t̲e̲p̲ e̲n̲d̲");
    EXPECT_EQ(n[1].text, "collectgarbage");
    EXPECT_EQ(n[2].sym, Sym::STEP);
}
