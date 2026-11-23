//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
#include "ast.h"

#include <gtest/gtest.h>

#include <sstream>

using namespace ast;

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
