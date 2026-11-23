//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The new lexer: the same symbols as the lexer of pass 1 on every program,
// and its own cases.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

#include "diag.h"
#include "golden.h"
#include "lexer.h"
#include "listing.h"
#include "pass1.h"
#include "source.h"
#include "symbols.h"

namespace {

// Symbols of pass 1 by their codes.
const std::map<unsigned, Sym> OLD_CODES = {
    { 001, Sym::IMPL },         { 002, Sym::OR },          { 003, Sym::AND },
    { 004, Sym::NOT },          { 005, Sym::LT },          { 006, Sym::LE },
    { 007, Sym::GE },           { 010, Sym::GT },          { 011, Sym::EQ },
    { 012, Sym::NE },           { 013, Sym::PLUS },        { 014, Sym::MINUS },
    { 015, Sym::TIMES },        { 016, Sym::SLASH },       { 017, Sym::POWER },
    { 020, Sym::LPAREN },       { 021, Sym::LBRACKET },    { 022, Sym::ASSIGN },
    { 026, Sym::GOTO },         { 027, Sym::IF },          { 030, Sym::THEN },
    { 031, Sym::ELSE },         { 032, Sym::FOR },         { 033, Sym::DO },
    { 034, Sym::STEP },         { 035, Sym::WHILE },       { 036, Sym::BEGIN },
    { 037, Sym::END },          { 040, Sym::ARRAY },       { 041, Sym::SWITCH },
    { 042, Sym::PROCEDURE },    { 043, Sym::EQUIV },       { 045, Sym::RPAREN },
    { 046, Sym::RBRACKET },     { 047, Sym::COMMA },       { 050, Sym::COLON },
    { 051, Sym::SEMICOLON },    { 052, Sym::UNTIL },       { 053, Sym::OWN },
    { 054, Sym::BOOLEAN },      { 055, Sym::INTEGER },     { 056, Sym::REAL },
    { 057, Sym::STRING },       { 060, Sym::LABEL },       { 061, Sym::VALUE },
    { 062, Sym::READ },         { 063, Sym::PRINT },       { 064, Sym::PUNCH },
    { 065, Sym::WRITE },        { 066, Sym::DECODE },      { 067, Sym::ENCODE },
    { 070, Sym::REWIND },       { 071, Sym::BACKSPACE },   { 072, Sym::ENDFILE },
    { 073, Sym::ALGOL },        { 074, Sym::CODE },        { 075, Sym::EOP },
    { 076, Sym::TRUE },         { 077, Sym::FALSE },       { 0101, Sym::BITS },
    { 0, Sym::IDENT },          { 023, Sym::REAL_CONST },  { 024, Sym::INTEGER_CONST },
    { 025, Sym::STRING_CONST }, { 0105, Sym::BITS_CONST },
};

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

class NewLexer : public ::testing::TestWithParam<std::string> {};

TEST_P(NewLexer, same_as_pass1)
{
    const std::string &name = GetParam();
    Context ctx;
    ctx.source = read_source(path(name, ".alg"));
    load_options(ctx.options, name);
    Pass1 old(ctx);
    std::ostringstream messages;
    Diagnostics diag(name, messages);
    Lexer lexer(ctx.source, diag);
    for (size_t i = 0;; i++) {
        try {
            old.lex();
        } catch (const Pass1::SyntaxError &e) {
            GTEST_SKIP() << "pass 1: syntax error " << e.number << " at symbol " << i;
        } catch (const Pass1::NoEop &) {
            GTEST_SKIP() << "pass 1: no 'EOP'";
        }
        Token t = lexer.next();
        while (t.sym == Sym::COMMON) // pass 1 reads it in its lexer
            t = lexer.next();
        unsigned code = old.symbol();
        auto it       = OLD_CODES.find(code);
        ASSERT_NE(it, OLD_CODES.end()) << "symbol " << i << ": code " << code;
        ASSERT_EQ(to_string(t.sym), to_string(it->second))
            << "symbol " << i << " at line " << t.line;
        unsigned slot = (old.symbol_word() >> 12) & 07777;
        Word w        = slot < Tables::SLOTS ? ctx.tables.name(slot) : 0;
        switch (t.sym) {
        case Sym::IDENT:
            EXPECT_EQ(t.key(), rtrim(listing_text(&w, 0, 6))) << "symbol " << i;
            break;
        case Sym::INTEGER_CONST:
            EXPECT_EQ(t.ival, Symbols::to_integer(w)) << "symbol " << i;
            break;
        case Sym::REAL_CONST:
            EXPECT_NEAR(t.rval, Symbols::to_real(w), std::fabs(t.rval) * 1e-11) << "symbol " << i;
            break;
        case Sym::BITS_CONST:
            EXPECT_EQ(t.bits, w) << "symbol " << i;
            break;
        default:
            break;
        }
        if (t.sym == Sym::EOP)
            break;
    }
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, NewLexer, ::testing::ValuesIn(program_names()), param_name);

TEST(NewLexer, numbers)
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

TEST(NewLexer, strings)
{
    auto t = tokens("''A B'' '('C')' ''X'101'Y'' 'EOP'");
    ASSERT_EQ(t.size(), 4u);
    EXPECT_EQ(t[0].text, "A B");
    EXPECT_EQ(t[1].text, "C");
    EXPECT_EQ(t[2].text, "XAY");
}

TEST(NewLexer, keywords_and_names)
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

TEST(NewLexer, comment_and_after_end)
{
    const std::string text = "'BEGIN' 'COMMENT' ANY TEXT; X 'END' OF IT; 'END' + 'EOP'";
    std::string messages;
    tokens(text, &messages);
    EXPECT_EQ(syms(text), (std::vector<Sym>{ Sym::BEGIN, Sym::IDENT, Sym::END, Sym::SEMICOLON,
                                             Sym::END, Sym::EOP }));
    EXPECT_EQ(messages,
              "x.alg:1: warning: Text after 'END' is skipped up to ';', 'END', 'ELSE' or 'EOP'\n");
}

TEST(NewLexer, common)
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

TEST(NewLexer, errors)
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
