//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The AST from the pass-2 stream. Read forward, the stream is mostly in
// prefix order: an operator before its operands, a statement's item before
// its parts. Exceptions: subscripts come before their array item, the
// arguments of a call before its procedure item (after the end item 0110
// or 0121). Blocks without arrays leave no items: their declarations go to
// the enclosing block that has them (or the procedure body, the program).
//
#include "ast_builder.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>

#include "passes.h"
#include "symbols.h"

namespace ast {

namespace {

// Item codes of the pass-2 stream (doc/passes.md, "Output stream").
enum : unsigned {
    IMPL           = 001,
    OR             = 002,
    AND            = 003, // count 1: on bits
    NOT            = 004, // count 1: on bits
    LT             = 005,
    LE             = 006,
    GE             = 007,
    GT             = 010,
    EQ             = 011,
    NE             = 012,
    RADD           = 013,
    RSUB           = 014,
    RMUL           = 015,
    RDIV           = 016,
    VALUE          = 017,
    CALL           = 020,
    CALL0          = 021,
    NAME_VALUE     = 022,
    CONST          = 023,
    LABEL          = 024,
    SWITCH_ELEMENT = 025,
    INEG           = 026,
    IF             = 027,
    SUBSCRIPT      = 030,
    ELSE           = 031,
    FOR_VAR        = 032,
    DO             = 033,
    STEP_INTEGER   = 034,
    WHILE          = 035,
    BEGIN          = 036,
    END            = 037,
    ARRAY          = 040,
    SWITCH         = 041,
    PROCEDURE      = 042,
    EQUIV          = 043,
    ELEMENT        = 044,
    CONST_SUB      = 045,
    ELEMENT_LEFT   = 046,
    RNEG           = 047,
    SLICE          = 050, // count 1: a string element
    MARK           = 051,
    FOR_END        = 052,
    END_IF         = 053,
    END_ELSE       = 054,
    FOR            = 055,
    FOR_VALUE      = 056,
    LOAD           = 057,
    DEFINE_LABEL   = 060,
    VALUE_FORMAL   = 061,
    PROCEDURE_END  = 062,
    TO_INTEGER     = 063,
    TO_REAL        = 064,
    CODE_FUNCTION  = 065,
    CODE_STRING    = 066,
    IADD           = 067,
    STRING         = 070,
    STD            = 071,
    STD2           = 072,
    IO             = 073,
    CODE_STR_EXPR  = 074,
    EOP            = 075,
    ISUB           = 076,
    IMUL           = 077,
    ASSIGN         = 0100,
    ASSIGN_NAME    = 0101,
    BLOCK_ARRAYS   = 0102,
    CODE_ADDRESS   = 0103,
    BOUNDS_END     = 0104,
    EQ_MIXED       = 0105,
    NE_MIXED       = 0106,
    CALL0_MARKED   = 0107,
    CALL_END       = 0110,
    ARG_CONST      = 0111,
    ARG_VAR        = 0112,
    ARG_PROCEDURE  = 0113,
    ARG_LABEL      = 0114,
    ARG_ARRAY      = 0115,
    THUNK_END      = 0116,
    THUNK          = 0117,
    ARG_EXPR       = 0120,
    CODE_END       = 0121,
    CODE_VAR       = 0122,
    CODE_ARRAY     = 0123,
    CODE_ELEMENT   = 0124,
    CODE_EXPR      = 0125,
    CODE_CALL      = 0126,
    VALUE_START    = 0127,
    JUMP_LT        = 0130,
    JUMP_EQ        = 0134,
    FOR_EXPR_NAME  = 0135,
    FOR_EXPR       = 0136,
    STEP_CONST     = 0137,
    STEP_REAL      = 0140,
    FOR_NAME       = 0141,
    SW_LABEL       = 0142,
    SW_END         = 0143,
    SW_EXPR        = 0144,
    JUMP_TARGET    = 0145,
    JUMP           = 0146,
    POWER_RR       = 0147, // count 1: bits ** real
    POWER_RI       = 0150, // count 1: bits ** integer
    POWER_II       = 0151,
    POWER_RC       = 0152, // count 1: a shift of bits
    POWER_IC       = 0153,
    VALUE_REAL     = 0154,
    VALUE_END      = 0155,
    ARG_STRING     = 0156,
    STRING_OP      = 0157,
};

// Values of STD and STD2 items.
enum : unsigned {
    STD_PRINT_REAL   = 011, // PRINT of an expression: 010 + type
    STD_PRINT_BITS   = 015,
    STD_PRINT_ARRAY  = 020, // PRINT of an array: 017 + type
    STD_PRINT_ARRAY5 = 024,
    STD_LENGTH       = 0100,
    STD_READ_REAL    = 073, // READ of a simple variable: 072 + type
    STD_READ_BITS    = 077,
    STD_IO_NAME      = 030, // I/O list: a string variable, an array
    STD_IO_ARRAY     = 031,
    STD2_IO_ITEM     = 027, // I/O list: after an element
    STD2_IO_END      = 032,
    STD2_NEWLINE     = 057,
    STD2_NEWPAGE     = 016,
    STD2_READ        = 062,
    STD2_READ_LEFT   = 051, // READ to a left part: 051 + type
    IO_WRITE         = 060,
};

const ast::StdFunc STD_FUNCTIONS[] = {
    ast::StdFunc::SIN, ast::StdFunc::COS,    ast::StdFunc::SQRT,
    ast::StdFunc::LN,  ast::StdFunc::ARCTAN, ast::StdFunc::EXP,
    ast::StdFunc::ABS, ast::StdFunc::SIGN,   ast::StdFunc::ENTIER,
};

// Types by the type code of pass 2: 1 real … 5 bits.
const Type TYPE_CODES[] = { Type::NONE,    Type::REAL,   Type::INTEGER,
                            Type::BOOLEAN, Type::STRING, Type::BITS };

Type type_of(::Type t)
{
    switch (t) {
    case ::Type::REAL:
        return Type::REAL;
    case ::Type::INTEGER:
        return Type::INTEGER;
    case ::Type::BOOLEAN:
        return Type::BOOLEAN;
    case ::Type::STRING:
        return Type::STRING;
    case ::Type::BITS:
        return Type::BITS;
    default:
        return Type::NONE;
    }
}

int signed12(unsigned v)
{
    return v >= 04000 ? int(v) - 010000 : int(v);
}

class Builder {
public:
    Builder(const Context &ctx, const Stream &stream);
    Program build();

private:
    Symbols sym_;
    const Stream &in_;
    size_t pos_{};
    SrcLoc loc_;
    std::map<unsigned, int> ids_;    // by D-list address
    std::set<unsigned> delimited_;   // blocks with items 036 … 037
    std::vector<DeclList *> decls_;  // of the blocks being built
    std::set<unsigned> common_vars_; // declarations of 'COMMON' elements

    [[noreturn]] void error(const std::string &msg) const;
    Item peek() const;
    Item next();
    Item expect(unsigned code);
    void skip();

    void number(unsigned decl);
    void number_scope(unsigned scope);
    unsigned resolve(unsigned addr) const;
    int id(unsigned addr) const;
    const ::Decl &decl(unsigned addr) const;
    Type decl_type(unsigned addr) const { return type_of(decl(addr).type); }

    void collect_decls(unsigned scope, DeclList &out);
    DeclPtr common_decl(const ::Decl &d);

    static bool ends(unsigned code);
    StmtList statements(std::initializer_list<unsigned> ends);
    static StmtPtr single(StmtList list, SrcLoc loc);
    StmtPtr statement();
    StmtPtr block(SrcLoc loc);
    void arrays();
    void switch_decl();
    void procedure();
    StmtPtr assignment(SrcLoc loc);
    StmtPtr if_stmt(SrcLoc loc);
    StmtPtr for_stmt(SrcLoc loc);
    std::vector<ForElement> for_elements(Type t);
    ExprPtr for_value(Type t);
    StmtPtr call_stmt(SrcLoc loc);
    StmtPtr code_stmt(SrcLoc loc);
    StmtPtr std_stmt(SrcLoc loc);
    IoList io_items();

    ExprPtr expr(Type hint);
    ExprPtr constant(unsigned slot, Type hint);
    ExprPtr condition();
    static ExprPtr relation(BinaryOp op, ExprPtr left, ExprPtr right);
    ExprPtr binary(BinaryOp op, Type type, Type operands);
    ExprList subscripts();
    ExprList actuals(std::vector<std::pair<size_t, unsigned>> &consts);
    ExprPtr call(unsigned proc, ExprList args, const std::vector<std::pair<size_t, unsigned>> &c);
    ExprList code_args();
    ExprPtr left_expr(LeftPart lp);
    Type left_type(const LeftPart &lp) const;
};

Builder::Builder(const Context &ctx, const Stream &stream) : sym_(ctx), in_(stream)
{
    for (const auto &item : in_)
        if (item.code() == BEGIN)
            delimited_.insert(item.value());
    for (const auto &e : sym_.common)
        if (e.slot)
            common_vars_.insert(e.decl);
    number_scope(sym_.root);
}

void Builder::error(const std::string &msg) const
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "AST: item %zu (%08o): ", pos_ ? pos_ - 1 : 0,
                  pos_ && pos_ <= in_.size() ? in_[pos_ - 1].bits : 0);
    throw std::runtime_error(buf + msg);
}

Item Builder::peek() const
{
    if (pos_ >= in_.size())
        throw std::runtime_error("AST: unexpected end of the stream");
    return in_[pos_];
}

Item Builder::next()
{
    Item item = peek();
    pos_++;
    return item;
}

Item Builder::expect(unsigned code)
{
    Item item = next();
    if (item.code() != code) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "item %03o expected", code);
        error(buf);
    }
    return item;
}

// Statement markers and items of no meaning here.
void Builder::skip()
{
    for (;;) {
        Item item = peek();
        switch (item.code()) {
        case MARK:
            loc_ = SrcLoc{ int(item.value()), int(item.count()) };
            break;
        case JUMP:
        case JUMP_TARGET:
        case BLOCK_ARRAYS:
            break;
        default:
            return;
        }
        pos_++;
    }
}

//
// Declarations: ids in the order of the source.
//
void Builder::number(unsigned addr)
{
    if (!ids_.count(addr) && sym_.is_decl(addr) && decl(addr).mode != Mode::STANDARD)
        ids_[addr] = int(ids_.size()) + 1;
}

void Builder::number_scope(unsigned scope)
{
    const Scope &s = sym_.scopes.at(scope);
    if (s.kind == ScopeKind::PROCEDURE) {
        number(s.procedure);
        for (unsigned f : sym_.formals(s.procedure))
            number(f);
    }
    for (auto it = s.items.rbegin(); it != s.items.rend(); ++it) {
        if (sym_.is_scope(*it))
            number_scope(*it);
        else
            number(*it);
    }
}

// A procedure's scope stands for the procedure (its result in the body).
unsigned Builder::resolve(unsigned addr) const
{
    auto s = sym_.scopes.find(addr);
    if (s != sym_.scopes.end() && s->second.kind == ScopeKind::PROCEDURE)
        return s->second.procedure;
    return addr;
}

int Builder::id(unsigned addr) const
{
    auto it = ids_.find(resolve(addr));
    if (it == ids_.end()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "no declaration at %05o", addr);
        error(buf);
    }
    return it->second;
}

const ::Decl &Builder::decl(unsigned addr) const
{
    auto it = sym_.decls.find(resolve(addr));
    if (it == sym_.decls.end()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "no declaration at %05o", addr);
        error(buf);
    }
    return it->second;
}

//
// The simple variables, labels and 'COMMON' blocks of a scope and of the
// scopes inside it that have no items of their own. Arrays, switches and
// procedures come from their items.
//
void Builder::collect_decls(unsigned scope, DeclList &out)
{
    const Scope &s = sym_.scopes.at(scope);
    std::set<unsigned> formals;
    if (s.kind == ScopeKind::PROCEDURE) {
        auto list = sym_.formals(s.procedure);
        formals.insert(list.begin(), list.end());
    }
    for (auto it = s.items.rbegin(); it != s.items.rend(); ++it) {
        unsigned a = *it;
        if (sym_.is_scope(a)) {
            const Scope &inner = sym_.scopes.at(a);
            if (inner.kind == ScopeKind::PROCEDURE ||
                (inner.kind == ScopeKind::BLOCK && delimited_.count(a)))
                continue;
            collect_decls(a, out);
            continue;
        }
        const ::Decl &d = decl(a);
        if (formals.count(a) || common_vars_.count(a))
            continue;
        if (d.mode == Mode::COMMON) {
            out.push_back(common_decl(d));
            continue;
        }
        if (d.mode != Mode::LOCAL)
            continue;
        if (d.kind == Kind::SIMPLE)
            out.push_back(make_decl(SrcLoc{}, decl::Var{ d.name, id(a), type_of(d.type) }));
        else if (d.kind == Kind::LABEL)
            out.push_back(make_decl(SrcLoc{}, decl::Label{ d.name, id(a) }));
    }
}

DeclPtr Builder::common_decl(const ::Decl &d)
{
    decl::Common c;
    c.name  = d.name;
    auto it = sym_.common.begin();
    while (it != sym_.common.end() && (it->slot || it->name != d.name))
        ++it;
    if (it != sym_.common.end())
        for (++it; it != sym_.common.end() && it->slot; ++it)
            c.elements.push_back(CommonElement{ it->name, id(it->decl), int(it->size) });
    return make_decl(SrcLoc{}, std::move(c));
}

//
// Statements.
//
bool Builder::ends(unsigned code)
{
    switch (code) {
    case END:
    case ELSE:
    case END_IF:
    case END_ELSE:
    case FOR_END:
    case PROCEDURE_END:
    case EOP:
        return true;
    default:
        return false;
    }
}

StmtList Builder::statements(std::initializer_list<unsigned> end)
{
    StmtList list;
    for (;;) {
        skip();
        unsigned code = peek().code();
        if (std::find(end.begin(), end.end(), code) != end.end())
            return list;
        if (ends(code))
            error("unexpected end of a statement list");
        if (StmtPtr s = statement())
            list.push_back(std::move(s));
    }
}

// A list as one statement.
StmtPtr Builder::single(StmtList list, SrcLoc loc)
{
    if (list.empty())
        return make_stmt(loc, stmt::Dummy{});
    if (list.size() == 1)
        return std::move(list[0]);
    SrcLoc first = list[0]->loc;
    return make_stmt(first, stmt::Compound{ std::move(list) });
}

// A statement, or a declaration (null).
StmtPtr Builder::statement()
{
    SrcLoc loc = loc_;
    Item item  = peek();
    switch (item.code()) {
    case DEFINE_LABEL: {
        next();
        skip();
        StmtPtr s;
        if (ends(peek().code()))
            s = make_stmt(loc_, stmt::Dummy{});
        else
            s = statement();
        if (!s)
            error("a declaration after a label");
        return make_stmt(loc, stmt::Labeled{ id(item.value()), std::move(s) });
    }
    case BEGIN:
        return block(loc);
    case ARRAY:
        arrays();
        return nullptr;
    case SW_END:
        switch_decl();
        return nullptr;
    case PROCEDURE:
        procedure();
        return nullptr;
    case ASSIGN:
    case ASSIGN_NAME:
    case CONST_SUB:
    case SUBSCRIPT:
        return assignment(loc);
    case IF:
    case JUMP_LT:
    case JUMP_LT + 1:
    case JUMP_LT + 2:
    case JUMP_LT + 3:
    case JUMP_EQ:
        return if_stmt(loc);
    case FOR:
        return for_stmt(loc);
    case CALL_END:
        return call_stmt(loc);
    case CALL0:
    case CALL0_MARKED:
        next();
        return make_stmt(loc, stmt::Call{ id(item.value()), {} });
    case CODE_END:
        return code_stmt(loc);
    case STD:
    case STD2:
        return std_stmt(loc);
    case LABEL:
        next();
        return make_stmt(loc,
                         stmt::Goto{ make_expr(Type::NONE, expr::LabelRef{ id(item.value()) }) });
    default:
        next();
        error("unknown statement");
    }
}

StmtPtr Builder::block(SrcLoc loc)
{
    unsigned head = next().value();
    stmt::Block b;
    collect_decls(head, b.decls);
    decls_.push_back(&b.decls);
    b.body = statements({ END });
    decls_.pop_back();
    next();
    return make_stmt(loc, std::move(b));
}

// 040 a, the bounds (lower and upper of each dimension), 0104.
void Builder::arrays()
{
    decl::Arrays a;
    // The count is the number of arrays, at a … a+n-1 (the last first).
    Item item = expect(ARRAY);
    for (unsigned k = item.count(); k > 0; k--) {
        unsigned addr = item.value() + k - 1;
        a.type        = decl_type(addr);
        a.arrays.push_back(ArrayName{ decl(addr).name, id(addr) });
    }
    while (peek().code() != BOUNDS_END) {
        BoundPair b;
        b.lower = expr(Type::INTEGER);
        b.upper = expr(Type::INTEGER);
        a.bounds.push_back(std::move(b));
    }
    next();
    decls_.back()->push_back(make_decl(SrcLoc{}, std::move(a)));
}

// 0143, elements (0142 l, 0144 expression), 041 s.
void Builder::switch_decl()
{
    next();
    decl::Switch s;
    for (;;) {
        Item item = next();
        if (item.code() == SW_LABEL)
            s.elements.push_back(make_expr(Type::NONE, expr::LabelRef{ id(item.value()) }));
        else if (item.code() == SW_EXPR)
            s.elements.push_back(expr(Type::NONE));
        else if (item.code() == SWITCH) {
            s.name = decl(item.value()).name;
            s.id   = id(item.value());
            break;
        } else
            error("bad switch element");
    }
    decls_.back()->push_back(make_decl(SrcLoc{}, std::move(s)));
}

// 042 head, [0155 value formals 0127], the body, 062 head.
void Builder::procedure()
{
    SrcLoc loc         = loc_;
    unsigned head      = next().value();
    const Scope &scope = sym_.scopes.at(head);
    const ::Decl &d    = decl(scope.procedure);
    decl::Procedure p;
    p.name = d.name;
    p.id   = id(scope.procedure);
    p.type = type_of(d.type);
    for (unsigned f : sym_.formals(scope.procedure)) {
        const ::Decl &fd = decl(f);
        Formal formal;
        formal.name      = fd.name;
        formal.id        = id(f);
        formal.by_value  = fd.mode == Mode::VALUE;
        formal.spec.type = type_of(fd.type);
        switch (fd.kind) {
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
        p.formals.push_back(std::move(formal));
    }
    if (peek().code() == VALUE_END) {
        while (next().code() != VALUE_START)
            ;
    }
    DeclList decls;
    collect_decls(head, decls);
    decls_.push_back(&decls);
    StmtList body = statements({ PROCEDURE_END });
    decls_.pop_back();
    next();
    if (decls.empty()) {
        p.body = single(std::move(body), loc);
    } else {
        SrcLoc first = body.empty() ? loc : body[0]->loc;
        p.body       = make_stmt(first, stmt::Block{ std::move(decls), std::move(body) });
    }
    decls_.back()->push_back(make_decl(SrcLoc{}, std::move(p)));
}

Type Builder::left_type(const LeftPart &lp) const
{
    if (auto v = std::get_if<left::Var>(&lp))
        for (const auto &[addr, i] : ids_)
            if (i == v->decl)
                return decl_type(addr);
    if (auto e = std::get_if<left::Element>(&lp))
        for (const auto &[addr, i] : ids_)
            if (i == e->array)
                return decl_type(addr);
    return Type::NONE;
}

ExprPtr Builder::left_expr(LeftPart lp)
{
    Type t = left_type(lp);
    if (auto v = std::get_if<left::Var>(&lp))
        return make_expr(t, expr::Var{ v->decl });
    auto &e = std::get<left::Element>(lp);
    return make_expr(t, expr::Element{ e.array, std::move(e.subscripts) });
}

//
// Left parts (0100 v, 0101 v, subscripts 046 a), then the value; or READ to
// them (072 051+type); or GOTO a switch element (subscripts 025 s).
//
StmtPtr Builder::assignment(SrcLoc loc)
{
    std::vector<LeftPart> targets;
    for (;;) {
        Item item = peek();
        if (item.code() == ASSIGN || item.code() == ASSIGN_NAME) {
            next();
            targets.push_back(left::Var{ id(item.value()) });
            continue;
        }
        if (item.code() != CONST_SUB && item.code() != SUBSCRIPT)
            break;
        size_t saved  = pos_;
        ExprList subs = subscripts();
        Item x        = peek();
        if (x.code() == ELEMENT_LEFT) {
            next();
            targets.push_back(left::Element{ id(x.value()), std::move(subs) });
            continue;
        }
        if (targets.empty() && x.code() == SWITCH_ELEMENT && subs.size() == 1) {
            next();
            auto target =
                make_expr(Type::NONE, expr::SwitchElement{ id(x.value()), std::move(subs[0]) });
            return make_stmt(loc, stmt::Goto{ std::move(target) });
        }
        pos_ = saved;
        break;
    }
    if (targets.empty())
        error("no left part");
    Item item = peek();
    if (item.code() == STD2 && item.value() >= STD2_READ_LEFT &&
        item.value() <= STD2_READ_LEFT + 5) {
        next();
        stmt::Io io;
        io.op = IoOp::READ;
        for (auto &t : targets)
            io.items.push_back(IoElement{ io::Item{ left_expr(std::move(t)) } });
        return make_stmt(loc, std::move(io));
    }
    stmt::Assign a;
    a.value   = expr(left_type(targets[0]));
    a.targets = std::move(targets);
    return make_stmt(loc, std::move(a));
}

// The condition, the statements, then 053; or 031, the statements, 054.
StmtPtr Builder::if_stmt(SrcLoc loc)
{
    stmt::If s;
    s.condition = condition();
    s.then_stmt = single(statements({ ELSE, END_IF }), loc);
    Item end    = next();
    if (end.code() == ELSE) {
        s.else_stmt = single(statements({ END_ELSE }), loc_);
        next();
    }
    return make_stmt(loc, std::move(s));
}

// 055 f, 032/0141 v, the elements, 033, the body, 052 f.
StmtPtr Builder::for_stmt(SrcLoc loc)
{
    next();
    Item var = next();
    if (var.code() != FOR_VAR && var.code() != FOR_NAME)
        error("FOR without a control variable");
    stmt::For s;
    s.control  = left::Var{ id(var.value()) };
    s.elements = for_elements(decl_type(var.value()));
    expect(DO);
    s.body = single(statements({ FOR_END }), loc_);
    next();
    return make_stmt(loc, std::move(s));
}

// A value of a FOR element: an operand, or 0136/0135 and an expression.
ExprPtr Builder::for_value(Type t)
{
    unsigned code = peek().code();
    if (code == FOR_EXPR || code == FOR_EXPR_NAME)
        next();
    return expr(t);
}

std::vector<ForElement> Builder::for_elements(Type t)
{
    std::vector<ForElement> list;
    while (peek().code() != DO) {
        Item item = next();
        switch (item.code()) {
        case FOR_VALUE:
            list.push_back(for_elem::Value{ for_value(t) });
            break;
        case STEP_INTEGER:
        case STEP_REAL: {
            for_elem::StepUntil s;
            s.init  = for_value(t);
            s.step  = for_value(t);
            s.until = for_value(t);
            list.push_back(std::move(s));
            break;
        }
        case STEP_CONST: {
            // The step is an integer without the exponent bits.
            for_elem::StepUntil s;
            s.init       = for_value(t);
            Word w       = sym_.constant(expect(CONST).value());
            int64_t step = int64_t(w << 23) >> 23;
            s.step       = make_expr(Type::INTEGER, expr::IntConst{ step });
            s.until      = for_value(t);
            list.push_back(std::move(s));
            break;
        }
        case WHILE: {
            for_elem::While w;
            w.value     = for_value(t);
            w.condition = expr(Type::BOOLEAN);
            list.push_back(std::move(w));
            break;
        }
        default:
            error("bad FOR element");
        }
    }
    return list;
}

// 0110, the arguments, 020 p (or 057 n: LOAD).
StmtPtr Builder::call_stmt(SrcLoc loc)
{
    next();
    std::vector<std::pair<size_t, unsigned>> consts;
    ExprList args = actuals(consts);
    Item end      = next();
    if (end.code() == LOAD)
        return make_stmt(loc, stmt::StdProc{ StdProc::LOAD, std::move(args) });
    if (end.code() != CALL)
        error("bad end of a call");
    ExprPtr c = call(end.value(), std::move(args), consts);
    return make_stmt(loc,
                     stmt::Call{ id(end.value()), std::move(std::get<expr::Call>(c->node).args) });
}

// 0121, the arguments, then 0126 p (a call) or 073 op (an I/O statement).
StmtPtr Builder::code_stmt(SrcLoc loc)
{
    next();
    ExprList args = code_args();
    Item end      = next();
    if (end.code() == CODE_CALL)
        return make_stmt(loc, stmt::Call{ id(end.value()), std::move(args) });
    if (end.code() != IO)
        error("bad end of a non-ALGOL call");
    if (end.value() != IO_WRITE)
        error("unknown I/O statement");
    stmt::Io io;
    io.op     = IoOp::WRITE;
    io.params = std::move(args);
    io.items  = io_items();
    return make_stmt(loc, std::move(io));
}

// The list of an I/O statement, up to 072 032.
IoList Builder::io_items()
{
    IoList list;
    for (;;) {
        Item item = peek();
        if (item.code() == STD2 && item.value() == STD2_IO_END) {
            next();
            return list;
        }
        if (item.code() == STD && (item.value() == STD_IO_NAME || item.value() == STD_IO_ARRAY)) {
            next();
            unsigned a = expect(VALUE).value();
            list.push_back(IoElement{ io::Item{ make_expr(decl_type(a), expr::Var{ id(a) }) } });
            continue;
        }
        ExprPtr e;
        if (item.code() == ASSIGN || item.code() == ASSIGN_NAME) {
            next();
            LeftPart lp = left::Var{ id(item.value()) };
            e           = left_expr(std::move(lp));
        } else {
            e = expr(Type::NONE);
        }
        Item end = next();
        if (end.code() != STD2 || end.value() != STD2_IO_ITEM)
            error("bad I/O list element");
        list.push_back(IoElement{ io::Item{ std::move(e) } });
    }
}

// PRINT and READ items of the standard procedures, as one statement.
StmtPtr Builder::std_stmt(SrcLoc loc)
{
    stmt::Io io;
    Item first = peek();
    if (first.code() == STD2 && first.value() == STD2_READ) {
        next();
        io.op = IoOp::READ;
        return make_stmt(loc, std::move(io));
    }
    if (first.code() == STD && first.value() >= STD_READ_REAL && first.value() <= STD_READ_BITS) {
        io.op = IoOp::READ;
        while (peek().code() == STD && peek().value() >= STD_READ_REAL &&
               peek().value() <= STD_READ_BITS) {
            next();
            unsigned a = expect(VALUE).value();
            io.items.push_back(
                IoElement{ io::Item{ make_expr(decl_type(a), expr::Var{ id(a) }) } });
        }
        return make_stmt(loc, std::move(io));
    }
    io.op = IoOp::PRINT;
    for (;;) {
        Item item  = peek();
        unsigned v = item.value();
        if (item.code() == STD && v >= STD_PRINT_REAL && v <= STD_PRINT_BITS) {
            next();
            io.items.push_back(IoElement{ io::Item{ expr(TYPE_CODES[v - 010]) } });
        } else if (item.code() == STD && v >= STD_PRINT_ARRAY && v <= STD_PRINT_ARRAY5) {
            next();
            unsigned a = expect(VALUE).value();
            io.items.push_back(
                IoElement{ io::Item{ make_expr(decl_type(a), expr::Var{ id(a) }) } });
        } else if (item.code() == STD2 && v == STD2_NEWLINE) {
            next();
            io.items.push_back(IoElement{ io::NewLine{} });
        } else if (item.code() == STD2 && v == STD2_NEWPAGE) {
            next();
            io.items.push_back(IoElement{ io::NewPage{} });
        } else {
            break;
        }
    }
    if (io.items.empty()) {
        next();
        error("unknown standard procedure item");
    }
    return make_stmt(loc, std::move(io));
}

//
// Expressions.
//

// A constant: its type from the context, else from the word (an integer
// has the exponent 0150).
ExprPtr Builder::constant(unsigned slot, Type hint)
{
    Word w = sym_.constant(slot);
    switch (hint) {
    case Type::BOOLEAN:
        return make_expr(hint, expr::BoolConst{ w != 0 });
    case Type::BITS:
        return make_expr(hint, expr::BitsConst{ w });
    case Type::REAL:
        return make_expr(hint, expr::RealConst{ Symbols::to_real(w) });
    case Type::INTEGER:
        return make_expr(hint, expr::IntConst{ Symbols::to_integer(w) });
    default:
        if (((w >> 41) & 0177) == 0150)
            return make_expr(Type::INTEGER, expr::IntConst{ Symbols::to_integer(w) });
        return make_expr(Type::REAL, expr::RealConst{ Symbols::to_real(w) });
    }
}

// Subscripts: 045 c (a constant), 030 c e (e + c).
ExprList Builder::subscripts()
{
    ExprList list;
    for (;;) {
        Item item = peek();
        if (item.code() == CONST_SUB) {
            next();
            list.push_back(make_expr(Type::INTEGER, expr::IntConst{ signed12(item.value()) }));
        } else if (item.code() == SUBSCRIPT) {
            next();
            ExprPtr e = expr(Type::INTEGER);
            int c     = signed12(item.value());
            if (c) {
                auto k = make_expr(Type::INTEGER, expr::IntConst{ c < 0 ? -c : c });
                e = make_expr(Type::INTEGER, expr::Binary{ c < 0 ? BinaryOp::SUB : BinaryOp::ADD,
                                                           std::move(e), std::move(k) });
            }
            list.push_back(std::move(e));
        } else {
            return list;
        }
    }
}

// The arguments of an ALGOL call, up to its procedure item. The constants
// (0111) are typed by the formals in call().
ExprList Builder::actuals(std::vector<std::pair<size_t, unsigned>> &consts)
{
    ExprList list;
    for (;;) {
        Item item  = peek();
        unsigned v = item.value();
        switch (item.code()) {
        case CALL:
        case LOAD:
            return list;
        case ARG_CONST:
            consts.emplace_back(list.size(), v);
            list.push_back(constant(v, Type::NONE));
            break;
        case ARG_VAR:
        case ARG_PROCEDURE:
        case ARG_ARRAY:
            list.push_back(make_expr(decl_type(v), expr::Var{ id(v) }));
            break;
        case ARG_LABEL:
            if (decl(v).kind == Kind::LABEL && decl(v).mode == Mode::LOCAL)
                list.push_back(make_expr(Type::NONE, expr::LabelRef{ id(v) }));
            else
                list.push_back(make_expr(decl_type(v), expr::Var{ id(v) }));
            break;
        case ARG_STRING:
            list.push_back(make_expr(Type::STRING, expr::StringConst{ sym_.string(v) }));
            break;
        case ARG_EXPR:
            next();
            list.push_back(expr(Type::NONE));
            continue;
        case THUNK:
            next();
            list.push_back(expr(Type::NONE));
            expect(THUNK_END);
            continue;
        default:
            next();
            error("bad argument");
        }
        next();
    }
}

ExprPtr Builder::call(unsigned proc, ExprList args,
                      const std::vector<std::pair<size_t, unsigned>> &consts)
{
    std::vector<unsigned> formals = sym_.formals(proc);
    for (const auto &[i, slot] : consts)
        if (i < formals.size())
            args[i] = constant(slot, decl_type(formals[i]));
    return make_expr(decl_type(proc), expr::Call{ id(proc), std::move(args) });
}

// The arguments of a non-ALGOL call or an I/O statement, up to its item.
ExprList Builder::code_args()
{
    ExprList list;
    for (;;) {
        Item item  = peek();
        unsigned v = item.value();
        switch (item.code()) {
        case CODE_CALL:
        case CODE_FUNCTION:
        case IO:
            return list;
        case CODE_EXPR:
            next();
            list.push_back(expr(Type::NONE));
            continue;
        case CODE_STR_EXPR:
            next();
            list.push_back(expr(Type::STRING));
            continue;
        case CODE_STRING:
            list.push_back(make_expr(Type::STRING, expr::StringConst{ sym_.string(v) }));
            break;
        case CODE_VAR:
        case CODE_ARRAY:
            list.push_back(make_expr(decl_type(v), expr::Var{ id(v) }));
            break;
        case CODE_ELEMENT: {
            next();
            ExprList subs = subscripts();
            unsigned a    = expect(CODE_ADDRESS).value();
            list.push_back(make_expr(decl_type(a), expr::Element{ id(a), std::move(subs) }));
            continue;
        }
        default:
            next();
            error("bad argument of a non-ALGOL call");
        }
        next();
    }
}

ExprPtr Builder::relation(BinaryOp op, ExprPtr left, ExprPtr right)
{
    if (left->type == Type::INTEGER && right->type == Type::REAL)
        left = make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL, std::move(left) });
    else if (left->type == Type::REAL && right->type == Type::INTEGER)
        right = make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL, std::move(right) });
    return make_expr(Type::BOOLEAN, expr::Binary{ op, std::move(left), std::move(right) });
}

ExprPtr Builder::binary(BinaryOp op, Type type, Type operands)
{
    ExprPtr left  = expr(operands);
    ExprPtr right = expr(operands);
    return make_expr(type, expr::Binary{ op, std::move(left), std::move(right) });
}

// 027 and a Boolean expression, or a compare-and-jump 0130-0134 and two operands.
ExprPtr Builder::condition()
{
    static const BinaryOp JUMPS[] = { BinaryOp::LT, BinaryOp::LE, BinaryOp::GE, BinaryOp::GT,
                                      BinaryOp::EQ };
    Item item                     = next();
    if (item.code() == IF)
        return expr(Type::BOOLEAN);
    if (item.code() < JUMP_LT || item.code() > JUMP_EQ)
        error("bad condition");
    ExprPtr left  = expr(Type::NONE);
    ExprPtr right = expr(Type::NONE);
    return relation(JUMPS[item.code() - JUMP_LT], std::move(left), std::move(right));
}

ExprPtr Builder::expr(Type hint)
{
    static const BinaryOp RELATIONS[] = { BinaryOp::LT, BinaryOp::LE, BinaryOp::GE,
                                          BinaryOp::GT, BinaryOp::EQ, BinaryOp::NE };
    Item item                         = next();
    unsigned v                        = item.value();
    bool bits                         = item.count() != 0;
    switch (item.code()) {
    case CONST:
        return constant(v, hint);
    case STRING:
        return make_expr(Type::STRING, expr::StringConst{ sym_.string(v) });
    case VALUE:
    case NAME_VALUE:
        return make_expr(decl_type(v), expr::Var{ id(v) });
    case CALL0:
    case CALL0_MARKED:
        return make_expr(decl_type(v), expr::Call{ id(v), {} });
    case LABEL:
        return make_expr(Type::NONE, expr::LabelRef{ id(v) });
    case CONST_SUB:
    case SUBSCRIPT: {
        pos_--;
        ExprList subs = subscripts();
        Item x        = next();
        if (x.code() == ELEMENT || x.code() == CODE_ADDRESS)
            return make_expr(decl_type(x.value()), expr::Element{ id(x.value()), std::move(subs) });
        if (x.code() == SWITCH_ELEMENT && subs.size() == 1)
            return make_expr(Type::NONE, expr::SwitchElement{ id(x.value()), std::move(subs[0]) });
        error("bad subscripted variable");
    }
    case CALL_END: {
        std::vector<std::pair<size_t, unsigned>> consts;
        ExprList args = actuals(consts);
        Item end      = expect(CALL);
        return call(end.value(), std::move(args), consts);
    }
    case CODE_END: {
        ExprList args = code_args();
        Item end      = expect(CODE_FUNCTION);
        return make_expr(decl_type(end.value()), expr::Call{ id(end.value()), std::move(args) });
    }
    case STD: {
        if (v == STD_LENGTH) {
            ExprList args;
            args.push_back(expr(Type::STRING));
            return make_expr(Type::INTEGER, expr::StdFunc{ StdFunc::LENGTH, std::move(args) });
        }
        if (v >= std::size(STD_FUNCTIONS))
            error("unknown standard function");
        StdFunc f = STD_FUNCTIONS[v];
        ExprList args;
        args.push_back(expr(Type::NONE));
        Type t = f == StdFunc::SIGN || f == StdFunc::ENTIER ? Type::INTEGER : Type::REAL;
        return make_expr(t, expr::StdFunc{ f, std::move(args) });
    }
    case TO_INTEGER:
        return make_expr(Type::INTEGER, expr::Convert{ Conversion::REAL_TO_INT, expr(Type::REAL) });
    case TO_REAL:
        return make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL, expr(Type::INTEGER) });
    case RNEG:
        return make_expr(Type::REAL, expr::Unary{ UnaryOp::NEG, expr(Type::REAL) });
    case INEG:
        return make_expr(Type::INTEGER, expr::Unary{ UnaryOp::NEG, expr(Type::INTEGER) });
    case NOT: {
        Type t = bits ? Type::BITS : Type::BOOLEAN;
        return make_expr(t, expr::Unary{ UnaryOp::NOT, expr(t) });
    }
    case RADD:
        return binary(BinaryOp::ADD, Type::REAL, Type::REAL);
    case RSUB:
        return binary(BinaryOp::SUB, Type::REAL, Type::REAL);
    case RMUL:
        return binary(BinaryOp::MUL, Type::REAL, Type::REAL);
    case RDIV:
        return binary(BinaryOp::DIV, Type::REAL, Type::REAL);
    case IADD:
        return binary(BinaryOp::ADD, Type::INTEGER, Type::INTEGER);
    case ISUB:
        return binary(BinaryOp::SUB, Type::INTEGER, Type::INTEGER);
    case IMUL:
        return binary(BinaryOp::MUL, Type::INTEGER, Type::INTEGER);
    case IMPL:
        return binary(BinaryOp::IMPL, Type::BOOLEAN, Type::BOOLEAN);
    case EQUIV:
        return binary(BinaryOp::EQUIV, Type::BOOLEAN, Type::BOOLEAN);
    case AND: {
        Type t = bits ? Type::BITS : Type::BOOLEAN;
        return binary(BinaryOp::AND, t, t);
    }
    case OR: {
        ExprPtr left  = expr(Type::BOOLEAN);
        Type t        = left->type == Type::BITS ? Type::BITS : Type::BOOLEAN;
        ExprPtr right = expr(t);
        return make_expr(t, expr::Binary{ BinaryOp::OR, std::move(left), std::move(right) });
    }
    case LT:
    case LE:
    case GE:
    case GT:
    case EQ:
    case NE: {
        ExprPtr left  = expr(Type::NONE);
        ExprPtr right = expr(Type::NONE);
        return relation(RELATIONS[item.code() - LT], std::move(left), std::move(right));
    }
    case EQ_MIXED:
    case NE_MIXED: {
        ExprPtr left  = expr(Type::NONE);
        ExprPtr right = expr(Type::NONE);
        return relation(item.code() == EQ_MIXED ? BinaryOp::EQ : BinaryOp::NE, std::move(left),
                        std::move(right));
    }
    case STRING_OP: {
        if (v == 7)
            return binary(BinaryOp::ADD, Type::STRING, Type::STRING);
        if (v < 1 || v > 6)
            error("bad string operator");
        return binary(RELATIONS[v - 1], Type::BOOLEAN, Type::STRING);
    }
    case POWER_RR:
        if (bits) {
            ExprPtr left = expr(Type::BITS);
            return make_expr(Type::BITS,
                             expr::Binary{ BinaryOp::POWER, std::move(left), expr(Type::REAL) });
        }
        return binary(BinaryOp::POWER, Type::REAL, Type::REAL);
    case POWER_RI: {
        Type t       = bits ? Type::BITS : Type::REAL;
        ExprPtr left = expr(t);
        return make_expr(t, expr::Binary{ BinaryOp::POWER, std::move(left), expr(Type::INTEGER) });
    }
    case POWER_II:
        return binary(BinaryOp::POWER, Type::INTEGER, Type::INTEGER);
    case POWER_RC:
    case POWER_IC: {
        Type t   = bits ? Type::BITS : item.code() == POWER_IC ? Type::INTEGER : Type::REAL;
        int c    = bits ? int(v) - 64 : int(v);
        auto exp = make_expr(Type::INTEGER, expr::IntConst{ c });
        return make_expr(t, expr::Binary{ BinaryOp::POWER, expr(t), std::move(exp) });
    }
    case SLICE: {
        ExprPtr s = expr(Type::STRING);
        if (bits)
            return make_expr(Type::STRING,
                             expr::StringElement{ std::move(s), expr(Type::INTEGER) });
        ExprPtr first = expr(Type::INTEGER);
        return make_expr(Type::STRING,
                         expr::Slice{ std::move(s), std::move(first), expr(Type::INTEGER) });
    }
    case IF:
    case JUMP_LT:
    case JUMP_LT + 1:
    case JUMP_LT + 2:
    case JUMP_LT + 3:
    case JUMP_EQ: {
        pos_--;
        ExprPtr c = condition();
        ExprPtr a = expr(hint);
        expect(ELSE);
        ExprPtr b = expr(hint);
        expect(END_ELSE);
        Type t = a->type;
        if (a->type != b->type) {
            if (a->type == Type::INTEGER && b->type == Type::REAL) {
                a = make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL, std::move(a) });
                t = Type::REAL;
            } else if (a->type == Type::REAL && b->type == Type::INTEGER) {
                b = make_expr(Type::REAL, expr::Convert{ Conversion::INT_TO_REAL, std::move(b) });
            }
        }
        return make_expr(t, expr::Cond{ std::move(c), std::move(a), std::move(b) });
    }
    default:
        error("unknown expression item");
    }
}

Program Builder::build()
{
    Program p;
    const Scope &root = sym_.scopes.at(sym_.root);
    stmt::Block b;
    for (auto it = root.items.rbegin(); it != root.items.rend(); ++it) {
        if (!sym_.is_decl(*it))
            continue;
        const ::Decl &d = decl(*it);
        if (d.mode == Mode::CODE || d.mode == Mode::ALGOL)
            p.externals.push_back(External{
                d.name, id(*it), d.mode == Mode::CODE ? External::CODE : External::ALGOL });
        else if (d.mode == Mode::COMMON)
            b.decls.push_back(common_decl(d));
    }
    skip();
    SrcLoc loc = loc_;
    if (sym_.program)
        collect_decls(sym_.program, b.decls);
    decls_.push_back(&b.decls);
    if (sym_.program && peek().code() == BEGIN && peek().value() == sym_.program) {
        next();
        b.body = statements({ END });
        next();
    } else {
        b.body = statements({ EOP });
    }
    decls_.pop_back();
    skip();
    expect(EOP);
    p.block = make_stmt(loc, std::move(b));
    return p;
}

} // namespace

} // namespace ast

ast::Program build_ast(const Context &ctx, const Stream &stream)
{
    return ast::Builder(ctx, stream).build();
}
