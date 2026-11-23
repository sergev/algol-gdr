//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The code generator: the AST to LLVM IR, after doc/codegen.md.
//

#include "codegen.h"

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>

#include "ir.h"

using namespace ast;

namespace {

constexpr int SLOT_LINK   = 0; // static link
constexpr int SLOT_HEADER = 4; // first free slot

// An Algol function: the program body or a procedure.
struct Func {
    ir::Function *fn{};
    Func *parent{};
    int slots{ SLOT_HEADER };
};

// What a declaration is, and where it lives.
struct Symbol {
    enum Kind { VAR, ARRAY, PROC, LABEL, SWITCH, FORMAL, EXTERNAL } kind{};
    Type type{ Type::NONE };
    std::string name;
    Func *owner{};      // the function whose frame or globals hold it
    int slot{ -1 };     // in the owner's frame; -1 for a global
    std::string global; // @V.id.name
    ir::Block *block{}; // LABEL: its block in the owner's function
};

// The memory type of a value type.
const char *mem_type(Type t)
{
    switch (t) {
    case Type::INTEGER:
    case Type::BITS:
        return "i64";
    case Type::REAL:
        return "double";
    case Type::BOOLEAN:
        return "i8";
    case Type::STRING:
        return "ptr";
    default:
        return "i64";
    }
}

// The register type of a value type.
const char *reg_type(Type t)
{
    return t == Type::BOOLEAN ? "i1" : mem_type(t);
}

class Codegen {
public:
    explicit Codegen(const CodegenOptions &opt) : opt_(opt), module_(opt.module_name)
    {
        module_.set_target(opt.target);
    }

    std::string run(const Program &program)
    {
        main_.fn = &module_.function("algol_main", "void", {});
        fn_      = main_.fn;
        scope_   = &main_;
        frame_   = { "ptr", "%frame" };
        declare(main_, *program.block);
        statement(*program.block);
        fn_->ret_void();
        main_.fn->stack_alloc("i64", main_.slots, 16, "frame");
        return module_.text();
    }

private:
    const CodegenOptions &opt_;
    ir::Module module_;
    Func main_;
    std::map<int, Symbol> symbols_;

    // Where code goes: the LLVM function, the Algol function whose frame
    // it works in, and the frame pointer there.
    ir::Function *fn_{};
    Func *scope_{};
    ir::Value frame_;

    [[noreturn]] static void unsupported(const std::string &what)
    {
        throw std::runtime_error(what + " is not supported yet");
    }

    //
    // Declarations: storage for every name, before any code.
    //
    void declare(Func &f, const Stmt &s)
    {
        std::visit(
            [&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, stmt::Block>) {
                    for (const auto &d : n.decls)
                        declare(f, *d);
                    for (const auto &b : n.body)
                        declare(f, *b);
                } else if constexpr (std::is_same_v<T, stmt::Compound>) {
                    for (const auto &b : n.body)
                        declare(f, *b);
                } else if constexpr (std::is_same_v<T, stmt::Labeled>) {
                    declare(f, *n.stmt);
                } else if constexpr (std::is_same_v<T, stmt::If>) {
                    declare(f, *n.then_stmt);
                    if (n.else_stmt)
                        declare(f, *n.else_stmt);
                } else if constexpr (std::is_same_v<T, stmt::For>) {
                    declare(f, *n.body);
                }
            },
            s.node);
    }

    void declare(Func &f, const Decl &d)
    {
        std::visit(
            [&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, decl::Var>) {
                    variable(f, n.id, n.name, n.type);
                } else if constexpr (std::is_same_v<T, decl::Label>) {
                    Symbol s;
                    s.kind         = Symbol::LABEL;
                    s.name         = n.name;
                    s.owner        = &f;
                    s.block        = f.fn->new_block("L." + n.name);
                    symbols_[n.id] = s;
                } else {
                    unsupported("This declaration");
                }
            },
            d.node);
    }

    void variable(Func &f, int id, const std::string &name, Type type)
    {
        Symbol s;
        s.kind  = Symbol::VAR;
        s.type  = type;
        s.name  = name;
        s.owner = &f;
        if (&f == &main_) {
            s.global = ir::global_name("V." + std::to_string(id) + "." + name);
            module_.global(s.global + " = internal global " + mem_type(type) + " " +
                           (type == Type::STRING ? "null"
                            : type == Type::REAL ? "0.0"
                                                 : "0") +
                           ", align 8");
        } else {
            s.slot = f.slots++;
        }
        symbols_[id] = s;
    }

    const Symbol &symbol(int id) const { return symbols_.at(id); }

    //
    // Memory.
    //

    // The frame of f, from the current scope up the static links.
    ir::Value frame_of(const Func *f)
    {
        ir::Value v = frame_;
        for (const Func *s = scope_; s != f; s = s->parent) {
            if (!s)
                throw std::logic_error("frame not in scope");
            v = fn_->load("ptr", fn_->gep("i64", v, ir::i64(SLOT_LINK)));
        }
        return v;
    }

    ir::Value slot(const Func *f, int k) { return fn_->gep("i64", frame_of(f), ir::i64(k)); }

    // The address of a variable.
    ir::Value address(int id)
    {
        const Symbol &s = symbol(id);
        if (s.kind != Symbol::VAR)
            unsupported("This variable");
        if (s.slot < 0)
            return { "ptr", s.global };
        return slot(s.owner, s.slot);
    }

    ir::Value load(Type t, const ir::Value &ptr)
    {
        ir::Value v = fn_->load(mem_type(t), ptr);
        return t == Type::BOOLEAN ? fn_->cast("trunc", v, "i1") : v;
    }

    void store(Type t, ir::Value v, const ir::Value &ptr)
    {
        if (t == Type::BOOLEAN)
            v = fn_->cast("zext", v, "i8");
        fn_->store(v, ptr);
    }

    // v of type from as type to: integer ↔ real.
    ir::Value convert(const ir::Value &v, Type from, Type to)
    {
        if (from == to)
            return v;
        if (from == Type::INTEGER && to == Type::REAL)
            return fn_->cast("sitofp", v, "double");
        if (from == Type::REAL && to == Type::INTEGER)
            return round(v);
        throw std::logic_error("bad conversion");
    }

    // Real to integer, as A/RTOI: ⌊x + ½⌋.
    ir::Value round(const ir::Value &v)
    {
        module_.declare("double", "llvm.floor.f64", "double");
        auto x = fn_->call("double", "@llvm.floor.f64", { fn_->binop("fadd", v, ir::real(0.5)) });
        return fn_->cast("fptosi", x, "i64");
    }

    ir::Value runtime(const std::string &ret, const std::string &name,
                      const std::vector<ir::Value> &args, const std::string &attrs = "")
    {
        std::string params;
        for (const auto &a : args)
            params += (params.empty() ? "" : ", ") + a.type;
        module_.declare(ret, name, params, attrs);
        return fn_->call(ret, "@" + name, args);
    }

    //
    // Expressions.
    //
    ir::Value expr(const Expr &e)
    {
        return std::visit(
            [&](const auto &n) -> ir::Value {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, expr::IntConst>) {
                    return ir::i64(n.value);
                } else if constexpr (std::is_same_v<T, expr::RealConst>) {
                    return ir::real(n.value);
                } else if constexpr (std::is_same_v<T, expr::BoolConst>) {
                    return ir::i1(n.value);
                } else if constexpr (std::is_same_v<T, expr::BitsConst>) {
                    return ir::i64(int64_t(n.value));
                } else if constexpr (std::is_same_v<T, expr::StringConst>) {
                    return module_.string(n.value);
                } else if constexpr (std::is_same_v<T, expr::Var>) {
                    return load(e.type, address(n.decl));
                } else if constexpr (std::is_same_v<T, expr::StdFunc>) {
                    return std_func(n);
                } else if constexpr (std::is_same_v<T, expr::Unary>) {
                    return unary(n, e.type);
                } else if constexpr (std::is_same_v<T, expr::Binary>) {
                    return binary(n, e.type);
                } else if constexpr (std::is_same_v<T, expr::Cond>) {
                    return conditional(n, e.type);
                } else if constexpr (std::is_same_v<T, expr::Convert>) {
                    ir::Value v = expr(*n.expr);
                    return n.conv == Conversion::INT_TO_REAL
                               ? convert(v, Type::INTEGER, Type::REAL)
                               : convert(v, Type::REAL, Type::INTEGER);
                } else {
                    unsupported("This expression");
                }
            },
            e.node);
    }

    ir::Value std_func(const expr::StdFunc &f)
    {
        const Expr &arg = *f.args.at(0);
        ir::Value x     = expr(arg);
        // The functions of reals take an integer as a real.
        if (arg.type == Type::INTEGER && f.func != StdFunc::SIGN && f.func != StdFunc::ENTIER)
            x = convert(x, Type::INTEGER, Type::REAL);
        auto libm       = [&](const char *name) {
            module_.declare("double", name, "double");
            return fn_->call("double", std::string("@") + name, { x });
        };
        switch (f.func) {
        case StdFunc::ABS:
            return libm("llvm.fabs.f64");
        case StdFunc::SIGN: {
            bool real    = arg.type == Type::REAL;
            ir::Value z  = real ? ir::real(0) : ir::i64(0);
            ir::Value gt = real ? fn_->fcmp("ogt", x, z) : fn_->icmp("sgt", x, z);
            ir::Value lt = real ? fn_->fcmp("olt", x, z) : fn_->icmp("slt", x, z);
            return fn_->binop("sub", fn_->cast("zext", gt, "i64"), fn_->cast("zext", lt, "i64"));
        }
        case StdFunc::ENTIER:
            if (arg.type == Type::INTEGER)
                return x;
            return fn_->cast("fptosi", libm("llvm.floor.f64"), "i64");
        case StdFunc::SQRT:
            return runtime("double", "algol_sqrt", { x });
        case StdFunc::LN:
            return runtime("double", "algol_ln", { x });
        case StdFunc::SIN:
            return libm("sin");
        case StdFunc::COS:
            return libm("cos");
        case StdFunc::ARCTAN:
            return libm("atan");
        case StdFunc::EXP:
            return libm("exp");
        case StdFunc::LENGTH:
            return runtime("i64", "algol_string_length", { x });
        }
        throw std::logic_error("bad function");
    }

    ir::Value unary(const expr::Unary &u, Type t)
    {
        ir::Value v = expr(*u.operand);
        if (u.op == UnaryOp::NOT)
            return fn_->binop("xor", v, t == Type::BITS ? ir::i64(-1) : ir::i1(true));
        if (t == Type::REAL)
            return fn_->binop("fsub", ir::real(-0.0), v);
        return fn_->binop("sub", ir::i64(0), v);
    }

    ir::Value binary(const expr::Binary &b, Type t)
    {
        Type lt     = b.left->type;
        ir::Value l = expr(*b.left);
        ir::Value r = expr(*b.right);
        bool real   = lt == Type::REAL;
        switch (b.op) {
        case BinaryOp::ADD:
            if (t == Type::STRING)
                unsupported("String concatenation");
            return fn_->binop(real ? "fadd" : "add", l, r);
        case BinaryOp::SUB:
            return fn_->binop(real ? "fsub" : "sub", l, r);
        case BinaryOp::MUL:
            return fn_->binop(real ? "fmul" : "mul", l, r);
        case BinaryOp::DIV:
            return fn_->binop("fdiv", l, r);
        case BinaryOp::POWER:
            return power(l, r, t, b.right->type);
        case BinaryOp::LT:
        case BinaryOp::LE:
        case BinaryOp::EQ:
        case BinaryOp::NE:
        case BinaryOp::GE:
        case BinaryOp::GT:
            return relation(b.op, l, r, lt);
        case BinaryOp::AND:
            return fn_->binop("and", l, r);
        case BinaryOp::OR:
            return fn_->binop("or", l, r);
        case BinaryOp::IMPL:
            return fn_->binop("or", fn_->binop("xor", l, ir::i1(true)), r);
        case BinaryOp::EQUIV:
            return fn_->binop("xor", fn_->binop("xor", l, r), ir::i1(true));
        }
        throw std::logic_error("bad operator");
    }

    ir::Value relation(BinaryOp op, const ir::Value &l, const ir::Value &r, Type t)
    {
        static const std::map<BinaryOp, std::pair<const char *, const char *>> conds = {
            { BinaryOp::LT, { "slt", "olt" } }, { BinaryOp::LE, { "sle", "ole" } },
            { BinaryOp::EQ, { "eq", "oeq" } },  { BinaryOp::NE, { "ne", "une" } },
            { BinaryOp::GE, { "sge", "oge" } }, { BinaryOp::GT, { "sgt", "ogt" } },
        };
        auto [ic, fc] = conds.at(op);
        if (t == Type::REAL)
            return fn_->fcmp(fc, l, r);
        if (t == Type::STRING) {
            auto c = runtime("i64", "algol_string_compare", { l, r });
            return fn_->icmp(ic, c, ir::i64(0));
        }
        return fn_->icmp(ic, l, r);
    }

    // ↑: integer↑integer, real↑integer, any↑real; bits↑n is a shift.
    ir::Value power(const ir::Value &l, const ir::Value &r, Type t, Type rt)
    {
        if (t == Type::BITS)
            return runtime("i64", "algol_bits_shift", { l, r });
        if (t == Type::INTEGER)
            return runtime("i64", "algol_ipow", { l, r });
        if (rt == Type::INTEGER)
            return runtime("double", "algol_rpow", { l, r });
        return runtime("double", "algol_power", { l, r });
    }

    ir::Value conditional(const expr::Cond &c, Type t)
    {
        ir::Value cond = expr(*c.condition);
        auto *yes      = fn_->new_block("if.then");
        auto *no       = fn_->new_block("if.else");
        auto *done     = fn_->new_block("if.end");
        fn_->cond_br(cond, yes, no);
        fn_->set_block(yes);
        ir::Value a = expr(*c.then_expr);
        auto *a_end = fn_->block();
        fn_->br(done);
        fn_->set_block(no);
        ir::Value b = expr(*c.else_expr);
        auto *b_end = fn_->block();
        fn_->br(done);
        fn_->set_block(done);
        return fn_->phi(reg_type(t), { { a, a_end }, { b, b_end } });
    }

    //
    // Statements.
    //
    void statement(const Stmt &s)
    {
        std::visit(
            [&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, stmt::Block> || std::is_same_v<T, stmt::Compound>) {
                    for (const auto &b : n.body)
                        statement(*b);
                } else if constexpr (std::is_same_v<T, stmt::Dummy>) {
                } else if constexpr (std::is_same_v<T, stmt::Labeled>) {
                    ir::Block *b = symbol(n.label).block;
                    fn_->br(b);
                    fn_->set_block(b);
                    statement(*n.stmt);
                } else if constexpr (std::is_same_v<T, stmt::Goto>) {
                    go_to(*n.target);
                } else if constexpr (std::is_same_v<T, stmt::If>) {
                    if_stmt(n);
                } else if constexpr (std::is_same_v<T, stmt::For>) {
                    for_loop(n.control, n.elements, [&] { statement(*n.body); });
                } else if constexpr (std::is_same_v<T, stmt::Assign>) {
                    assign(n);
                } else if constexpr (std::is_same_v<T, stmt::Io>) {
                    io(n);
                } else {
                    unsupported("This statement");
                }
            },
            s.node);
    }

    void if_stmt(const stmt::If &s)
    {
        ir::Value cond = expr(*s.condition);
        auto *yes      = fn_->new_block("then");
        auto *done     = fn_->new_block("endif");
        auto *no       = s.else_stmt ? fn_->new_block("else") : done;
        fn_->cond_br(cond, yes, no);
        fn_->set_block(yes);
        statement(*s.then_stmt);
        fn_->br(done);
        if (s.else_stmt) {
            fn_->set_block(no);
            statement(*s.else_stmt);
            fn_->br(done);
        }
        fn_->set_block(done);
    }

    void go_to(const Expr &target)
    {
        auto ref = std::get_if<expr::LabelRef>(&target.node);
        if (!ref || symbol(ref->label).owner != scope_ || fn_ != scope_->fn)
            unsupported("This goto");
        fn_->br(symbol(ref->label).block);
    }

    //
    // FOR: the body once; each element goes to it with its number in a
    // state cell, and after the body a switch on the state goes back.
    //
    void for_loop(const LeftPart &control, const std::vector<ForElement> &elements,
                  const std::function<void()> &body)
    {
        Type vt          = control_type(control);
        ir::Value state  = fn_->stack_alloc("i64");
        auto *body_block = fn_->new_block("for.body");
        auto *done       = fn_->new_block("for.end");
        std::vector<std::pair<int64_t, const ir::Block *>> back;
        auto set = [&](const Expr &e) {
            auto [ptr, t] = left_part(control);
            store(t, convert(expr(e), e.type, t), ptr);
        };
        auto enter = [&](int64_t k) {
            fn_->store(ir::i64(k), state);
            fn_->br(body_block);
            auto *cont = fn_->new_block("for.next");
            back.push_back({ k, cont });
            fn_->set_block(cont);
        };
        for (size_t k = 0; k < elements.size(); k++) {
            std::visit(
                [&](const auto &el) {
                    using T = std::decay_t<decltype(el)>;
                    if constexpr (std::is_same_v<T, for_elem::Value>) {
                        set(*el.value);
                        enter(k);
                    } else if constexpr (std::is_same_v<T, for_elem::While>) {
                        auto *loop = fn_->new_block("for.while");
                        auto *next = fn_->new_block("for.elem");
                        fn_->br(loop);
                        fn_->set_block(loop);
                        set(*el.value);
                        auto *go = fn_->new_block("for.go");
                        fn_->cond_br(expr(*el.condition), go, next);
                        fn_->set_block(go);
                        enter(k);
                        fn_->br(loop);
                        fn_->set_block(next);
                    } else {
                        set(*el.init);
                        auto *test = fn_->new_block("for.test");
                        auto *next = fn_->new_block("for.elem");
                        fn_->br(test);
                        fn_->set_block(test);
                        // (V - C) × sign(B) > 0: the end.
                        Type t         = vt == Type::REAL || el.step->type == Type::REAL ||
                                         el.until->type == Type::REAL
                                             ? Type::REAL
                                             : Type::INTEGER;
                        ir::Value v    = convert(load(vt, left_part(control).first), vt, t);
                        ir::Value b    = convert(expr(*el.step), el.step->type, t);
                        ir::Value c    = convert(expr(*el.until), el.until->type, t);
                        bool real      = t == Type::REAL;
                        ir::Value d    = fn_->binop(real ? "fsub" : "sub", v, c);
                        ir::Value zero = real ? ir::real(0) : ir::i64(0);
                        auto gt        = [&](const ir::Value &x) {
                            return real ? fn_->fcmp("ogt", x, zero) : fn_->icmp("sgt", x, zero);
                        };
                        auto lt = [&](const ir::Value &x) {
                            return real ? fn_->fcmp("olt", x, zero) : fn_->icmp("slt", x, zero);
                        };
                        ir::Value over = fn_->binop("or", fn_->binop("and", gt(b), gt(d)),
                                                    fn_->binop("and", lt(b), lt(d)));
                        auto *go       = fn_->new_block("for.go");
                        fn_->cond_br(over, next, go);
                        fn_->set_block(go);
                        enter(k);
                        // V := V + B
                        auto [ptr, ct] = left_part(control);
                        Type st       = ct == Type::REAL || el.step->type == Type::REAL ? Type::REAL
                                                                                        : Type::INTEGER;
                        ir::Value sum = fn_->binop(st == Type::REAL ? "fadd" : "add",
                                                   convert(load(ct, ptr), ct, st),
                                                   convert(expr(*el.step), el.step->type, st));
                        store(ct, convert(sum, st, ct), ptr);
                        fn_->br(test);
                        fn_->set_block(next);
                    }
                },
                elements[k]);
        }
        fn_->br(done);
        fn_->set_block(body_block);
        body();
        fn_->switch_(fn_->load("i64", state), done, back);
        fn_->set_block(done);
    }

    Type control_type(const LeftPart &control) const
    {
        if (auto v = std::get_if<left::Var>(&control))
            return symbol(v->decl).type;
        unsupported("An element as the control variable");
    }

    // The left parts first, from left to right, then the value.
    void assign(const stmt::Assign &a)
    {
        std::vector<std::pair<ir::Value, Type>> targets(a.targets.size());
        std::transform(a.targets.begin(), a.targets.end(), targets.begin(),
                       [this](const LeftPart &lp) { return left_part(lp); });
        ir::Value v = expr(*a.value);
        for (const auto &[ptr, t] : targets)
            store(t, convert(v, a.value->type, t), ptr);
    }

    std::pair<ir::Value, Type> left_part(const LeftPart &lp)
    {
        if (auto v = std::get_if<left::Var>(&lp))
            return { address(v->decl), symbol(v->decl).type };
        unsupported("An element as a left part");
    }

    void io(const stmt::Io &s)
    {
        if (s.op != IoOp::PRINT || !s.params.empty())
            unsupported("This input or output");
        io_items(s.items);
    }

    void io_items(const IoList &items)
    {
        for (const auto &item : items) {
            std::visit(
                [&](const auto &n) {
                    using T = std::decay_t<decltype(n)>;
                    if constexpr (std::is_same_v<T, io::Item>) {
                        print(*n.expr);
                    } else if constexpr (std::is_same_v<T, io::NewLine>) {
                        runtime("void", "algol_newline", {});
                    } else if constexpr (std::is_same_v<T, io::NewPage>) {
                        runtime("void", "algol_newpage", {});
                    } else {
                        for_loop(n.control, n.elements, [&] { io_items(n.body); });
                    }
                },
                item.node);
        }
    }

    void print(const Expr &e)
    {
        ir::Value v = expr(e);
        switch (e.type) {
        case Type::INTEGER:
            runtime("void", "algol_print_integer", { v });
            break;
        case Type::REAL:
            runtime("void", "algol_print_real", { v });
            break;
        case Type::BOOLEAN:
            runtime("void", "algol_print_boolean", { v });
            break;
        case Type::STRING:
            runtime("void", "algol_print_string", { v });
            break;
        case Type::BITS:
            runtime("void", "algol_print_bits", { v });
            break;
        default:
            unsupported("This item of PRINT");
        }
    }
};

} // namespace

std::string codegen(const Program &program, const CodegenOptions &opt)
{
    return Codegen(opt).run(program);
}
