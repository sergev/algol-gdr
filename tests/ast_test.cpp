//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
#include "ast.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "ast_builder.h"
#include "golden.h"
#include "passes.h"
#include "source.h"
#include "util.h"

using namespace ast;

//
// The AST of each program that compiles must print as NAME.ast. With
// ALGC_UPDATE_GOLDENS set, NAME.ast is written instead.
//
class AstGolden : public ::testing::TestWithParam<std::string> {};

TEST_P(AstGolden, yaml)
{
    const std::string &name = GetParam();
    Context ctx;
    load_options(ctx.options, name);
    ctx.source = read_source(path(name, ".alg"));
    Stream stream;
    if (!compile(ctx, stream))
        GTEST_SKIP() << "errors";
    std::ostringstream out;
    print(out, build_ast(ctx, stream));
    if (std::getenv("ALGC_UPDATE_GOLDENS")) {
        std::ofstream(path(name, ".ast")) << out.str();
        return;
    }
    EXPECT_EQ(out.str(), file_contents(path(name, ".ast")));
}

static std::string param_name(const ::testing::TestParamInfo<std::string> &p)
{
    return p.param;
}

INSTANTIATE_TEST_SUITE_P(algol, AstGolden, ::testing::ValuesIn(program_names()), param_name);

// 'BEGIN' 'REAL' X; L: X := 1; PRINT(''A'', X) 'END'
TEST(Ast, print)
{
    Program p;
    p.externals.push_back(External{ "F", 1, External::CODE });

    stmt::Block b;
    b.decls.push_back(make_decl(SrcLoc{}, decl::Var{ "X", 2, Type::REAL }));
    b.decls.push_back(make_decl(SrcLoc{}, decl::Label{ "L", 3 }));

    stmt::Assign a;
    a.targets.push_back(left::Var{ 2 });
    a.value = make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL,
                                                   make_expr(Type::INTEGER, expr::IntConst{ 1 }) });
    b.body.push_back(
        make_stmt(SrcLoc{ 1, 2 }, stmt::Labeled{ 3, make_stmt(SrcLoc{ 1, 2 }, std::move(a)) }));

    stmt::Io io;
    io.op = IoOp::PRINT;
    io.items.push_back(
        IoElement{ io::Item{ make_expr(Type::STRING, expr::StringConst{ "A\"" }) } });
    io.items.push_back(IoElement{ io::Item{ make_expr(Type::REAL, expr::Var{ 2 }) } });
    io.items.push_back(IoElement{ io::NewLine{} });
    b.body.push_back(make_stmt(SrcLoc{ 1, 3 }, std::move(io)));
    p.block = make_stmt(SrcLoc{ 1, 1 }, std::move(b));

    std::ostringstream out;
    print(out, p);
    EXPECT_EQ(out.str(), R"(externals:
  - {kind: code, name: "F", id: 1}
block:
  kind: block
  loc: 1/1
  decls:
    - {kind: var, name: "X", id: 2, type: real}
    - {kind: label, name: "L", id: 3}
  body:
    - kind: labeled
      loc: 1/2
      label: 3
      name: "L"
      stmt:
        kind: assign
        loc: 1/2
        targets:
          - {kind: var, decl: 2, name: "X"}
        value:
          kind: convert
          type: real
          conv: int_to_real
          expr: {kind: int_const, type: integer, value: 1}
    - kind: io
      loc: 1/3
      op: print
      params: []
      items:
        - kind: item
          expr: {kind: string_const, type: string, value: "A\""}
        - kind: item
          expr: {kind: var, type: real, decl: 2, name: "X"}
        - {kind: new_line}
)");
}

TEST(Ast, real_text)
{
    Program p;
    stmt::Block b;
    stmt::Goto g;
    g.target = make_expr(Type::REAL, expr::RealConst{ 2 });
    b.body.push_back(make_stmt(SrcLoc{ 1, 1 }, std::move(g)));
    p.block = make_stmt(SrcLoc{}, std::move(b));
    std::ostringstream out;
    print(out, p);
    EXPECT_NE(out.str().find("value: 2.0}"), std::string::npos) << out.str();
}
