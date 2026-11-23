//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The checker: scopes, declarations, types. A block declares its names
// (labels too) before its statements are checked, so they are visible in
// the whole block; array bounds are checked in the enclosing scope. An
// expression with an error has type none, which keeps the operators above
// it quiet.
//
#include "checker.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>

#include "diag.h"

namespace {

using ast::Type;
using namespace ast;

enum class Kind { SIMPLE, ARRAY, PROCEDURE, LABEL, SWITCH };
enum class Mode { LOCAL, VALUE, NAME, EXTERNAL };

struct Symbol {
    int id{};
    std::string name;
    Kind kind{};
    Type type{ Type::NONE };
    Mode mode{ Mode::LOCAL };
    int count{ -1 };          // dimensions or parameters; -1 when not known
    std::vector<int> formals; // of a procedure declared here: symbol indexes
    bool assigned{};          // a function: its result was assigned
    bool any{};               // a formal without a specification, only passed on
};

// How a formal parameter without a specification is used in the body of
// its procedure (inner declarations of the same name are not seen).
class Usage {
public:
    explicit Usage(const std::string &name) : name_(name) {}

    bool subscripted{}, switched{}, called{}, function{}, jumped{}, boolean{}, integer{}, other{};

    void stmt(const syn::Stmt &s)
    {
        std::visit(
            [&](const auto &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, syn::stmt::Block>) {
                    for (const auto &d : x.decls)
                        decl(*d);
                    for (const auto &b : x.body)
                        stmt(*b);
                } else if constexpr (std::is_same_v<T, syn::stmt::Compound>) {
                    for (const auto &b : x.body)
                        stmt(*b);
                } else if constexpr (std::is_same_v<T, syn::stmt::Labeled>) {
                    stmt(*x.stmt);
                } else if constexpr (std::is_same_v<T, syn::stmt::Assign>) {
                    bool b = is_boolean(*x.value);
                    for (const auto &t : x.targets) {
                        if (b && is_name(*t))
                            boolean = true;
                        operand(*t);
                    }
                    operand(*x.value);
                } else if constexpr (std::is_same_v<T, syn::stmt::Goto>) {
                    designational(*x.target);
                } else if constexpr (std::is_same_v<T, syn::stmt::If>) {
                    condition(*x.condition);
                    stmt(*x.then_stmt);
                    if (x.else_stmt)
                        stmt(*x.else_stmt);
                } else if constexpr (std::is_same_v<T, syn::stmt::For>) {
                    operand(*x.var);
                    elements(x.elements);
                    stmt(*x.body);
                } else if constexpr (std::is_same_v<T, syn::stmt::Call>) {
                    auto c = std::get_if<syn::expr::Call>(&x.call->node);
                    if (is_name(*x.call) || (c && c->name == name_))
                        called = true;
                    if (c)
                        for (const auto &a : c->args)
                            expr(*a);
                } else if constexpr (std::is_same_v<T, syn::stmt::Io>) {
                    for (const auto &p : x.params)
                        expr(*p);
                    items(x.items);
                }
            },
            s.node);
    }

private:
    const std::string &name_;

    bool is_name(const syn::Expr &e) const
    {
        auto n = std::get_if<syn::expr::Name>(&e.node);
        return n && n->name == name_;
    }

    static bool is_boolean(const syn::Expr &e)
    {
        if (std::holds_alternative<syn::expr::Bool>(e.node))
            return true;
        if (auto u = std::get_if<syn::expr::Unary>(&e.node))
            return u->op == Sym::NOT;
        if (auto b = std::get_if<syn::expr::Binary>(&e.node))
            return b->op != Sym::PLUS && b->op != Sym::MINUS && b->op != Sym::TIMES &&
                   b->op != Sym::SLASH && b->op != Sym::INTDIV && b->op != Sym::POWER;
        return false;
    }

    void condition(const syn::Expr &e)
    {
        if (is_name(e))
            boolean = true;
        expr(e);
    }

    void designational(const syn::Expr &e)
    {
        if (is_name(e)) {
            jumped = true;
            return;
        }
        if (auto x = std::get_if<syn::expr::Index>(&e.node); x && x->name == name_) {
            switched = true;
            for (const auto &a : x->subscripts)
                expr(*a);
            return;
        }
        if (auto c = std::get_if<syn::expr::If>(&e.node)) {
            condition(*c->condition);
            designational(*c->then_expr);
            designational(*c->else_expr);
            return;
        }
        expr(e);
    }

    void decl(const syn::Decl &d)
    {
        if (auto a = std::get_if<syn::decl::Arrays>(&d.node)) {
            for (const auto &seg : a->segments)
                for (const auto &b : seg.bounds) {
                    expr(*b.lower);
                    expr(*b.upper);
                }
        } else if (auto sw = std::get_if<syn::decl::Switch>(&d.node)) {
            for (const auto &e : sw->elements)
                designational(*e);
        } else if (auto p = std::get_if<syn::decl::Procedure>(&d.node)) {
            stmt(*p->body);
        }
    }

    void elements(const std::vector<syn::ForElement> &list)
    {
        for (const auto &el : list) {
            operand(*el.value);
            if (el.kind == syn::ForElement::WHILE) {
                condition(*el.step);
            } else if (el.kind == syn::ForElement::STEP_UNTIL) {
                operand(*el.step);
                operand(*el.until);
            }
        }
    }

    void items(const std::vector<syn::IoItem> &list)
    {
        for (const auto &it : list) {
            if (it.expr) {
                operand(*it.expr);
            } else {
                elements(it.elements);
                items(it.items);
            }
        }
    }

    void expr(const syn::Expr &e)
    {
        std::visit(
            [&](const auto &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, syn::expr::Name>) {
                    // An argument alone tells nothing.
                } else if constexpr (std::is_same_v<T, syn::expr::Index>) {
                    if (x.name == name_)
                        subscripted = true;
                    for (const auto &a : x.subscripts)
                        operand(*a);
                } else if constexpr (std::is_same_v<T, syn::expr::Slice>) {
                    operand(*x.first);
                    operand(*x.last);
                } else if constexpr (std::is_same_v<T, syn::expr::Call>) {
                    if (x.name == name_)
                        called = function = true;
                    for (const auto &a : x.args)
                        expr(*a);
                } else if constexpr (std::is_same_v<T, syn::expr::Unary>) {
                    if (x.op == Sym::NOT)
                        condition(*x.operand);
                    else
                        operand(*x.operand);
                } else if constexpr (std::is_same_v<T, syn::expr::Binary>) {
                    switch (x.op) {
                    case Sym::AND:
                    case Sym::OR:
                    case Sym::IMPL:
                    case Sym::EQUIV:
                        condition(*x.left);
                        condition(*x.right);
                        break;
                    case Sym::INTDIV:
                        if (is_name(*x.left) || is_name(*x.right))
                            integer = true;
                        expr(*x.left);
                        expr(*x.right);
                        break;
                    default:
                        operand(*x.left);
                        operand(*x.right);
                    }
                } else if constexpr (std::is_same_v<T, syn::expr::If>) {
                    condition(*x.condition);
                    expr(*x.then_expr);
                    expr(*x.else_expr);
                }
            },
            e.node);
    }

    // In an arithmetic expression.
    void operand(const syn::Expr &e)
    {
        if (is_name(e))
            other = true;
        expr(e);
    }
};

// Whether a formal parameter has a specification.
bool specified(const syn::decl::Procedure &p, const std::string &name)
{
    return std::any_of(p.specs.begin(), p.specs.end(), [&](const syn::Spec &spec) {
        return std::any_of(spec.names.begin(), spec.names.end(),
                           [&](const syn::Ident &id) { return id.name == name; });
    });
}

// The kind and type of a formal without a specification, from its use: a
// number is taken as real.
void infer(Symbol &s, const std::string &name, const syn::Stmt &body)
{
    Usage u(name);
    u.stmt(body);
    s.kind = Kind::SIMPLE;
    s.type = u.boolean ? Type::BOOLEAN : u.integer ? Type::INTEGER : Type::REAL;
    if (u.called) {
        s.kind = Kind::PROCEDURE;
        if (!u.function)
            s.type = Type::NONE;
    } else if (u.subscripted) {
        s.kind = Kind::ARRAY;
    } else if (u.switched) {
        s.kind = Kind::SWITCH;
        s.type = Type::NONE;
    } else if (u.jumped) {
        s.kind = Kind::LABEL;
        s.type = Type::NONE;
    } else if (!u.boolean && !u.integer && !u.other) {
        s.type = Type::NONE;
        s.any  = true;
    }
}

// The standard identifiers.
enum class Std {
    NONE,
    ABS,
    ARCTAN,
    COS,
    ENTIER,
    EXP,
    LENGTH,
    LN,
    SIGN,
    SIN,
    SQRT,
    PRINT,
    READ,
    NEWLINE,
    NEWPAGE,
    STOP,
    LOAD,
    LOADGO,
    ASSIGN,
    ATOS,
    STOA,
};

// A standard identifier by its name in any case.
Std standard(const std::string &name)
{
    static const std::map<std::string, Std> table = {
        { "ABS", Std::ABS },         { "ARCTAN", Std::ARCTAN },   { "COS", Std::COS },
        { "ENTIER", Std::ENTIER },   { "EXP", Std::EXP },         { "LENGTH", Std::LENGTH },
        { "LN", Std::LN },           { "SIGN", Std::SIGN },       { "SIN", Std::SIN },
        { "SQRT", Std::SQRT },       { "PRINT", Std::PRINT },     { "READ", Std::READ },
        { "NEWLIN", Std::NEWLINE },  { "NEWPAG", Std::NEWPAGE },  { "STOP", Std::STOP },
        { "NEWLINE", Std::NEWLINE }, { "NEWPAGE", Std::NEWPAGE }, { "LOAD", Std::LOAD },
        { "LOADGO", Std::LOADGO },   { "ASSIGN", Std::ASSIGN },   { "ATOS", Std::ATOS },
        { "STOA", Std::STOA },
    };
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](char c) { return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c; });
    auto it = table.find(upper);
    return it == table.end() ? Std::NONE : it->second;
}

// A standard function of one number.
bool std_function(Std s, ast::StdFunc &f, Type &t)
{
    t = Type::REAL;
    switch (s) {
    case Std::ABS:
        f = StdFunc::ABS;
        return true;
    case Std::ARCTAN:
        f = StdFunc::ARCTAN;
        return true;
    case Std::COS:
        f = StdFunc::COS;
        return true;
    case Std::EXP:
        f = StdFunc::EXP;
        return true;
    case Std::LN:
        f = StdFunc::LN;
        return true;
    case Std::SIN:
        f = StdFunc::SIN;
        return true;
    case Std::SQRT:
        f = StdFunc::SQRT;
        return true;
    case Std::SIGN:
        f = StdFunc::SIGN;
        t = Type::INTEGER;
        return true;
    case Std::ENTIER:
        f = StdFunc::ENTIER;
        t = Type::INTEGER;
        return true;
    default:
        return false;
    }
}

bool numeric(Type t)
{
    return t == Type::INTEGER || t == Type::REAL;
}

std::string quoted(const std::string &name)
{
    return "'" + name + "'";
}

const char *op_text(Sym s)
{
    return to_string(s);
}

class Checker {
public:
    explicit Checker(Diagnostics &diag) : diag_(diag) {}
    Program program(const syn::Program &p);

private:
    struct Scope {
        std::map<std::string, int> names; // name → symbol
    };

    Diagnostics &diag_;
    std::vector<Symbol> symbols_;
    std::vector<Scope> scopes_;
    std::vector<int> functions_; // procedures whose bodies are being checked
    std::set<std::string> undeclared_;

    void error(int line, const std::string &message) { diag_.error(line, message); }

    int declare(int line, const std::string &name, Symbol s);
    int lookup(const std::string &name) const;
    const Symbol &sym(int index) const { return symbols_[index]; }

    // Declarations and blocks.
    using Decls = std::vector<const syn::Decl *>;
    using Stmts = std::vector<const syn::Stmt *>;
    StmtPtr make_block(SrcLoc loc, const Decls &decls, const Stmts &body);
    static Decls ptrs(const syn::DeclList &list);
    static Stmts ptrs(const syn::StmtList &list);
    static void collect_labels(const syn::Stmt &s, std::vector<const syn::Ident *> &labels);
    void declare_labels(const Stmts &body, DeclList &out);
    void procedure_body(const syn::decl::Procedure &p, int index, decl::Procedure &out);
    DeclPtr common(int line, const CommonBlock &block);

    // Statements.
    StmtList statements(const syn::StmtList &list);
    StmtList statements(const Stmts &list);
    StmtPtr statement(const syn::Stmt &s);
    StmtPtr assignment(SrcLoc loc, const syn::stmt::Assign &a);
    StmtPtr for_stmt(SrcLoc loc, const syn::stmt::For &f);
    std::vector<ForElement> for_elements(const std::vector<syn::ForElement> &list, Type t,
                                         int line);
    StmtPtr call_stmt(SrcLoc loc, const syn::Expr &e);
    StmtPtr std_stmt(SrcLoc loc, Std s, const std::string &name, const syn::ExprList *args,
                     int line);
    StmtPtr io_stmt(SrcLoc loc, const syn::stmt::Io &io);
    IoList io_items(const std::vector<syn::IoItem> &items, int line);
    std::optional<LeftPart> left_part(const syn::Expr &e, Type &type);

    // Expressions.
    ExprPtr expr(const syn::Expr &e);
    ExprPtr bad() { return make_expr(Type::NONE, expr::IntConst{ 0 }); }
    ExprPtr convert(ExprPtr e, Type to, int line, const std::string &what);
    ExprPtr integer(const syn::Expr &e, const std::string &what);
    ExprPtr boolean(const syn::Expr &e, const std::string &what);
    ExprPtr binary(const syn::expr::Binary &b, int line);
    ExprPtr named(const std::string &name, int line, bool argument);
    ExprPtr indexed(const syn::expr::Index &x, int line);
    ExprPtr call(const std::string &name, const syn::ExprList &args, int line);
    ExprList arguments(int index, const syn::ExprList &args, int line);
    ExprPtr actual(const syn::Expr &e, const Symbol *formal, int n, const std::string &proc);
    ExprPtr designational(const syn::Expr &e, const std::string &what);
};

int Checker::declare(int line, const std::string &name, Symbol s)
{
    auto &names = scopes_.back().names;
    if (names.count(name))
        error(line, quoted(name) + " is declared twice");
    s.id   = int(symbols_.size()) + 1;
    s.name = name;
    symbols_.push_back(std::move(s));
    int index   = int(symbols_.size()) - 1;
    names[name] = index;
    return index;
}

// A symbol by name, innermost scope first; -1 when not declared.
int Checker::lookup(const std::string &name) const
{
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto f = it->names.find(name);
        if (f != it->names.end())
            return f->second;
    }
    return -1;
}

Program Checker::program(const syn::Program &p)
{
    Program out;
    scopes_.emplace_back(); // the externals
    for (const auto &e : p.externals) {
        Symbol s;
        s.kind = Kind::PROCEDURE;
        s.type = e.type;
        s.mode = Mode::EXTERNAL;
        int i  = declare(e.name.line, e.name.name, s);
        out.externals.push_back(External{ e.name.name, sym(i).id, e.kind, e.type });
    }
    Decls decls = ptrs(p.decls);
    if (!p.procedures.empty()) {
        // Procedures compiled alone: a block of them.
        Decls more = ptrs(p.procedures);
        decls.insert(decls.end(), more.begin(), more.end());
        out.block = make_block(SrcLoc{ p.procedures.front()->line, 1 }, decls, {});
        out.unit  = true;
        return out;
    }
    if (!p.body)
        return out;
    const syn::Stmt &s = *p.body;
    if (auto b = std::get_if<syn::stmt::Block>(&s.node)) {
        Decls more = ptrs(b->decls);
        decls.insert(decls.end(), more.begin(), more.end());
        out.block = make_block(s.loc, decls, ptrs(b->body));
    } else if (auto c = std::get_if<syn::stmt::Compound>(&s.node)) {
        out.block = make_block(s.loc, decls, ptrs(c->body));
    } else {
        out.block = make_block(s.loc, decls, { &s }); // labels before the program
    }
    return out;
}

//
// A block: the bounds of its arrays in the enclosing scope, then its names,
// then the switches, the procedures, the _common blocks and the statements.
//
Checker::Decls Checker::ptrs(const syn::DeclList &list)
{
    Decls out(list.size());
    std::transform(list.begin(), list.end(), out.begin(), [](const auto &d) { return d.get(); });
    return out;
}

Checker::Stmts Checker::ptrs(const syn::StmtList &list)
{
    Stmts out(list.size());
    std::transform(list.begin(), list.end(), out.begin(), [](const auto &s) { return s.get(); });
    return out;
}

StmtPtr Checker::make_block(SrcLoc loc, const Decls &decls, const Stmts &body)
{
    std::vector<std::vector<BoundPair>> bounds;
    for (const auto *d : decls) {
        if (auto a = std::get_if<syn::decl::Arrays>(&d->node)) {
            for (const auto &seg : a->segments) {
                std::vector<BoundPair> list(seg.bounds.size());
                std::transform(seg.bounds.begin(), seg.bounds.end(), list.begin(),
                               [this](const syn::BoundPair &b) {
                                   return BoundPair{ integer(*b.lower, "an array bound"),
                                                     integer(*b.upper, "an array bound") };
                               });
                bounds.push_back(std::move(list));
            }
        }
    }

    scopes_.emplace_back();
    stmt::Block out;
    std::vector<std::pair<const syn::Decl *, Decl *>> later; // switches, procedures
    size_t segment = 0;
    std::vector<const syn::Decl *> commons;
    for (const auto *d : decls) {
        SrcLoc dloc{ d->line, 0 };
        std::visit(
            [&](const auto &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, syn::decl::Vars>) {
                    for (const auto &n : x.names) {
                        Symbol s;
                        s.type = x.type;
                        int i  = declare(n.line, n.name, s);
                        out.decls.push_back(make_decl(
                            SrcLoc{ n.line, 0 }, decl::Var{ n.name, sym(i).id, x.type, x.own }));
                    }
                } else if constexpr (std::is_same_v<T, syn::decl::Arrays>) {
                    for (const auto &seg : x.segments) {
                        decl::Arrays a;
                        a.type = x.type;
                        a.own  = x.own;
                        for (const auto &n : seg.names) {
                            Symbol s;
                            s.kind  = Kind::ARRAY;
                            s.type  = x.type;
                            s.count = int(seg.bounds.size());
                            int i   = declare(n.line, n.name, s);
                            a.arrays.push_back(ArrayName{ n.name, sym(i).id });
                        }
                        a.bounds = std::move(bounds[segment++]);
                        out.decls.push_back(make_decl(dloc, std::move(a)));
                    }
                } else if constexpr (std::is_same_v<T, syn::decl::Switch>) {
                    Symbol s;
                    s.kind = Kind::SWITCH;
                    int i  = declare(x.name.line, x.name.name, s);
                    out.decls.push_back(
                        make_decl(dloc, decl::Switch{ x.name.name, sym(i).id, {} }));
                    later.emplace_back(d, out.decls.back().get());
                } else if constexpr (std::is_same_v<T, syn::decl::Procedure>) {
                    Symbol s;
                    s.kind  = Kind::PROCEDURE;
                    s.type  = x.type;
                    s.count = int(x.formals.size());
                    int i   = declare(x.name.line, x.name.name, s);
                    // The formals: symbols now, for the calls before the body.
                    for (const auto &f : x.formals) {
                        Symbol fs;
                        fs.mode = Mode::NAME;
                        for (const auto &v : x.values)
                            if (v.name == f.name)
                                fs.mode = Mode::VALUE;
                        for (const auto &spec : x.specs) {
                            for (const auto &n : spec.names) {
                                if (n.name != f.name)
                                    continue;
                                fs.type = spec.type;
                                switch (spec.kind) {
                                case Spec::SIMPLE:
                                    fs.kind = Kind::SIMPLE;
                                    break;
                                case Spec::ARRAY:
                                    fs.kind = Kind::ARRAY;
                                    break;
                                case Spec::PROCEDURE:
                                    fs.kind = Kind::PROCEDURE;
                                    break;
                                case Spec::LABEL:
                                    fs.kind = Kind::LABEL;
                                    break;
                                case Spec::SWITCH:
                                    fs.kind = Kind::SWITCH;
                                    break;
                                }
                            }
                        }
                        if (!specified(x, f.name))
                            infer(fs, f.name, *x.body);
                        fs.id   = int(symbols_.size()) + 1;
                        fs.name = f.name;
                        symbols_.push_back(fs);
                        symbols_[i].formals.push_back(int(symbols_.size()) - 1);
                    }
                    out.decls.push_back(make_decl(
                        dloc, decl::Procedure{ x.name.name, sym(i).id, x.type, {}, nullptr }));
                    later.emplace_back(d, out.decls.back().get());
                } else if constexpr (std::is_same_v<T, syn::decl::Common>) {
                    commons.push_back(d);
                }
            },
            d->node);
    }
    declare_labels(body, out.decls);

    for (const auto &[d, o] : later) {
        if (auto s = std::get_if<syn::decl::Switch>(&d->node)) {
            auto &sw = std::get<decl::Switch>(o->node);
            for (const auto &e : s->elements)
                sw.elements.push_back(
                    designational(*e, "an element of switch " + quoted(s->name.name)));
        } else {
            const auto &p = std::get<syn::decl::Procedure>(d->node);
            int index     = lookup(p.name.name);
            procedure_body(p, index, std::get<decl::Procedure>(o->node));
        }
    }
    for (const auto *d : commons) {
        const auto &c = std::get<syn::decl::Common>(d->node);
        out.decls.push_back(common(d->line, *c.block));
    }
    out.body = statements(body);
    scopes_.pop_back();
    return make_stmt(loc, std::move(out));
}

// The labels of a statement in its block: not in inner blocks.
void Checker::collect_labels(const syn::Stmt &s, std::vector<const syn::Ident *> &labels)
{
    std::visit(
        [&](const auto &x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, syn::stmt::Labeled>) {
                labels.push_back(&x.label);
                collect_labels(*x.stmt, labels);
            } else if constexpr (std::is_same_v<T, syn::stmt::Compound>) {
                for (const auto &b : x.body)
                    collect_labels(*b, labels);
            } else if constexpr (std::is_same_v<T, syn::stmt::If>) {
                collect_labels(*x.then_stmt, labels);
                if (x.else_stmt)
                    collect_labels(*x.else_stmt, labels);
            } else if constexpr (std::is_same_v<T, syn::stmt::For>) {
                collect_labels(*x.body, labels);
            }
        },
        s.node);
}

void Checker::declare_labels(const Stmts &body, DeclList &out)
{
    std::vector<const syn::Ident *> labels;
    for (const auto *s : body)
        collect_labels(*s, labels);
    for (const auto *l : labels) {
        Symbol s;
        s.kind = Kind::LABEL;
        int i  = declare(l->line, l->name, s);
        out.push_back(make_decl(SrcLoc{ l->line, 0 }, decl::Label{ l->name, sym(i).id }));
    }
}

// The formals and the body of a procedure.
void Checker::procedure_body(const syn::decl::Procedure &p, int index, decl::Procedure &out)
{
    scopes_.emplace_back();
    for (int f : symbols_[index].formals) {
        const Symbol &s = symbols_[f];
        if (s.mode == Mode::VALUE &&
            (s.kind == Kind::SWITCH || s.kind == Kind::PROCEDURE || s.type == Type::STRING))
            error(p.name.line, "Formal parameter " + quoted(s.name) + " cannot be called by value");
        scopes_.back().names[s.name] = f;
        Formal formal;
        formal.name      = s.name;
        formal.id        = s.id;
        formal.by_value  = s.mode == Mode::VALUE;
        formal.spec.type = s.type;
        switch (s.kind) {
        case Kind::SIMPLE:
            formal.spec.kind = Spec::SIMPLE;
            break;
        case Kind::ARRAY:
            formal.spec.kind = Spec::ARRAY;
            break;
        case Kind::PROCEDURE:
            formal.spec.kind = Spec::PROCEDURE;
            break;
        case Kind::LABEL:
            formal.spec.kind = Spec::LABEL;
            break;
        case Kind::SWITCH:
            formal.spec.kind = Spec::SWITCH;
            break;
        }
        out.formals.push_back(std::move(formal));
    }
    functions_.push_back(index);
    const syn::Stmt &body = *p.body;
    if (auto b = std::get_if<syn::stmt::Block>(&body.node)) {
        out.body = make_block(body.loc, ptrs(b->decls), ptrs(b->body));
    } else {
        DeclList labels;
        std::vector<const syn::Ident *> list;
        collect_labels(body, list);
        for (const auto *l : list) {
            Symbol s;
            s.kind = Kind::LABEL;
            int i  = declare(l->line, l->name, s);
            labels.push_back(make_decl(SrcLoc{ l->line, 0 }, decl::Label{ l->name, sym(i).id }));
        }
        StmtPtr s = statement(body);
        if (labels.empty()) {
            out.body = std::move(s);
        } else {
            StmtList one;
            one.push_back(std::move(s));
            out.body = make_stmt(body.loc, stmt::Block{ std::move(labels), std::move(one) });
        }
    }
    functions_.pop_back();
    if (p.type != Type::NONE && !symbols_[index].assigned)
        error(p.name.line, "Function " + quoted(p.name.name) + " does not assign its result");
    scopes_.pop_back();
}

// _common /block/: its elements are variables or arrays of this block.
DeclPtr Checker::common(int line, const CommonBlock &block)
{
    decl::Common c;
    c.name = block.name;
    for (const auto &e : block.elements) {
        auto it = scopes_.back().names.find(e.name);
        if (it == scopes_.back().names.end()) {
            error(line, "_common element " + quoted(e.name) + " is not declared in its block");
            continue;
        }
        const Symbol &s = sym(it->second);
        if (s.kind != Kind::SIMPLE && s.kind != Kind::ARRAY) {
            error(line, "_common element " + quoted(e.name) + " is not a variable or an array");
            continue;
        }
        c.elements.push_back(CommonElement{ e.name, s.id, e.size });
    }
    return make_decl(SrcLoc{ line, 0 }, std::move(c));
}

//
// Statements.
//
// The statements of a list; empty ones without a label are dropped.
StmtList Checker::statements(const Stmts &list)
{
    StmtList out;
    for (const auto *s : list) {
        if (std::holds_alternative<syn::stmt::Dummy>(s->node))
            continue;
        out.push_back(statement(*s));
    }
    return out;
}

StmtList Checker::statements(const syn::StmtList &list)
{
    Stmts all = ptrs(list);
    return statements(all);
}

StmtPtr Checker::statement(const syn::Stmt &s)
{
    SrcLoc loc = s.loc;
    return std::visit(
        [&](const auto &x) -> StmtPtr {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, syn::stmt::Block>) {
                return make_block(loc, ptrs(x.decls), ptrs(x.body));
            } else if constexpr (std::is_same_v<T, syn::stmt::Compound>) {
                return make_stmt(loc, stmt::Compound{ statements(x.body) });
            } else if constexpr (std::is_same_v<T, syn::stmt::Labeled>) {
                int i  = lookup(x.label.name);
                int id = i >= 0 ? sym(i).id : 0;
                return make_stmt(loc, stmt::Labeled{ id, statement(*x.stmt) });
            } else if constexpr (std::is_same_v<T, syn::stmt::Dummy> ||
                                 std::is_same_v<T, syn::stmt::Error>) {
                return make_stmt(loc, stmt::Dummy{});
            } else if constexpr (std::is_same_v<T, syn::stmt::Assign>) {
                return assignment(loc, x);
            } else if constexpr (std::is_same_v<T, syn::stmt::Goto>) {
                return make_stmt(loc, stmt::Goto{ designational(*x.target, "_goto") });
            } else if constexpr (std::is_same_v<T, syn::stmt::If>) {
                stmt::If i;
                i.condition = boolean(*x.condition, "the condition of _if");
                i.then_stmt = statement(*x.then_stmt);
                if (x.else_stmt)
                    i.else_stmt = statement(*x.else_stmt);
                return make_stmt(loc, std::move(i));
            } else if constexpr (std::is_same_v<T, syn::stmt::For>) {
                return for_stmt(loc, x);
            } else if constexpr (std::is_same_v<T, syn::stmt::Call>) {
                return call_stmt(loc, *x.call);
            } else {
                return io_stmt(loc, x);
            }
        },
        s.node);
}

// A left part: a variable, a formal, the result of a function being
// checked, an array element. type: its type.
std::optional<LeftPart> Checker::left_part(const syn::Expr &e, Type &type)
{
    if (auto n = std::get_if<syn::expr::Name>(&e.node)) {
        int i = lookup(n->name);
        if (i < 0) {
            named(n->name, e.line, false);
            return std::nullopt;
        }
        Symbol &s = symbols_[i];
        if (s.kind == Kind::SIMPLE) {
            type = s.type;
            return left::Var{ s.id };
        }
        if (s.kind == Kind::PROCEDURE && s.type != Type::NONE) {
            if (std::find(functions_.begin(), functions_.end(), i) != functions_.end()) {
                s.assigned = true;
                type       = s.type;
                return left::Var{ s.id };
            }
        }
        error(e.line, "Cannot assign to " + quoted(n->name));
        return std::nullopt;
    }
    if (auto x = std::get_if<syn::expr::Index>(&e.node)) {
        int i = lookup(x->name);
        if (i >= 0 && sym(i).kind == Kind::ARRAY) {
            ExprPtr el = indexed(*x, e.line);
            if (auto el2 = std::get_if<expr::Element>(&el->node)) {
                type = sym(i).type;
                return left::Element{ el2->array, std::move(el2->subscripts) };
            }
            return std::nullopt;
        }
        if (i < 0)
            named(x->name, e.line, false);
        else
            error(e.line, "Cannot assign to an element of " + quoted(x->name));
        return std::nullopt;
    }
    error(e.line, "Illegal left part of an assignment");
    return std::nullopt;
}

// Left parts of the same type; the value converted to it.
StmtPtr Checker::assignment(SrcLoc loc, const syn::stmt::Assign &a)
{
    stmt::Assign out;
    Type type = Type::NONE;
    bool ok   = true;
    for (const auto &t : a.targets) {
        Type tt = Type::NONE;
        auto lp = left_part(*t, tt);
        if (!lp) {
            ok = false;
            continue;
        }
        if (type != Type::NONE && tt != type) {
            error(t->line, "Left parts of different types");
            ok = false;
        }
        type = tt;
        out.targets.push_back(std::move(*lp));
    }
    ExprPtr v = expr(*a.value);
    if (ok)
        v = convert(std::move(v), type, a.value->line, "the value of ':='");
    out.value = std::move(v);
    return make_stmt(loc, std::move(out));
}

StmtPtr Checker::for_stmt(SrcLoc loc, const syn::stmt::For &f)
{
    stmt::For out;
    int line     = f.var->line;
    Type t       = Type::NONE;
    auto control = left_part(*f.var, t);
    if (control && !numeric(t)) {
        error(line, "The variable of _for must be an integer or real variable");
        t = Type::NONE;
    }
    out.control  = control ? std::move(*control) : LeftPart{ left::Var{ 0 } };
    out.elements = for_elements(f.elements, t, line);
    out.body     = statement(*f.body);
    return make_stmt(loc, std::move(out));
}

std::vector<ForElement> Checker::for_elements(const std::vector<syn::ForElement> &list, Type t,
                                              int line)
{
    std::vector<ForElement> out;
    const std::string what = "an element of _for";
    for (const auto &e : list) {
        switch (e.kind) {
        case syn::ForElement::VALUE:
            out.push_back(for_elem::Value{ convert(expr(*e.value), t, line, what) });
            break;
        case syn::ForElement::STEP_UNTIL:
            out.push_back(for_elem::StepUntil{ convert(expr(*e.value), t, line, what),
                                               convert(expr(*e.step), t, line, what),
                                               convert(expr(*e.until), t, line, what) });
            break;
        case syn::ForElement::WHILE:
            out.push_back(for_elem::While{ convert(expr(*e.value), t, line, what),
                                           boolean(*e.step, "the condition of _while") });
            break;
        }
    }
    return out;
}

// A procedure statement: a call, or a standard procedure.
StmtPtr Checker::call_stmt(SrcLoc loc, const syn::Expr &e)
{
    const std::string &nm     = std::holds_alternative<syn::expr::Name>(e.node)
                                    ? std::get<syn::expr::Name>(e.node).name
                                    : std::get<syn::expr::Call>(e.node).name;
    const syn::ExprList *args = nullptr;
    if (auto c = std::get_if<syn::expr::Call>(&e.node))
        args = &c->args;
    int i = lookup(nm);
    if (i < 0) {
        if (Std s = standard(nm); s != Std::NONE)
            return std_stmt(loc, s, nm, args, e.line);
        named(nm, e.line, false);
        return make_stmt(loc, stmt::Dummy{});
    }
    if (sym(i).kind != Kind::PROCEDURE) {
        error(e.line, quoted(nm) + " is not a procedure");
        return make_stmt(loc, stmt::Dummy{});
    }
    syn::ExprList none;
    return make_stmt(loc, stmt::Call{ sym(i).id, arguments(i, args ? *args : none, e.line) });
}

StmtPtr Checker::std_stmt(SrcLoc loc, Std s, const std::string &nm, const syn::ExprList *args,
                          int line)
{
    size_t n = args ? args->size() : 0;
    switch (s) {
    case Std::PRINT: {
        stmt::Io io;
        io.op = IoOp::PRINT;
        for (size_t k = 0; k < n; k++) {
            const syn::Expr &a = *(*args)[k];
            if (auto nn = std::get_if<syn::expr::Name>(&a.node); nn && lookup(nn->name) < 0) {
                Std ss = standard(nn->name);
                if (ss == Std::NEWLINE) {
                    io.items.push_back(IoElement{ io::NewLine{} });
                    continue;
                }
                if (ss == Std::NEWPAGE) {
                    io.items.push_back(IoElement{ io::NewPage{} });
                    continue;
                }
            }
            ExprPtr e;
            auto nn = std::get_if<syn::expr::Name>(&a.node);
            int i   = nn ? lookup(nn->name) : -1;
            if (i >= 0 && sym(i).kind == Kind::ARRAY)
                e = make_expr(sym(i).type, expr::Var{ sym(i).id });
            else
                e = expr(a);
            if (e->type == Type::NONE && !std::holds_alternative<expr::IntConst>(e->node))
                error(a.line, "Illegal argument of " + quoted(nm));
            io.items.push_back(IoElement{ io::Item{ std::move(e) } });
        }
        return make_stmt(loc, std::move(io));
    }
    case Std::NEWLINE:
    case Std::NEWPAGE: {
        if (n)
            error(line, quoted(nm) + " has no arguments");
        stmt::Io io;
        io.op = IoOp::PRINT;
        if (s == Std::NEWLINE)
            io.items.push_back(IoElement{ io::NewLine{} });
        else
            io.items.push_back(IoElement{ io::NewPage{} });
        return make_stmt(loc, std::move(io));
    }
    case Std::READ: {
        stmt::Io io;
        io.op = IoOp::READ;
        for (size_t k = 0; k < n; k++) {
            const syn::Expr &a = *(*args)[k];
            if (auto nn = std::get_if<syn::expr::Name>(&a.node)) {
                int i = lookup(nn->name);
                if (i >= 0 && sym(i).kind == Kind::ARRAY) {
                    io.items.push_back(
                        IoElement{ io::Item{ make_expr(sym(i).type, expr::Var{ sym(i).id }) } });
                    continue;
                }
            }
            Type t  = Type::NONE;
            auto lp = left_part(a, t);
            if (!lp)
                continue;
            if (auto v = std::get_if<left::Var>(&*lp))
                io.items.push_back(IoElement{ io::Item{ make_expr(t, expr::Var{ v->decl }) } });
            else {
                auto &el = std::get<left::Element>(*lp);
                io.items.push_back(IoElement{ io::Item{
                    make_expr(t, expr::Element{ el.array, std::move(el.subscripts) }) } });
            }
        }
        return make_stmt(loc, std::move(io));
    }
    case Std::STOP:
        if (n)
            error(line, quoted(nm) + " has no arguments");
        return make_stmt(loc, stmt::StdProc{ StdProc::STOP, {} });
    case Std::ASSIGN:
        if (n != 2)
            error(line, quoted(nm) + " needs two arguments");
        [[fallthrough]];
    case Std::LOAD:
    case Std::LOADGO:
    case Std::ATOS:
    case Std::STOA: {
        static const std::map<Std, StdProc> procs = {
            { Std::ASSIGN, StdProc::ASSIGN }, { Std::LOAD, StdProc::LOAD },
            { Std::LOADGO, StdProc::LOADGO }, { Std::ATOS, StdProc::ATOS },
            { Std::STOA, StdProc::STOA },
        };
        stmt::StdProc p;
        p.proc = procs.at(s);
        for (size_t k = 0; k < n; k++)
            p.args.push_back(actual(*(*args)[k], nullptr, int(k) + 1, nm));
        return make_stmt(loc, std::move(p));
    }
    default:
        error(line, "Illegal use of " + quoted(nm));
        return make_stmt(loc, stmt::Dummy{});
    }
}

// _print, _write and the other I/O statements.
StmtPtr Checker::io_stmt(SrcLoc loc, const syn::stmt::Io &io)
{
    static const std::map<Sym, IoOp> ops = {
        { Sym::READ, IoOp::READ },       { Sym::PRINT, IoOp::WRITE },
        { Sym::WRITE, IoOp::WRITE },     { Sym::PUNCH, IoOp::PUNCH },
        { Sym::DECODE, IoOp::DECODE },   { Sym::ENCODE, IoOp::ENCODE },
        { Sym::REWIND, IoOp::REWIND },   { Sym::BACKSPACE, IoOp::BACKSPACE },
        { Sym::ENDFILE, IoOp::ENDFILE },
    };
    stmt::Io out;
    out.op = ops.at(io.op);
    for (size_t k = 0; k < io.params.size(); k++)
        out.params.push_back(actual(*io.params[k], nullptr, int(k) + 1, to_string(io.op)));
    out.items = io_items(io.items, loc.line);
    return make_stmt(loc, std::move(out));
}

IoList Checker::io_items(const std::vector<syn::IoItem> &items, int line)
{
    IoList out;
    for (const auto &item : items) {
        if (item.expr) {
            out.push_back(IoElement{ io::Item{ actual(*item.expr, nullptr, 0, "") } });
            continue;
        }
        io::For f;
        int i  = lookup(item.var);
        Type t = Type::NONE;
        if (i < 0)
            named(item.var, line, false);
        else
            t = sym(i).type;
        f.control  = left::Var{ i >= 0 ? sym(i).id : 0 };
        f.elements = for_elements(item.elements, t, line);
        f.body     = io_items(item.items, line);
        out.push_back(IoElement{ std::move(f) });
    }
    return out;
}

//
// Expressions.
//
ExprPtr Checker::convert(ExprPtr e, Type to, int line, const std::string &what)
{
    if (e->type == to || e->type == Type::NONE || to == Type::NONE)
        return e;
    if (e->type == Type::INTEGER && to == Type::REAL)
        return make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL, std::move(e) });
    if (e->type == Type::REAL && to == Type::INTEGER)
        return make_expr(Type::INTEGER, expr::Convert{ Conversion::REAL_TO_INT, std::move(e) });
    error(line, std::string("Type error in ") + what + ": " + to_string(e->type) + " instead of " +
                    to_string(to));
    return bad();
}

ExprPtr Checker::integer(const syn::Expr &e, const std::string &what)
{
    return convert(expr(e), Type::INTEGER, e.line, what);
}

ExprPtr Checker::boolean(const syn::Expr &e, const std::string &what)
{
    ExprPtr x = expr(e);
    if (x->type != Type::BOOLEAN && x->type != Type::NONE) {
        error(e.line, "Type error in " + what + ": " + to_string(x->type) + " instead of boolean");
        return bad();
    }
    return x;
}

// A label, a switch element or a conditional of them.
ExprPtr Checker::designational(const syn::Expr &e, const std::string &what)
{
    if (auto c = std::get_if<syn::expr::If>(&e.node)) {
        ExprPtr cond = boolean(*c->condition, "the condition of _if");
        ExprPtr a    = designational(*c->then_expr, what);
        ExprPtr b    = designational(*c->else_expr, what);
        return make_expr(Type::NONE, expr::Cond{ std::move(cond), std::move(a), std::move(b) });
    }
    // An unsigned integer is a label.
    auto n    = std::get_if<syn::expr::Int>(&e.node);
    ExprPtr x = n ? named(std::to_string(n->value), e.line, false) : expr(e);
    if (!std::holds_alternative<expr::LabelRef>(x->node) &&
        !std::holds_alternative<expr::SwitchElement>(x->node)) {
        if (!(x->type == Type::NONE && std::holds_alternative<expr::IntConst>(x->node)))
            error(e.line, "A label is expected in " + what);
        return bad();
    }
    return x;
}

ExprPtr Checker::expr(const syn::Expr &e)
{
    int line = e.line;
    return std::visit(
        [&](const auto &x) -> ExprPtr {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, syn::expr::Int>) {
                return make_expr(Type::INTEGER, expr::IntConst{ x.value });
            } else if constexpr (std::is_same_v<T, syn::expr::Real>) {
                return make_expr(Type::REAL, expr::RealConst{ x.value });
            } else if constexpr (std::is_same_v<T, syn::expr::Bits>) {
                return make_expr(Type::BITS, expr::BitsConst{ x.value });
            } else if constexpr (std::is_same_v<T, syn::expr::Bool>) {
                return make_expr(Type::BOOLEAN, expr::BoolConst{ x.value });
            } else if constexpr (std::is_same_v<T, syn::expr::String>) {
                return make_expr(Type::STRING, expr::StringConst{ x.value });
            } else if constexpr (std::is_same_v<T, syn::expr::Name>) {
                return named(x.name, line, false);
            } else if constexpr (std::is_same_v<T, syn::expr::Index>) {
                return indexed(x, line);
            } else if constexpr (std::is_same_v<T, syn::expr::Slice>) {
                ExprPtr s = named(x.name, line, false);
                if (s->type != Type::STRING) {
                    if (s->type != Type::NONE)
                        error(line, "A substring of " + quoted(x.name) + ", not a string");
                    return bad();
                }
                ExprPtr first = integer(*x.first, "a substring");
                ExprPtr last  = integer(*x.last, "a substring");
                return make_expr(Type::STRING,
                                 expr::Slice{ std::move(s), std::move(first), std::move(last) });
            } else if constexpr (std::is_same_v<T, syn::expr::Call>) {
                return call(x.name, x.args, line);
            } else if constexpr (std::is_same_v<T, syn::expr::Unary>) {
                ExprPtr a = expr(*x.operand);
                if (a->type == Type::NONE)
                    return a;
                if (x.op == Sym::NOT) {
                    if (a->type != Type::BOOLEAN && a->type != Type::BITS) {
                        error(line, "Type error in _not: " + std::string(to_string(a->type)));
                        return bad();
                    }
                    Type t = a->type;
                    return make_expr(t, expr::Unary{ UnaryOp::NOT, std::move(a) });
                }
                if (!numeric(a->type)) {
                    error(line, std::string("Type error in ") + op_text(x.op) + ": " +
                                    to_string(a->type));
                    return bad();
                }
                if (x.op == Sym::PLUS)
                    return a;
                Type t = a->type;
                return make_expr(t, expr::Unary{ UnaryOp::NEG, std::move(a) });
            } else if constexpr (std::is_same_v<T, syn::expr::Binary>) {
                return binary(x, line);
            } else {
                ExprPtr c = boolean(*x.condition, "the condition of _if");
                ExprPtr a = expr(*x.then_expr);
                ExprPtr b = expr(*x.else_expr);
                if (a->type == Type::NONE || b->type == Type::NONE) {
                    bool labels = std::holds_alternative<expr::LabelRef>(a->node) ||
                                  std::holds_alternative<expr::SwitchElement>(a->node);
                    if (!labels)
                        return bad();
                    return make_expr(Type::NONE,
                                     expr::Cond{ std::move(c), std::move(a), std::move(b) });
                }
                Type t = a->type;
                if (a->type != b->type) {
                    if (numeric(a->type) && numeric(b->type)) {
                        t = Type::REAL;
                        a = convert(std::move(a), t, line, "");
                        b = convert(std::move(b), t, line, "");
                    } else {
                        error(line, "Branches of a conditional expression of different types");
                        return bad();
                    }
                }
                return make_expr(t, expr::Cond{ std::move(c), std::move(a), std::move(b) });
            }
        },
        e.node);
}

ExprPtr Checker::binary(const syn::expr::Binary &b, int line)
{
    ExprPtr l = expr(*b.left);
    ExprPtr r = expr(*b.right);
    if (l->type == Type::NONE || r->type == Type::NONE)
        return bad();
    auto fail = [&]() {
        error(line, std::string("Type error in ") + op_text(b.op) + ": " + to_string(l->type) +
                        " and " + to_string(r->type));
        return bad();
    };
    auto make = [&](BinaryOp op, Type t) {
        return make_expr(t, expr::Binary{ op, std::move(l), std::move(r) });
    };
    // Both numbers: an integer one converted when the other is real.
    auto unify = [&]() {
        if (l->type == Type::INTEGER && r->type == Type::REAL)
            l = convert(std::move(l), Type::REAL, line, "");
        else if (l->type == Type::REAL && r->type == Type::INTEGER)
            r = convert(std::move(r), Type::REAL, line, "");
        return l->type;
    };
    switch (b.op) {
    case Sym::PLUS:
    case Sym::MINUS:
    case Sym::TIMES: {
        BinaryOp op = b.op == Sym::PLUS    ? BinaryOp::ADD
                      : b.op == Sym::MINUS ? BinaryOp::SUB
                                           : BinaryOp::MUL;
        if (op == BinaryOp::ADD && l->type == Type::STRING && r->type == Type::STRING)
            return make(op, Type::STRING);
        if (!numeric(l->type) || !numeric(r->type))
            return fail();
        Type t = unify();
        return make(op, t);
    }
    case Sym::SLASH:
        if (!numeric(l->type) || !numeric(r->type))
            return fail();
        l = convert(std::move(l), Type::REAL, line, "");
        r = convert(std::move(r), Type::REAL, line, "");
        return make(BinaryOp::DIV, Type::REAL);
    case Sym::INTDIV:
        if (l->type != Type::INTEGER || r->type != Type::INTEGER)
            return fail();
        return make(BinaryOp::INT_DIV, Type::INTEGER);
    case Sym::POWER: {
        if (l->type == Type::BITS && r->type == Type::INTEGER)
            return make(BinaryOp::POWER, Type::BITS);
        if (!numeric(l->type) || !numeric(r->type))
            return fail();
        // An integer to a constant power not below 0 stays integer.
        auto k = std::get_if<expr::IntConst>(&r->node);
        if (l->type == Type::INTEGER && k && k->value >= 0)
            return make(BinaryOp::POWER, Type::INTEGER);
        l = convert(std::move(l), Type::REAL, line, "");
        return make(BinaryOp::POWER, Type::REAL);
    }
    case Sym::LT:
    case Sym::LE:
    case Sym::EQ:
    case Sym::NE:
    case Sym::GE:
    case Sym::GT: {
        static const std::map<Sym, BinaryOp> rel = {
            { Sym::LT, BinaryOp::LT }, { Sym::LE, BinaryOp::LE }, { Sym::EQ, BinaryOp::EQ },
            { Sym::NE, BinaryOp::NE }, { Sym::GE, BinaryOp::GE }, { Sym::GT, BinaryOp::GT },
        };
        BinaryOp op = rel.at(b.op);
        if (numeric(l->type) && numeric(r->type)) {
            unify();
            return make(op, Type::BOOLEAN);
        }
        if (l->type == Type::STRING && r->type == Type::STRING)
            return make(op, Type::BOOLEAN);
        if (l->type == Type::BITS && r->type == Type::BITS &&
            (op == BinaryOp::EQ || op == BinaryOp::NE))
            return make(op, Type::BOOLEAN);
        return fail();
    }
    case Sym::AND:
    case Sym::OR: {
        BinaryOp op = b.op == Sym::AND ? BinaryOp::AND : BinaryOp::OR;
        if (l->type == r->type && (l->type == Type::BOOLEAN || l->type == Type::BITS)) {
            Type t = l->type;
            return make(op, t);
        }
        return fail();
    }
    case Sym::IMPL:
    case Sym::EQUIV:
        if (l->type != Type::BOOLEAN || r->type != Type::BOOLEAN)
            return fail();
        return make(b.op == Sym::IMPL ? BinaryOp::IMPL : BinaryOp::EQUIV, Type::BOOLEAN);
    default:
        return fail();
    }
}

// A name alone: a variable, a call without arguments, a label; an array, a
// switch or a procedure only as an argument.
ExprPtr Checker::named(const std::string &nm, int line, bool argument)
{
    int i = lookup(nm);
    if (i < 0) {
        Std s = standard(nm);
        if (s == Std::NONE) {
            if (undeclared_.insert(nm).second)
                error(line, "Undeclared identifier " + quoted(nm));
        } else {
            error(line, "Illegal use of " + quoted(nm));
        }
        return bad();
    }
    const Symbol &s = sym(i);
    switch (s.kind) {
    case Kind::SIMPLE:
        return make_expr(s.type, expr::Var{ s.id });
    case Kind::LABEL:
        return make_expr(Type::NONE, expr::LabelRef{ s.id });
    case Kind::PROCEDURE:
        if (argument)
            return make_expr(s.type, expr::Var{ s.id });
        if (s.count > 0) {
            error(line, quoted(nm) + " needs arguments");
            return bad();
        }
        if (s.type == Type::NONE) {
            error(line, "Procedure " + quoted(nm) + " has no value");
            return bad();
        }
        return make_expr(s.type, expr::Call{ s.id, {} });
    case Kind::ARRAY:
    case Kind::SWITCH:
        if (argument)
            return make_expr(s.type, expr::Var{ s.id });
        error(line, "Illegal use of " + quoted(nm));
        return bad();
    }
    return bad();
}

// a[i, j]: an array element, a switch element, a character of a string.
ExprPtr Checker::indexed(const syn::expr::Index &x, int line)
{
    int i = lookup(x.name);
    if (i < 0) {
        named(x.name, line, false);
        return bad();
    }
    const Symbol &s = sym(i);
    if (s.kind == Kind::ARRAY) {
        if (s.count >= 0 && size_t(s.count) != x.subscripts.size()) {
            error(line, "Wrong number of subscripts of " + quoted(x.name));
            return bad();
        }
        ExprList subs(x.subscripts.size());
        std::transform(x.subscripts.begin(), x.subscripts.end(), subs.begin(),
                       [this](const auto &e) { return integer(*e, "a subscript"); });
        return make_expr(s.type, expr::Element{ s.id, std::move(subs) });
    }
    if (x.subscripts.size() != 1) {
        error(line, "Wrong number of subscripts of " + quoted(x.name));
        return bad();
    }
    if (s.kind == Kind::SWITCH)
        return make_expr(Type::NONE,
                         expr::SwitchElement{ s.id, integer(*x.subscripts[0], "a subscript") });
    if (s.kind == Kind::SIMPLE && s.type == Type::STRING)
        return make_expr(Type::STRING,
                         expr::StringElement{ make_expr(Type::STRING, expr::Var{ s.id }),
                                              integer(*x.subscripts[0], "a subscript") });
    error(line, quoted(x.name) + " has no subscripts");
    return bad();
}

// f(args): a function, a standard function.
ExprPtr Checker::call(const std::string &nm, const syn::ExprList &args, int line)
{
    int i = lookup(nm);
    if (i < 0) {
        Std s     = standard(nm);
        StdFunc f = StdFunc::ABS;
        Type t    = Type::NONE;
        if (std_function(s, f, t) || s == Std::LENGTH) {
            if (args.size() != 1) {
                error(line, quoted(nm) + " needs one argument");
                return bad();
            }
            ExprPtr a = expr(*args[0]);
            if (a->type == Type::NONE)
                return a;
            if (s == Std::LENGTH) {
                if (a->type != Type::STRING) {
                    error(line, "The argument of " + quoted(nm) + " must be a string");
                    return bad();
                }
                f = StdFunc::LENGTH;
                t = Type::INTEGER;
            } else if (!numeric(a->type)) {
                error(line, "The argument of " + quoted(nm) + " must be a number");
                return bad();
            }
            ExprList list;
            list.push_back(std::move(a));
            return make_expr(t, expr::StdFunc{ f, std::move(list) });
        }
        named(nm, line, false);
        return bad();
    }
    const Symbol &s = sym(i);
    if (s.kind != Kind::PROCEDURE) {
        error(line, quoted(nm) + " is not a procedure");
        return bad();
    }
    if (s.type == Type::NONE) {
        error(line, "Procedure " + quoted(nm) + " has no value");
        return bad();
    }
    Type t = s.type;
    int id = s.id;
    return make_expr(t, expr::Call{ id, arguments(i, args, line) });
}

// The arguments of a call of procedure index, against its formals if known.
ExprList Checker::arguments(int index, const syn::ExprList &args, int line)
{
    const Symbol &p = sym(index);
    ExprList out;
    bool known = p.mode == Mode::LOCAL && p.count >= 0;
    if (known && size_t(p.count) != args.size()) {
        error(line, "Wrong number of arguments of " + quoted(p.name));
        known = false;
    }
    for (size_t k = 0; k < args.size(); k++) {
        const Symbol *formal = known ? &sym(p.formals[k]) : nullptr;
        out.push_back(actual(*args[k], formal, int(k) + 1, p.name));
    }
    return out;
}

// An actual of type a for a formal of type f: numbers are converted.
static bool compatible(Type a, Type f)
{
    return f == Type::NONE || a == f || (numeric(a) && numeric(f));
}

// An argument for the formal (null: not known).
ExprPtr Checker::actual(const syn::Expr &e, const Symbol *formal, int n, const std::string &proc)
{
    auto mismatch = [&]() {
        error(e.line, "Argument " + std::to_string(n) + " of " + quoted(proc) +
                          " does not match formal parameter " + quoted(formal->name));
        return bad();
    };
    if (formal && formal->kind == Kind::LABEL && std::holds_alternative<syn::expr::Int>(e.node))
        return designational(e, "argument " + std::to_string(n) + " of " + quoted(proc));
    ExprPtr x;
    if (auto nm = std::get_if<syn::expr::Name>(&e.node))
        x = named(nm->name, e.line, true);
    else
        x = expr(e);
    if (!formal || formal->any ||
        (x->type == Type::NONE && std::holds_alternative<expr::IntConst>(x->node)))
        return x;

    // What the argument is.
    Kind kind    = Kind::SIMPLE;
    bool by_name = false;
    if (auto v = std::get_if<expr::Var>(&x->node)) {
        const Symbol &s = symbols_[v->decl - 1];
        if (s.any)
            return x;
        kind    = s.kind;
        by_name = true;
    } else if (std::holds_alternative<expr::LabelRef>(x->node) ||
               std::holds_alternative<expr::SwitchElement>(x->node) ||
               (x->type == Type::NONE && std::holds_alternative<expr::Cond>(x->node))) {
        kind = Kind::LABEL;
    }
    switch (formal->kind) {
    case Kind::SIMPLE:
        if (kind != Kind::SIMPLE && !(by_name && kind == Kind::PROCEDURE && x->type != Type::NONE))
            return mismatch();
        if (by_name && kind == Kind::PROCEDURE)
            x = make_expr(x->type, expr::Call{ std::get<expr::Var>(x->node).decl, {} });
        if (x->type == formal->type || formal->type == Type::NONE)
            return x;
        // By name, the cell keeps its type, converted on each access.
        if (numeric(x->type) && numeric(formal->type)) {
            if (formal->mode == Mode::VALUE)
                return convert(std::move(x), formal->type, e.line, "");
            return x;
        }
        return mismatch();
    case Kind::ARRAY:
        if (!by_name || kind != Kind::ARRAY || !compatible(x->type, formal->type))
            return mismatch();
        return x;
    case Kind::PROCEDURE:
        if (!by_name || kind != Kind::PROCEDURE || !compatible(x->type, formal->type))
            return mismatch();
        return x;
    case Kind::LABEL:
        if (kind != Kind::LABEL)
            return mismatch();
        return x;
    case Kind::SWITCH:
        if (!by_name || kind != Kind::SWITCH)
            return mismatch();
        return x;
    }
    return x;
}

} // namespace

ast::Program check(const syn::Program &program, Diagnostics &diag)
{
    return Checker(diag).program(program);
}
