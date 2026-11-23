//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
#include "ast.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "checker.h"
#include "diag.h"
#include "frontend.h"
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

// _begin _real X; L: X := 1; PRINT(''A'', X) _end
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
    EXPECT_EQ(
        check_messages("_begin _boolean b; outreal(1, b); ininteger(0); outstring(1, 3) _end"),
        "x.alg:1:31: error: Type error in argument 2 of 'outreal': boolean instead of real\n"
        "x.alg:1:35: error: Wrong number of arguments of 'ininteger'\n"
        "x.alg:1:62: error: Type error in argument 2 of 'outstring': integer instead of string\n");
    EXPECT_EQ(check_messages("_begin _integer I; _boolean B;\n"
                             "I := B; J := 1; _goto I\n_end _eop"),
              "x.alg:2:6: error: Type error in the value of ':=': boolean instead of integer\n"
              "x.alg:2:9: error: Undeclared identifier 'J'\n"
              "x.alg:2:23: error: A label is expected in _goto\n");
    EXPECT_EQ(check_messages("_begin _real _procedure F; ; F _end _eop"),
              "x.alg:1:25: warning: Function 'F' does not assign its result\n");
    EXPECT_EQ(check_messages("_begin _procedure P(X); _integer X; X := 1;\n"
                             "_real Y; _boolean Z; P(Y); P(Y + 1); P(Z) _end _eop"),
              "x.alg:2:40: error: Argument 1 of 'P' does not match formal parameter 'X'\n");
}

TEST(Checker, names)
{
    // Names count in full and by case; standard ones in any case.
    EXPECT_EQ(check_messages("_begin _integer counter1, counter2, Counter1;\n"
                             "counter1 := counter2 := Counter1 := 1; COUNTER1 := sqrt(4);\n"
                             "Print(counter1) _end _eop"),
              "x.alg:2:40: error: Undeclared identifier 'COUNTER1'\n");
}

TEST(Checker, scopes_and_conversions)
{
    std::ostringstream err, out;
    Diagnostics diag("x.alg", err);
    auto program = check(parse("_begin _real X; _integer I;\n"
                               "L: X := I + 1; _begin _integer X; X := 2.5 _end;\n"
                               "_goto L _end _eop",
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

// The X1 library: print is X1's in the publication form only.
TEST(Checker, x1_library)
{
    auto dump = [](const std::string &text) {
        std::ostringstream err, out;
        Diagnostics diag("x.a60", err);
        auto program = check(parse(text, diag), diag);
        EXPECT_EQ(err.str(), "");
        print(out, program);
        return out.str();
    };
    std::string text = dump(
        "b̲e̲g̲i̲n̲ i̲n̲t̲e̲g̲e̲r̲ i; r̲e̲a̲l̲ x;\n"
        "x := read + SUM(i, 1, 10, i) + RANDOM + TIMEOFDAY;\n"
        "i := RESYM + EVEN(i) + XEEN(3);\n"
        "print(i, x); FLOT(5, x); PRINTTEXT(`a'); NLCR e̲n̲d̲");
    for (const char *s : { "func: read", "func: sum", "func: random", "func: timeofday",
                           "func: resym", "func: even", "func: xeen", "proc: print", "proc: flot",
                           "proc: printtext", "proc: nlcr" })
        EXPECT_NE(text.find(s), std::string::npos) << s;
    // FLOT(n, x) is FLOT(n, 2, x).
    EXPECT_NE(text.find("{kind: int_const, type: integer, value: 2}"), std::string::npos);
    text = dump("_begin PRINT(1) _end");
    EXPECT_NE(text.find("kind: io"), std::string::npos) << text;

    EXPECT_EQ(check_messages("b̲e̲g̲i̲n̲ r̲e̲a̲l̲ x;\n"
                             "x := SUM(x, 1, 2, x); PRINTTEXT(1); print(`a'); FLOT(1);\n"
                             "SUM; x := EVEN e̲n̲d̲"),
              "x.alg:2:10: error: Type error in argument 1 of 'SUM': real instead of integer\n"
              "x.alg:2:33: error: Type error in argument 1 of 'PRINTTEXT': integer instead of "
              "string\n"
              "x.alg:2:43: error: Type error in argument 1 of 'print': string instead of a "
              "number\n"
              "x.alg:2:49: error: Wrong number of arguments of 'FLOT'\n"
              "x.alg:3:1: error: Illegal use of 'SUM'\n"
              "x.alg:3:11: error: Wrong number of arguments of 'EVEN'\n");
}

// The programs of x1-algol-compiler in the publication form pass the front
// end.
TEST(Ast, x1_examples)
{
    namespace fs = std::filesystem;
    if (!fs::is_directory(X1_EXAMPLES))
        GTEST_SKIP() << "no " << X1_EXAMPLES;
    int n = 0;
    for (const auto &e : fs::directory_iterator(X1_EXAMPLES)) {
        if (e.path().extension() != ".a60")
            continue;
        std::ostringstream err;
        Diagnostics diag(e.path().filename().string(), err);
        front_end(read_source(e.path().string()), diag, nullptr);
        EXPECT_EQ(diag.errors(), 0) << err.str();
        n++;
    }
    EXPECT_GT(n, 0);
}
