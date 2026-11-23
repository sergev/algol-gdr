//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The parser on every program: its listing is that of the original up to
// the messages there; its messages are NAME.err (none when the file is
// absent). With ALGC_UPDATE_GOLDENS set, NAME.err is written instead.
//
#include "parser.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "diag.h"
#include "golden.h"
#include "listing.h"
#include "source.h"
#include "util.h"

class ParserTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ParserTest, listing_and_messages)
{
    const std::string &name = GetParam();
    std::ostringstream lst, err;
    Listing listing(lst);
    listing.print("A L G O L - Г Д P");
    listing.print("    (7.01.82)");
    listing.blank();
    Diagnostics diag(name + ".alg", err);
    parse(read_source(path(name, ".alg")), diag, &listing);

    // The cards up to the first message of the original.
    std::string want = file_contents(path(name, ".lst"));
    std::string got  = lst.str();
    size_t n         = 0;
    while (n < got.size() && n < want.size() && got[n] == want[n])
        n++;
    std::string rest = want.substr(n);
    if (n < got.size()) {
        EXPECT_TRUE(rest.empty() || rest.rfind("     ", 0) != 0) << "listing differs at:\n"
                                                                 << got.substr(n, 200);
    }

    const std::string err_file = path(name, ".err");
    if (std::getenv("ALGC_UPDATE_GOLDENS")) {
        if (err.str().empty())
            std::remove(err_file.c_str());
        else
            std::ofstream(err_file) << err.str();
        return;
    }
    std::ifstream in(err_file);
    EXPECT_EQ(err.str(), in ? file_contents(err_file) : "");
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, ParserTest, ::testing::ValuesIn(program_names()), param_name);

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
    auto p = parse("'BEGIN' X := A + B * C ** D 'END' 'EOP'", diag);
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
    EXPECT_EQ(messages("'BEGIN' X := (1; Y := 2 + ; Z := 3\n'END' 'EOP'"),
              "x.alg:1: error: Expected ')', found ';'\n"
              "x.alg:1: error: Expected an expression, found ';'\n");
    EXPECT_EQ(messages("'BEGIN' 'INTEGER' X 'END' 'EOP'"),
              "x.alg:1: error: Expected ';', found 'END'\n");
    EXPECT_EQ(messages("'BEGIN' X := 1;"),
              "x.alg:1: error: Missing 'END'\n"
              "x.alg:1: error: Unexpected end of the file, 'EOP' "
              "is missing\n");
}

TEST(Parser, procedure_heading)
{
    EXPECT_EQ(messages("'BEGIN' 'PROCEDURE' P(A, B); 'VALUE' C; 'REAL' A, A; X := 1;\n"
                       "P(1, 2) 'END' 'EOP'"),
              "x.alg:1: error: 'C' is not a formal parameter of 'P'\n"
              "x.alg:1: error: Formal parameter 'A' is specified twice\n"
              "x.alg:1: error: Formal parameter 'B' is not specified\n");
}
