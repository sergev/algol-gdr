//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The parser: precedence, recovery, procedure headings.
//
#include "parser.h"

#include <gtest/gtest.h>

#include <sstream>

#include "diag.h"

static std::string messages(const std::string &text)
{
    std::ostringstream err;
    Diagnostics diag("x.alg", err);
    parse(text, diag);
    return err.str();
}

TEST(Parser, precedence)
{
    std::ostringstream err;
    Diagnostics diag("x.alg", err);
    auto p = parse("_begin X := A + B * C ** D _end", diag);
    ASSERT_EQ(err.str(), "");
    auto &c = std::get<syn::stmt::Compound>(p.body->node);
    auto &a = std::get<syn::stmt::Assign>(c.body[0]->node);
    auto &e = std::get<syn::expr::Binary>(a.value->node);
    EXPECT_EQ(e.op, Sym::PLUS);
    auto &r = std::get<syn::expr::Binary>(e.right->node);
    EXPECT_EQ(r.op, Sym::TIMES);
    EXPECT_EQ(std::get<syn::expr::Binary>(r.right->node).op, Sym::POWER);
}

TEST(Parser, recovery)
{
    EXPECT_EQ(messages("_begin X := (1; Y := 2 + ; Z := 3\n_end"),
              "x.alg:1:15: error: Expected ')', found ';'\n"
              "x.alg:1:26: error: Expected an expression, found ';'\n");
    EXPECT_EQ(messages("_begin _integer X _end"), "x.alg:1:19: error: Expected ';', found _end\n");
    EXPECT_EQ(messages("_begin X := 1;"), "x.alg:1:14: error: Missing _end\n");
    EXPECT_EQ(messages("_begin X := (1"), "x.alg:1:14: error: Unexpected end of the file\n");
}

// _eop, which the original needed, ends the input with an error.
// A lexical error is not reported again; a stray _end or _else before the
// program is reported once; a quoted keyword is read as the keyword.
TEST(Parser, lexical_errors)
{
    EXPECT_EQ(messages("_begin X := 3 # 4; Y := 1 _end"),
              "x.alg:1:15: error: Illegal character '#'\n");
    EXPECT_EQ(messages("_end _else _begin X := 1 _end"),
              "x.alg:1:1: error: Expected the program, found _end\n"
              "x.alg:1:6: error: Expected the program, found _else\n");
    EXPECT_EQ(messages("'BEGIN' X := 1 'END'"),
              "x.alg:1:1: error: Write a keyword as _BEGIN, not in quotes\n"
              "x.alg:1:16: error: Write a keyword as _END, not in quotes\n");
}

TEST(Parser, eop)
{
    EXPECT_EQ(messages("_begin X := 1 _end\n_eop\n"),
              "x.alg:2:1: error: _eop is not used: the program ends at the end of the file\n");
    EXPECT_EQ(messages("_begin X := 1; e̲o̲p̲ Y := 2 _end"),
              "x.alg:1:16: error: _eop is not used: the program ends at the end of the file\n"
              "x.alg:1:16: error: Missing _end\n");
}

TEST(Parser, procedure_heading)
{
    EXPECT_EQ(messages("_begin _procedure P(A, B); _value C; _real A, A; X := 1;\n"
                       "P(1, 2) _end"),
              "x.alg:1:35: error: 'C' is not a formal parameter of 'P'\n"
              "x.alg:1:47: error: Formal parameter 'A' is specified twice\n");
    EXPECT_EQ(messages("_begin _procedure P(A); _value A; ; P(1) _end"),
              "x.alg:1:32: error: Formal parameter 'A' is called by value but not specified\n");
}
