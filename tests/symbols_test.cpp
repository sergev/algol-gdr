//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The tables of the front end, decoded after pass 2, must print as NAME.sym.
// With ALGC_UPDATE_GOLDENS set, NAME.sym is written instead.
//
#include "symbols.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "golden.h"
#include "passes.h"
#include "source.h"
#include "util.h"

class SymbolsTest : public ::testing::TestWithParam<std::string> {};

TEST_P(SymbolsTest, text)
{
    const std::string &name = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    ctx.source = read_source(path(name, ".alg"));
    auto s     = run_pass(1, ctx, {});
    if (ctx.finished)
        GTEST_SKIP() << "pass 1 stopped";
    run_pass(2, ctx, s);
    std::ostringstream out;
    Symbols(ctx).print(out);
    if (std::getenv("ALGC_UPDATE_GOLDENS")) {
        std::ofstream(path(name, ".sym")) << out.str();
        return;
    }
    EXPECT_EQ(out.str(), file_contents(path(name, ".sym")));
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, SymbolsTest, ::testing::ValuesIn(program_names()), param_name);

TEST(Symbols, numbers)
{
    EXPECT_EQ(Symbols::to_integer(06400000000000005), 5);
    EXPECT_EQ(Symbols::to_integer(06437777777777777), -1);
    EXPECT_EQ(Symbols::to_real(04050000000000000), 1.0);
    EXPECT_EQ(Symbols::to_real(04010000000000000), 0.5);
    EXPECT_EQ(Symbols::to_real(04114000000000000), 3.0);
    EXPECT_EQ(Symbols::to_integer(04114000000000000), 3);
}

static Context compiled(const std::string &text)
{
    Context ctx;
    ctx.source = text;
    auto s     = run_pass(1, ctx, {});
    run_pass(2, ctx, s);
    return ctx;
}

TEST(Symbols, formals)
{
    Context ctx = compiled("'BEGIN' 'REAL' 'PROCEDURE' F(A, B, C); 'VALUE' B;\n"
                           "'INTEGER' A, B; 'REAL' 'ARRAY' C; F := A + B + C[1];\n"
                           "PRINT(F(1, 2, ''X''))\n'END'\n'EOP'\n");
    Symbols sym(ctx);
    unsigned f = 0;
    for (auto &[addr, d] : sym.decls)
        if (d.name == "F")
            f = addr;
    ASSERT_NE(f, 0u);
    EXPECT_EQ(sym.decls[f].type, Type::REAL);
    EXPECT_EQ(sym.decls[f].kind, Kind::PROCEDURE);
    EXPECT_EQ(sym.decls[f].count, 3u);
    auto formals = sym.formals(f);
    ASSERT_EQ(formals.size(), 3u);
    const Decl &a = sym.decls[formals[0]], &b = sym.decls[formals[1]], &c = sym.decls[formals[2]];
    EXPECT_EQ(a.name, "A");
    EXPECT_EQ(a.mode, Mode::NAME);
    EXPECT_EQ(a.type, Type::INTEGER);
    EXPECT_EQ(b.name, "B");
    EXPECT_EQ(b.mode, Mode::VALUE);
    EXPECT_EQ(c.name, "C");
    EXPECT_EQ(c.kind, Kind::ARRAY);
    EXPECT_EQ(c.mode, Mode::NAME);
    EXPECT_EQ(sym.string(01634), "X");
}
