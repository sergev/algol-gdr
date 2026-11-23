//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_AST_H
#define ALGOL_AST_H

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

//
// AST of algc, after doc/ast.asdl. A sum type is a std::variant of the
// structs of its constructors, in the namespace named after it.
// Declarations have ids unique in the program; uses refer to them.
//
namespace ast {

struct SrcLoc {
    int line{};
    int stmt{};
};

enum class Type { INTEGER, REAL, BOOLEAN, STRING, BITS, NONE };

enum class StdFunc { ABS, SIGN, SQRT, SIN, COS, ARCTAN, LN, EXP, ENTIER, LENGTH };
enum class StdProc { STOP, LOAD, LOADGO, ASSIGN, ATOS, STOA };
enum class IoOp { PRINT, PUNCH, READ, WRITE, DECODE, ENCODE, REWIND, BACKSPACE, ENDFILE };
enum class UnaryOp { NEG, NOT };
enum class BinaryOp {
    ADD,
    SUB,
    MUL,
    DIV,
    INT_DIV,
    POWER,
    LT,
    LE,
    EQ,
    NE,
    GE,
    GT,
    AND,
    OR,
    IMPL,
    EQUIV
};
enum class Conversion { INT_TO_REAL, REAL_TO_INT };

struct Expr;
struct Stmt;
struct Decl;
using ExprPtr  = std::unique_ptr<Expr>;
using StmtPtr  = std::unique_ptr<Stmt>;
using DeclPtr  = std::unique_ptr<Decl>;
using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;
using DeclList = std::vector<DeclPtr>;

namespace expr {
struct IntConst {
    int64_t value{};
};
struct RealConst {
    double value{};
};
struct BoolConst {
    bool value{};
};
struct BitsConst {
    uint64_t value{};
};
struct StringConst {
    std::string value;
};
struct Var {
    int decl{};
};
struct Element {
    int array{};
    ExprList subscripts;
};
struct Call {
    int proc{};
    ExprList args;
};
struct StdFunc {
    ast::StdFunc func{};
    ExprList args;
};
struct Unary {
    UnaryOp op{};
    ExprPtr operand;
};
struct Binary {
    BinaryOp op{};
    ExprPtr left, right;
};
struct Cond {
    ExprPtr condition, then_expr, else_expr;
};
struct Convert {
    Conversion conv{};
    ExprPtr expr;
};
struct StringElement {
    ExprPtr string, index;
};
struct Slice {
    ExprPtr string, first, last;
};
struct LabelRef {
    int label{};
};
struct SwitchElement {
    int switch_{};
    ExprPtr index;
};
} // namespace expr

struct Expr {
    using Node =
        std::variant<expr::IntConst, expr::RealConst, expr::BoolConst, expr::BitsConst,
                     expr::StringConst, expr::Var, expr::Element, expr::Call, expr::StdFunc,
                     expr::Unary, expr::Binary, expr::Cond, expr::Convert, expr::StringElement,
                     expr::Slice, expr::LabelRef, expr::SwitchElement>;
    Node node;
    Type type{};
};

template <class T>
ExprPtr make_expr(Type type, T node)
{
    return std::make_unique<Expr>(Expr{ std::move(node), type });
}

// A left part of an assignment, or the control variable of a FOR.
namespace left {
struct Var {
    int decl{};
};
struct Element {
    int array{};
    ExprList subscripts;
};
} // namespace left
using LeftPart = std::variant<left::Var, left::Element>;

namespace for_elem {
struct Value {
    ExprPtr value;
};
struct StepUntil {
    ExprPtr init, step, until;
};
struct While {
    ExprPtr value, condition;
};
} // namespace for_elem
using ForElement = std::variant<for_elem::Value, for_elem::StepUntil, for_elem::While>;

struct IoElement;
using IoList = std::vector<IoElement>;

namespace io {
struct Item {
    ExprPtr expr;
};
struct For {
    LeftPart control;
    std::vector<ForElement> elements;
    IoList body;
};
struct NewLine {};
struct NewPage {};
} // namespace io

struct IoElement {
    std::variant<io::Item, io::For, io::NewLine, io::NewPage> node;
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
    int label{};
    StmtPtr stmt;
};
struct Dummy {};
struct Assign {
    std::vector<LeftPart> targets;
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
    LeftPart control;
    std::vector<ForElement> elements;
    StmtPtr body;
};
struct Call {
    int proc{};
    ExprList args;
};
struct StdProc {
    ast::StdProc proc{};
    ExprList args;
};
struct Io {
    IoOp op{};
    ExprList params;
    IoList items;
};
} // namespace stmt

struct Stmt {
    using Node = std::variant<stmt::Block, stmt::Compound, stmt::Labeled, stmt::Dummy, stmt::Assign,
                              stmt::Goto, stmt::If, stmt::For, stmt::Call, stmt::StdProc, stmt::Io>;
    Node node;
    SrcLoc loc;
};

template <class T>
StmtPtr make_stmt(SrcLoc loc, T node)
{
    return std::make_unique<Stmt>(Stmt{ std::move(node), loc });
}

struct Spec {
    enum Kind { SIMPLE, ARRAY, PROCEDURE, LABEL, SWITCH } kind{};
    Type type{ Type::NONE }; // of SIMPLE, ARRAY, PROCEDURE
};

struct Formal {
    std::string name;
    int id{};
    Spec spec;
    bool by_value{};
};

struct ArrayName {
    std::string name;
    int id{};
};

struct BoundPair {
    ExprPtr lower, upper;
};

struct CommonElement {
    std::string name;
    int decl{};
    int size{}; // words
};

namespace decl {
struct Var {
    std::string name;
    int id{};
    Type type{};
    bool own{};
};
struct Arrays {
    Type type{};
    bool own{};
    std::vector<ArrayName> arrays;
    std::vector<BoundPair> bounds;
};
struct Switch {
    std::string name;
    int id{};
    ExprList elements;
};
struct Label {
    std::string name;
    int id{};
};
struct Procedure {
    std::string name;
    int id{};
    Type type{};
    std::vector<Formal> formals;
    StmtPtr body;
};
struct Common {
    std::string name;
    std::vector<CommonElement> elements;
};
} // namespace decl

struct Decl {
    using Node = std::variant<decl::Var, decl::Arrays, decl::Switch, decl::Label, decl::Procedure,
                              decl::Common>;
    Node node;
    SrcLoc loc;
};

template <class T>
DeclPtr make_decl(SrcLoc loc, T node)
{
    return std::make_unique<Decl>(Decl{ std::move(node), loc });
}

struct External {
    enum Kind { CODE, ALGOL };
    std::string name;
    int id{};
    Kind kind{};
    Type type{ Type::NONE }; // of a function
};

struct Program {
    std::vector<External> externals;
    StmtPtr block;
    bool unit{}; // procedures compiled alone, global under their names
};

const char *to_string(Type t);
const char *to_string(StdFunc f);
const char *to_string(StdProc p);
const char *to_string(IoOp op);
const char *to_string(UnaryOp op);
const char *to_string(BinaryOp op);
const char *to_string(Conversion c);

// Text form (NAME.ast): statements one per line, indented, with their
// position; expressions as S-expressions; declarations as name#id.
void print(std::ostream &out, const Program &program);

} // namespace ast

#endif
