//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_SYNTAX_H
#define ALGOL_SYNTAX_H

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ast.h"
#include "lexer.h"

//
// The syntax tree of a program, as the parser reads it: names are not
// resolved, expressions have no types. The checker makes the AST from it.
// Its grammar: doc/algol60.asdl.
//
namespace syn {

struct Expr;
struct Stmt;
struct Decl;
using ExprPtr  = std::unique_ptr<Expr>;
using StmtPtr  = std::unique_ptr<Stmt>;
using DeclPtr  = std::unique_ptr<Decl>;
using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using DeclList = std::vector<DeclPtr>;

// A name where it is written.
struct Ident {
    std::string name;
    int line{};
};

namespace expr {
struct Int {
    int64_t value{};
};
struct Real {
    double value{};
};
struct Bits {
    uint64_t value{};
};
struct Bool {
    bool value{};
};
struct String {
    std::string value;
};
struct Name {
    std::string name;
};
// a[i, j]: an element of an array, of a switch, a character of a string.
struct Index {
    std::string name;
    ExprList subscripts;
};
// s[i:j]
struct Slice {
    std::string name;
    ExprPtr first, last;
};
struct Call {
    std::string name;
    ExprList args;
};
struct Unary {
    Sym op{}; // PLUS, MINUS, NOT
    ExprPtr operand;
};
struct Binary {
    Sym op{};
    ExprPtr left, right;
};
struct If {
    ExprPtr condition, then_expr, else_expr;
};
} // namespace expr

struct Expr {
    std::variant<expr::Int, expr::Real, expr::Bits, expr::Bool, expr::String, expr::Name,
                 expr::Index, expr::Slice, expr::Call, expr::Unary, expr::Binary, expr::If>
        node;
    int line{};
};

struct ForElement {
    enum Kind { VALUE, STEP_UNTIL, WHILE } kind{};
    ExprPtr value; // or the initial value
    ExprPtr step;  // or the condition of WHILE
    ExprPtr until;
};

struct IoItem {
    ExprPtr expr;                     // or, for a FOR item:
    std::string var;                  //   the control variable,
    std::vector<ForElement> elements; //   its elements,
    std::vector<IoItem> items;        //   the items
};

namespace stmt {
struct Block {
    DeclList decls;
    StmtList body;
};
struct Compound {
    StmtList body;
};
struct Labeled {
    Ident label;
    StmtPtr stmt;
};
struct Dummy {};
struct Error {}; // skipped after a syntax error
struct Assign {
    ExprList targets; // Name or Index
    ExprPtr value;
};
struct Goto {
    ExprPtr target;
};
struct If {
    ExprPtr condition;
    StmtPtr then_stmt;
    StmtPtr else_stmt; // or null
};
struct For {
    Ident var;
    std::vector<ForElement> elements;
    StmtPtr body;
};
struct Call {
    ExprPtr call; // Name or Call
};
// 'READ', 'PRINT', 'WRITE' and the like.
struct Io {
    Sym op{};
    ExprList params;
    std::vector<IoItem> items;
};
} // namespace stmt

struct Stmt {
    std::variant<stmt::Block, stmt::Compound, stmt::Labeled, stmt::Dummy, stmt::Error, stmt::Assign,
                 stmt::Goto, stmt::If, stmt::For, stmt::Call, stmt::Io>
        node;
    ast::SrcLoc loc;
};

struct BoundPair {
    ExprPtr lower, upper;
};

struct Spec {
    ast::Spec::Kind kind{};
    ast::Type type{ ast::Type::NONE };
    std::vector<Ident> names;
};

namespace decl {
struct Vars {
    ast::Type type{};
    bool own{};
    std::vector<Ident> names;
};
// Segments: arrays with the same bounds.
struct Arrays {
    struct Segment {
        std::vector<Ident> names;
        std::vector<BoundPair> bounds;
    };
    ast::Type type{};
    bool own{};
    std::vector<Segment> segments;
};
struct Switch {
    Ident name;
    ExprList elements;
};
struct Procedure {
    ast::Type type{ ast::Type::NONE };
    Ident name;
    std::vector<Ident> formals;
    std::vector<Ident> values;
    std::vector<Spec> specs;
    StmtPtr body;
};
struct Common {
    std::shared_ptr<CommonBlock> block;
};
} // namespace decl

struct Decl {
    std::variant<decl::Vars, decl::Arrays, decl::Switch, decl::Procedure, decl::Common> node;
    int line{};
};

// A 'CODE' or 'ALGOL' procedure before the program.
struct External {
    Ident name;
    ast::Type type{ ast::Type::NONE };
    ast::External::Kind kind{};
};

struct Program {
    std::vector<External> externals;
    DeclList decls; // 'COMMON' before the program
    StmtPtr body;   // the program: a block or compound statement, maybe labeled;
    DeclPtr module; // or a procedure compiled alone
};

} // namespace syn

#endif
