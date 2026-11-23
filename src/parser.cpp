//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The parser: recursive descent over the symbols of the lexer, building the
// syntax tree. Operators by priority, as in Algol-GDR: EQUIV, IMPL, OR, AND,
// NOT, relations, + -, * /, **.
//
#include "parser.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <map>
#include <optional>
#include <set>

#include "diag.h"
#include "listing.h"

namespace {

using namespace syn;

constexpr unsigned MAX_ERRORS = 30;

// The priority of a binary operator; 0 for other symbols.
int priority(Sym s)
{
    switch (s) {
    case Sym::EQUIV:
        return 1;
    case Sym::IMPL:
        return 2;
    case Sym::OR:
        return 3;
    case Sym::AND:
        return 4;
    case Sym::LT:
    case Sym::LE:
    case Sym::EQ:
    case Sym::NE:
    case Sym::GE:
    case Sym::GT:
        return 6;
    case Sym::PLUS:
    case Sym::MINUS:
        return 7;
    case Sym::TIMES:
    case Sym::SLASH:
        return 8;
    case Sym::POWER:
        return 9;
    default:
        return 0;
    }
}
constexpr int NOT_PRIORITY   = 5;
constexpr int UNARY_PRIORITY = 7;

// The type of a type keyword, or nothing.
std::optional<ast::Type> type_of(Sym s)
{
    switch (s) {
    case Sym::REAL:
        return ast::Type::REAL;
    case Sym::INTEGER:
        return ast::Type::INTEGER;
    case Sym::BOOLEAN:
        return ast::Type::BOOLEAN;
    case Sym::STRING:
        return ast::Type::STRING;
    case Sym::BITS:
        return ast::Type::BITS;
    default:
        return std::nullopt;
    }
}

bool is_io(Sym s)
{
    switch (s) {
    case Sym::READ:
    case Sym::PRINT:
    case Sym::PUNCH:
    case Sym::WRITE:
    case Sym::DECODE:
    case Sym::ENCODE:
    case Sym::REWIND:
    case Sym::BACKSPACE:
    case Sym::ENDFILE:
        return true;
    default:
        return false;
    }
}

// What ends a statement.
bool ends_statement(Sym s)
{
    switch (s) {
    case Sym::SEMICOLON:
    case Sym::END:
    case Sym::ELSE:
    case Sym::EOP:
    case Sym::END_OF_FILE:
        return true;
    default:
        return false;
    }
}

template <class T>
ExprPtr new_expr(int line, T node)
{
    return std::make_unique<Expr>(Expr{ std::move(node), line });
}

template <class T>
StmtPtr new_stmt(ast::SrcLoc loc, T node)
{
    return std::make_unique<Stmt>(Stmt{ std::move(node), loc });
}

template <class T>
DeclPtr new_decl(int line, T node)
{
    return std::make_unique<Decl>(Decl{ std::move(node), line });
}

class Parser {
public:
    Parser(const std::string &source, Diagnostics &diag, Listing *listing);
    Program program();

private:
    struct SyntaxError {}; // reported; recover
    struct Stop {};        // no more parsing

    Diagnostics &diag_;
    Lexer lexer_;
    Token tok_;
    std::optional<Token> peeked_;
    std::map<int, int> statements_; // per line: statements started

    void advance();
    const Token &peek();
    bool accept(Sym s);
    std::string found() const;
    void report(int line, const std::string &message);
    [[noreturn]] void fail(const std::string &message);
    void expect(Sym s);
    Ident ident();
    ast::SrcLoc start();
    void skip_statement();
    void skip_declaration();

    void head(Program &p);
    StmtPtr statement();
    StmtPtr unlabeled(ast::SrcLoc loc);
    StmtPtr compound(ast::SrcLoc loc);
    StmtList statements();
    DeclList declarations();
    bool at_declaration() const;
    DeclPtr declaration();
    DeclPtr procedure(ast::Type type);
    DeclPtr arrays(ast::Type type);
    DeclPtr switch_decl();
    StmtPtr if_stmt(ast::SrcLoc loc);
    StmtPtr for_stmt(ast::SrcLoc loc);
    std::vector<ForElement> for_list();
    StmtPtr assignment_or_call(ast::SrcLoc loc);
    StmtPtr io(ast::SrcLoc loc);
    IoItem io_item();

    ExprPtr expression(int prio);
    ExprPtr operators(int prio, ExprPtr left);
    ExprPtr designator();
    ExprList expressions(Sym close);
};

Parser::Parser(const std::string &source, Diagnostics &diag, Listing *listing)
    : diag_(diag), lexer_(source, diag, [listing](int line, const std::string &text) {
          if (!listing)
              return;
          char num[16];
          std::snprintf(num, sizeof(num), "%6d.     ", line);
          listing->print(num + text);
      })
{
}

void Parser::advance()
{
    if (peeked_) {
        tok_ = std::move(*peeked_);
        peeked_.reset();
    } else {
        tok_ = lexer_.next();
    }
}

const Token &Parser::peek()
{
    if (!peeked_)
        peeked_ = lexer_.next();
    return *peeked_;
}

bool Parser::accept(Sym s)
{
    if (tok_.sym != s)
        return false;
    advance();
    return true;
}

std::string Parser::found() const
{
    switch (tok_.sym) {
    case Sym::IDENT:
        return "'" + tok_.text + "'";
    case Sym::INTEGER_CONST:
    case Sym::REAL_CONST:
    case Sym::BITS_CONST:
        return "a number";
    case Sym::STRING_CONST:
        return "a string";
    case Sym::END_OF_FILE:
        return "the end of the file";
    default:
        return to_string(tok_.sym);
    }
}

void Parser::report(int line, const std::string &message)
{
    diag_.error(line, message);
    if (diag_.errors() >= MAX_ERRORS) {
        diag_.error(line, "Too many errors, compilation stopped");
        throw Stop{};
    }
}

// A syntax error at the current symbol; a lexical error is reported already.
void Parser::fail(const std::string &message)
{
    if (tok_.sym == Sym::END_OF_FILE) {
        report(tok_.line, "Unexpected end of the file, 'eop' is missing");
        throw Stop{};
    }
    if (tok_.sym != Sym::ERROR)
        report(tok_.line, message);
    throw SyntaxError{};
}

void Parser::expect(Sym s)
{
    if (!accept(s))
        fail(std::string("Expected ") + to_string(s) + ", found " + found());
}

Ident Parser::ident()
{
    if (tok_.sym != Sym::IDENT)
        fail("Expected an identifier, found " + found());
    Ident id{ tok_.text, tok_.line };
    advance();
    return id;
}

// The position of a statement starting here: its line, and how many
// statements started on the line so far.
ast::SrcLoc Parser::start()
{
    return ast::SrcLoc{ tok_.line, ++statements_[tok_.line] };
}

// After an error: up to the end of the statement (BEGIN … END skipped).
void Parser::skip_statement()
{
    int depth = 0;
    for (;; advance()) {
        switch (tok_.sym) {
        case Sym::BEGIN:
            depth++;
            continue;
        case Sym::END:
            if (depth-- > 0)
                continue;
            return;
        case Sym::SEMICOLON:
        case Sym::ELSE:
            if (depth > 0)
                continue;
            return;
        case Sym::EOP:
        case Sym::END_OF_FILE:
            return;
        default:
            continue;
        }
    }
}

// After an error in a declaration: past its ';'.
void Parser::skip_declaration()
{
    skip_statement();
    accept(Sym::SEMICOLON);
}

//
// The program: 'code': and 'algol': lists of procedures before it, then a
// block or compound statement, or a procedure; then 'eop'.
//
Program Parser::program()
{
    Program p;
    try {
        advance();
        head(p);
        if (tok_.sym == Sym::END_OF_FILE)
            report(tok_.line, "Unexpected end of the file, 'eop' is missing");
        else if (tok_.sym != Sym::EOP)
            report(tok_.line, "Expected 'eop' after the program, found " + found());
    } catch (const Stop &) {
    }
    return p;
}

void Parser::head(Program &p)
{
    std::optional<ast::External::Kind> kind;
    for (;;) {
        try {
            if (tok_.sym == Sym::ALGOL || tok_.sym == Sym::CODE) {
                kind = tok_.sym == Sym::ALGOL ? ast::External::ALGOL : ast::External::CODE;
                advance();
                expect(Sym::COLON);
                continue;
            }
            if (tok_.sym == Sym::COMMON) {
                p.decls.push_back(declaration());
                continue;
            }
            if (tok_.sym == Sym::BEGIN || (tok_.sym == Sym::IDENT && peek().sym == Sym::COLON)) {
                p.body = statement();
                return;
            }
            ast::Type type = ast::Type::NONE;
            if (auto t = type_of(tok_.sym)) {
                type = *t;
                advance();
            }
            if (tok_.sym == Sym::PROCEDURE) {
                p.module = procedure(type);
                accept(Sym::SEMICOLON);
                return;
            }
            if (tok_.sym != Sym::IDENT)
                fail("Expected the program, found " + found());
            if (!kind)
                fail("Expected 'code': or 'algol': before external procedures");
            for (;;) {
                p.externals.push_back(External{ ident(), type, *kind });
                if (!accept(Sym::COMMA))
                    break;
            }
            expect(Sym::SEMICOLON);
        } catch (const SyntaxError &) {
            skip_declaration();
            if (tok_.sym == Sym::EOP || tok_.sym == Sym::END_OF_FILE)
                return;
        }
    }
}

//
// Statements.
//
StmtPtr Parser::statement()
{
    ast::SrcLoc loc = start();
    if (tok_.sym == Sym::IDENT && peek().sym == Sym::COLON) {
        Ident label = ident();
        advance();
        return new_stmt(loc, stmt::Labeled{ label, statement() });
    }
    try {
        return unlabeled(loc);
    } catch (const SyntaxError &) {
        skip_statement();
        return new_stmt(loc, stmt::Error{});
    }
}

StmtPtr Parser::unlabeled(ast::SrcLoc loc)
{
    switch (tok_.sym) {
    case Sym::BEGIN:
        return compound(loc);
    case Sym::IF:
        return if_stmt(loc);
    case Sym::GOTO:
        advance();
        return new_stmt(loc, stmt::Goto{ expression(0) });
    case Sym::FOR:
        return for_stmt(loc);
    case Sym::IDENT:
        return assignment_or_call(loc);
    case Sym::OWN:
        fail("'own' is not supported");
    default:
        if (is_io(tok_.sym))
            return io(loc);
        if (ends_statement(tok_.sym))
            return new_stmt(loc, stmt::Dummy{});
        fail("Expected a statement, found " + found());
    }
}

// 'begin' [declarations;] statements 'end': a block or compound statement.
StmtPtr Parser::compound(ast::SrcLoc loc)
{
    advance();
    if (at_declaration()) {
        stmt::Block b;
        b.decls = declarations();
        b.body  = statements();
        return new_stmt(loc, std::move(b));
    }
    return new_stmt(loc, stmt::Compound{ statements() });
}

// Statements separated by ';', then 'end'.
StmtList Parser::statements()
{
    StmtList list;
    for (;;) {
        list.push_back(statement());
        if (accept(Sym::SEMICOLON))
            continue;
        if (accept(Sym::END))
            return list;
        if (tok_.sym == Sym::EOP || tok_.sym == Sym::END_OF_FILE) {
            report(tok_.line, "Missing 'end'");
            return list;
        }
        report(tok_.line, "Expected ';' or 'end', found " + found());
        // The statement before is incomplete: not checked.
        if (!std::holds_alternative<stmt::Labeled>(list.back()->node))
            list.back()->node = stmt::Error{};
        if (tok_.sym == Sym::ELSE)
            advance();
        skip_statement();
        accept(Sym::SEMICOLON);
    }
}

bool Parser::at_declaration() const
{
    switch (tok_.sym) {
    case Sym::REAL:
    case Sym::INTEGER:
    case Sym::BOOLEAN:
    case Sym::STRING:
    case Sym::BITS:
    case Sym::ARRAY:
    case Sym::SWITCH:
    case Sym::PROCEDURE:
    case Sym::COMMON:
        return true;
    default:
        return false;
    }
}

DeclList Parser::declarations()
{
    DeclList list;
    while (at_declaration()) {
        try {
            list.push_back(declaration());
        } catch (const SyntaxError &) {
            skip_declaration();
        }
    }
    return list;
}

DeclPtr Parser::declaration()
{
    int line = tok_.line;
    switch (tok_.sym) {
    case Sym::COMMON: {
        auto block = tok_.common;
        advance();
        return new_decl(line, decl::Common{ block });
    }
    case Sym::ARRAY:
        return arrays(ast::Type::REAL);
    case Sym::SWITCH:
        return switch_decl();
    case Sym::PROCEDURE: {
        DeclPtr d = procedure(ast::Type::NONE);
        expect(Sym::SEMICOLON);
        return d;
    }
    default:
        break;
    }
    ast::Type type = *type_of(tok_.sym);
    advance();
    if (tok_.sym == Sym::ARRAY)
        return arrays(type);
    if (tok_.sym == Sym::PROCEDURE) {
        DeclPtr d = procedure(type);
        expect(Sym::SEMICOLON);
        return d;
    }
    decl::Vars v;
    v.type = type;
    for (;;) {
        v.names.push_back(ident());
        if (!accept(Sym::COMMA))
            break;
    }
    expect(Sym::SEMICOLON);
    return new_decl(line, std::move(v));
}

// 'array' segments: names [bounds] (or with '(' ')'), separated by ','.
DeclPtr Parser::arrays(ast::Type type)
{
    int line = tok_.line;
    advance();
    decl::Arrays a;
    a.type = type;
    for (;;) {
        decl::Arrays::Segment seg;
        for (;;) {
            seg.names.push_back(ident());
            if (!accept(Sym::COMMA))
                break;
        }
        Sym close = Sym::RBRACKET;
        if (accept(Sym::LPAREN))
            close = Sym::RPAREN;
        else
            expect(Sym::LBRACKET);
        for (;;) {
            BoundPair b;
            b.lower = expression(0);
            expect(Sym::COLON);
            b.upper = expression(0);
            seg.bounds.push_back(std::move(b));
            if (!accept(Sym::COMMA))
                break;
        }
        expect(close);
        a.segments.push_back(std::move(seg));
        if (!accept(Sym::COMMA))
            break;
    }
    expect(Sym::SEMICOLON);
    return new_decl(line, std::move(a));
}

DeclPtr Parser::switch_decl()
{
    int line = tok_.line;
    advance();
    decl::Switch s;
    s.name = ident();
    expect(Sym::ASSIGN);
    for (;;) {
        s.elements.push_back(expression(0));
        if (!accept(Sym::COMMA))
            break;
    }
    expect(Sym::SEMICOLON);
    return new_decl(line, std::move(s));
}

//
// 'procedure' name [(formals)]; ['value' names;] specifications body. Every
// formal is specified once.
//
DeclPtr Parser::procedure(ast::Type type)
{
    int line = tok_.line;
    advance();
    decl::Procedure p;
    p.type = type;
    p.name = ident();
    if (accept(Sym::LPAREN)) {
        for (;;) {
            p.formals.push_back(ident());
            if (!accept(Sym::COMMA))
                break;
        }
        expect(Sym::RPAREN);
    }
    expect(Sym::SEMICOLON);
    std::set<std::string> formals, specified;
    std::transform(p.formals.begin(), p.formals.end(), std::inserter(formals, formals.end()),
                   [](const Ident &f) { return f.name; });
    auto check = [&](const Ident &id) {
        if (!formals.count(id.name))
            report(id.line, "'" + id.name + "' is not a formal parameter of '" + p.name.name + "'");
    };
    if (accept(Sym::VALUE)) {
        for (;;) {
            Ident id = ident();
            check(id);
            p.values.push_back(id);
            if (!accept(Sym::COMMA))
                break;
        }
        expect(Sym::SEMICOLON);
    }
    for (;;) {
        Spec spec;
        if (auto t = type_of(tok_.sym)) {
            spec.type = *t;
            spec.kind = ast::Spec::SIMPLE;
            advance();
            if (accept(Sym::ARRAY))
                spec.kind = ast::Spec::ARRAY;
            else if (accept(Sym::PROCEDURE))
                spec.kind = ast::Spec::PROCEDURE;
        } else if (accept(Sym::ARRAY)) {
            spec.kind = ast::Spec::ARRAY;
            spec.type = ast::Type::REAL;
        } else if (accept(Sym::PROCEDURE)) {
            spec.kind = ast::Spec::PROCEDURE;
        } else if (accept(Sym::LABEL)) {
            spec.kind = ast::Spec::LABEL;
        } else if (accept(Sym::SWITCH)) {
            spec.kind = ast::Spec::SWITCH;
        } else {
            break;
        }
        for (;;) {
            Ident id = ident();
            check(id);
            if (!specified.insert(id.name).second)
                report(id.line, "Formal parameter '" + id.name + "' is specified twice");
            spec.names.push_back(id);
            if (!accept(Sym::COMMA))
                break;
        }
        expect(Sym::SEMICOLON);
        p.specs.push_back(std::move(spec));
    }
    for (const auto &f : p.formals)
        if (!specified.count(f.name))
            report(f.line, "Formal parameter '" + f.name + "' is not specified");
    p.body = statement();
    return new_decl(line, std::move(p));
}

// 'if' b 'then' s ['else' s]: no IF right after THEN, no ELSE after a FOR.
StmtPtr Parser::if_stmt(ast::SrcLoc loc)
{
    advance();
    stmt::If s;
    s.condition = expression(0);
    expect(Sym::THEN);
    if (tok_.sym == Sym::IF)
        fail("A conditional statement after 'then' must be in 'begin' 'end'");
    bool is_for = tok_.sym == Sym::FOR;
    s.then_stmt = statement();
    if (tok_.sym == Sym::ELSE) {
        if (is_for)
            fail("A 'for' statement before 'else' must be in 'begin' 'end'");
        advance();
        s.else_stmt = statement();
    }
    return new_stmt(loc, std::move(s));
}

// 'for' v := elements 'do' s
StmtPtr Parser::for_stmt(ast::SrcLoc loc)
{
    advance();
    stmt::For s;
    s.var = ident();
    expect(Sym::ASSIGN);
    s.elements = for_list();
    expect(Sym::DO);
    s.body = statement();
    return new_stmt(loc, std::move(s));
}

// e, e 'step' e 'until' e, e 'while' b, …
std::vector<ForElement> Parser::for_list()
{
    std::vector<ForElement> list;
    for (;;) {
        ForElement e;
        e.value = expression(0);
        if (accept(Sym::STEP)) {
            e.kind = ForElement::STEP_UNTIL;
            e.step = expression(0);
            expect(Sym::UNTIL);
            e.until = expression(0);
        } else if (accept(Sym::WHILE)) {
            e.kind = ForElement::WHILE;
            e.step = expression(0);
        }
        list.push_back(std::move(e));
        if (!accept(Sym::COMMA))
            return list;
    }
}

// A designator, then ':=' and the left parts and the value; or a call.
StmtPtr Parser::assignment_or_call(ast::SrcLoc loc)
{
    ExprPtr first = designator();
    if (!accept(Sym::ASSIGN)) {
        if (!std::holds_alternative<expr::Name>(first->node) &&
            !std::holds_alternative<expr::Call>(first->node))
            fail("Expected ':=', found " + found());
        return new_stmt(loc, stmt::Call{ std::move(first) });
    }
    stmt::Assign a;
    a.targets.push_back(std::move(first));
    for (;;) {
        ExprPtr e = expression(0);
        if (tok_.sym != Sym::ASSIGN) {
            a.value = std::move(e);
            return new_stmt(loc, std::move(a));
        }
        if (!std::holds_alternative<expr::Name>(e->node) &&
            !std::holds_alternative<expr::Index>(e->node))
            fail("Illegal left part of an assignment");
        advance();
        a.targets.push_back(std::move(e));
    }
}

//
// 'print' (e, …) items; and the like: the parameters in parentheses, or
// one expression and ','; then the list, up to the end of the statement.
//
StmtPtr Parser::io(ast::SrcLoc loc)
{
    stmt::Io s;
    s.op = tok_.sym;
    advance();
    if (accept(Sym::LPAREN)) {
        s.params = expressions(Sym::RPAREN);
    } else {
        s.params.push_back(expression(0));
        accept(Sym::COMMA);
    }
    bool has_list = s.op != Sym::REWIND && s.op != Sym::BACKSPACE && s.op != Sym::ENDFILE;
    if (has_list && !ends_statement(tok_.sym)) {
        for (;;) {
            s.items.push_back(io_item());
            if (!accept(Sym::COMMA))
                break;
        }
    }
    return new_stmt(loc, std::move(s));
}

// An expression, or 'for' v := elements 'do' item.
IoItem Parser::io_item()
{
    IoItem item;
    if (!accept(Sym::FOR)) {
        item.expr = expression(0);
        return item;
    }
    item.var = ident().name;
    expect(Sym::ASSIGN);
    item.elements = for_list();
    expect(Sym::DO);
    item.items.push_back(io_item());
    return item;
}

//
// Expressions.
//
ExprPtr Parser::expression(int prio)
{
    int line = tok_.line;
    ExprPtr e;
    switch (tok_.sym) {
    case Sym::IF: {
        if (prio)
            fail("A conditional expression here must be in parentheses");
        advance();
        expr::If x;
        x.condition = expression(0);
        expect(Sym::THEN);
        x.then_expr = expression(1);
        expect(Sym::ELSE);
        x.else_expr = expression(0);
        return new_expr(line, std::move(x));
    }
    case Sym::LPAREN:
        advance();
        e = expression(0);
        expect(Sym::RPAREN);
        break;
    case Sym::NOT:
    case Sym::PLUS:
    case Sym::MINUS: {
        Sym op = tok_.sym;
        int p  = op == Sym::NOT ? NOT_PRIORITY : UNARY_PRIORITY;
        advance();
        e = new_expr(line, expr::Unary{ op, expression(std::max(p + 1, prio)) });
        break;
    }
    case Sym::IDENT:
        e = designator();
        break;
    case Sym::INTEGER_CONST:
        e = new_expr(line, expr::Int{ tok_.ival });
        advance();
        break;
    case Sym::REAL_CONST:
        e = new_expr(line, expr::Real{ tok_.rval });
        advance();
        break;
    case Sym::BITS_CONST:
        e = new_expr(line, expr::Bits{ tok_.bits });
        advance();
        break;
    case Sym::STRING_CONST:
        e = new_expr(line, expr::String{ tok_.text });
        advance();
        break;
    case Sym::TRUE:
    case Sym::FALSE:
        e = new_expr(line, expr::Bool{ tok_.sym == Sym::TRUE });
        advance();
        break;
    default:
        fail("Expected an expression, found " + found());
    }
    return operators(prio, std::move(e));
}

// Binary operators of priority prio or more after the operand left.
ExprPtr Parser::operators(int prio, ExprPtr left)
{
    for (;;) {
        int p = priority(tok_.sym);
        if (!p || p < prio)
            return left;
        Sym op   = tok_.sym;
        int line = tok_.line;
        advance();
        ExprPtr right = expression(p + 1);
        left          = new_expr(line, expr::Binary{ op, std::move(left), std::move(right) });
    }
}

// name, name(args), name[subscripts], name[i:j]
ExprPtr Parser::designator()
{
    int line         = tok_.line;
    std::string name = ident().name;
    if (accept(Sym::LPAREN))
        return new_expr(line, expr::Call{ name, expressions(Sym::RPAREN) });
    if (!accept(Sym::LBRACKET))
        return new_expr(line, expr::Name{ name });
    ExprPtr first = expression(0);
    if (accept(Sym::COLON)) {
        ExprPtr last = expression(0);
        expect(Sym::RBRACKET);
        return new_expr(line, expr::Slice{ name, std::move(first), std::move(last) });
    }
    ExprList subs;
    subs.push_back(std::move(first));
    while (accept(Sym::COMMA))
        subs.push_back(expression(0));
    expect(Sym::RBRACKET);
    return new_expr(line, expr::Index{ name, std::move(subs) });
}

// Expressions separated by ',', then close.
ExprList Parser::expressions(Sym close)
{
    ExprList list;
    for (;;) {
        list.push_back(expression(0));
        if (!accept(Sym::COMMA))
            break;
    }
    expect(close);
    return list;
}

} // namespace

syn::Program parse(const std::string &source, Diagnostics &diag, Listing *listing)
{
    return Parser(source, diag, listing).program();
}
