//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The lexer: numbers, strings, keywords, comments, 'COMMON', errors.
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
    // Blanks do not separate symbols.
    auto t = tokens("1 2, 1.5, .25, 1⏨3, ⏨-2, 3.,'EOP'");
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
    auto t = tokens("''A B'' '('C')' ''X'101'Y'' 'EOP'");
    ASSERT_EQ(t.size(), 4u);
    EXPECT_EQ(t[0].text, "A B");
    EXPECT_EQ(t[1].text, "C");
    EXPECT_EQ(t[2].text, "XAY");
}

TEST(Lexer, keywords_and_names)
{
    EXPECT_EQ(syms("'BEGIN' 'INTEGER' ALPHA1; 'END' 'EOP'"),
              (std::vector<Sym>{ Sym::BEGIN, Sym::INTEGER, Sym::IDENT, Sym::SEMICOLON, Sym::END,
                                 Sym::EOP }));
    auto t = tokens("ABCDEFGH 'EOP'");
    EXPECT_EQ(t[0].text, "ABCDEFGH");
    EXPECT_EQ(t[0].key(), "ABCDEF");
    EXPECT_EQ(
        syms("A := B ** C (/ D /) 'LE' 'NOTEQUAL' 'EOP'"),
        (std::vector<Sym>{ Sym::IDENT, Sym::ASSIGN, Sym::IDENT, Sym::POWER, Sym::IDENT,
                           Sym::LBRACKET, Sym::IDENT, Sym::RBRACKET, Sym::LE, Sym::NE, Sym::EOP }));
    EXPECT_EQ(syms("¬A ∨ B ∧ C ⊃ D ≡ E ≤ F ≥ G ≠ H × I ↑ J 'EOP'"),
              syms("'NOT'A 'OR' B 'AND' C 'IMPL' D 'EQUIV' E\n"
                   "'LE' F 'GE' G 'NE' H * I 'POWER' J 'EOP'"));
}

TEST(Lexer, comment_and_after_end)
{
    const std::string text = "'BEGIN' 'COMMENT' ANY TEXT; X 'END' OF IT; 'END' + 'EOP'";
    std::string messages;
    tokens(text, &messages);
    EXPECT_EQ(syms(text), (std::vector<Sym>{ Sym::BEGIN, Sym::IDENT, Sym::END, Sym::SEMICOLON,
                                             Sym::END, Sym::EOP }));
    EXPECT_EQ(messages,
              "x.alg:1: warning: Text after 'END' is skipped up to ';', 'END', 'ELSE' or 'EOP'\n");
}

TEST(Lexer, common)
{
    auto t = tokens("'COMMON' /ABC/ X, Y(10), Z(2,3); 'EOP'");
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
    auto t = tokens("A % 'FOO' 1.X\n''S'9'' 'EOP'", &messages);
    EXPECT_EQ(messages,
              "x.alg:1: error: Illegal character '%'\n"
              "x.alg:1: error: Unknown keyword 'FOO'\n"
              "x.alg:1: error: Malformed number\n"
              "x.alg:2: error: Bad character code in a string\n");
    EXPECT_EQ(t.back().sym, Sym::END_OF_FILE);
}
