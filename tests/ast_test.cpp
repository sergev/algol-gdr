//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
#include "ast.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "checker.h"
#include "diag.h"
#include "golden.h"
#include "parser.h"
#include "source.h"
#include "util.h"

using namespace ast;

//
// The new front end on each program: its messages are NAME.err, and the AST
// of a program without errors prints as NAME.ast. A golden that is absent
// means none. With ALGC_UPDATE_GOLDENS set, the goldens are written instead.
//
class AstGolden : public ::testing::TestWithParam<std::string> {};

static void golden(const std::string &file, const std::string &text)
{
    if (std::getenv("ALGC_UPDATE_GOLDENS")) {
        if (text.empty())
            std::remove(file.c_str());
        else
            std::ofstream(file) << text;
        return;
    }
    std::ifstream in(file);
    EXPECT_EQ(text, in ? file_contents(file) : "") << file;
}

TEST_P(AstGolden, yaml)
{
    const std::string &name = GetParam();
    std::ostringstream err, out;
    Diagnostics diag(name + ".alg", err);
    auto tree    = parse(read_source(path(name, ".alg")), diag);
    auto program = check(tree, diag);
    if (!diag.errors())
        print(out, program);
    golden(path(name, ".err"), err.str());
    golden(path(name, ".ast"), out.str());
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

static std::string check_messages(const std::string &text)
{
    std::ostringstream err;
    Diagnostics diag("x.alg", err);
    check(parse(text, diag), diag);
    return err.str();
}

TEST(Checker, messages)
{
    EXPECT_EQ(check_messages("'BEGIN' 'INTEGER' I; 'BOOLEAN' B;\n"
                             "I := B; J := 1; 'GOTO' I\n'END' 'EOP'"),
              "x.alg:2: error: Type error in the value of ':=': boolean instead of integer\n"
              "x.alg:2: error: Undeclared identifier 'J'\n"
              "x.alg:2: error: A label is expected in 'goto'\n");
    EXPECT_EQ(check_messages("'BEGIN' 'REAL' 'PROCEDURE' F; ; F 'END' 'EOP'"),
              "x.alg:1: error: Function 'F' does not assign its result\n");
    EXPECT_EQ(check_messages("'BEGIN' 'PROCEDURE' P(X); 'INTEGER' X; X := 1;\n"
                             "'REAL' Y; P(Y); P(Y + 1) 'END' 'EOP'"),
              "x.alg:2: error: Argument 1 of 'P' does not match formal parameter 'X'\n");
}

TEST(Checker, names)
{
    // Names count in full and by case; standard ones in any case.
    EXPECT_EQ(check_messages("'begin' 'integer' counter1, counter2, Counter1;\n"
                             "counter1 := counter2 := Counter1 := 1; COUNTER1 := sqrt(4);\n"
                             "Print(counter1) 'end' 'eop'"),
              "x.alg:2: error: Undeclared identifier 'COUNTER1'\n");
}

TEST(Checker, scopes_and_conversions)
{
    std::ostringstream err, out;
    Diagnostics diag("x.alg", err);
    auto program = check(parse("'BEGIN' 'REAL' X; 'INTEGER' I;\n"
                               "L: X := I + 1; 'BEGIN' 'INTEGER' X; X := 2.5 'END';\n"
                               "'GOTO' L 'END' 'EOP'",
                               diag),
                         diag);
    ASSERT_EQ(err.str(), "");
    print(out, program);
    const std::string text = out.str();
    // X := I + 1 converted to real; the inner X rounded.
    EXPECT_NE(text.find("conv: int_to_real"), std::string::npos) << text;
    EXPECT_NE(text.find("conv: real_to_int"), std::string::npos) << text;
    EXPECT_NE(text.find("{kind: label, line: 2, name: \"L\", id: 3}"), std::string::npos) << text;
    EXPECT_NE(text.find("{kind: var, line: 2, name: \"X\", id: 4, type: integer}"),
              std::string::npos)
        << text;
}
