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
    auto p = parse("_begin X := A + B * C ** D _end _eop", diag);
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
    EXPECT_EQ(messages("_begin X := (1; Y := 2 + ; Z := 3\n_end _eop"),
              "x.alg:1: error: Expected ')', found ';'\n"
              "x.alg:1: error: Expected an expression, found ';'\n");
    EXPECT_EQ(messages("_begin _integer X _end _eop"),
              "x.alg:1: error: Expected ';', found _end\n");
    EXPECT_EQ(messages("_begin X := 1;"), "x.alg:1: error: Missing _end\n");
    EXPECT_EQ(messages("_begin X := (1"), "x.alg:1: error: Unexpected end of the file\n");
}

TEST(Parser, procedure_heading)
{
    EXPECT_EQ(messages("_begin _procedure P(A, B); _value C; _real A, A; X := 1;\n"
                       "P(1, 2) _end _eop"),
              "x.alg:1: error: 'C' is not a formal parameter of 'P'\n"
              "x.alg:1: error: Formal parameter 'A' is specified twice\n");
    EXPECT_EQ(messages("_begin _procedure P(A); _value A; ; P(1) _end _eop"),
              "x.alg:1: error: Formal parameter 'A' is called by value but not specified\n");
}
