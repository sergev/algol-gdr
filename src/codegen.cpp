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
constexpr int SLOT_RESULT = 3; // the value of a function
constexpr int SLOT_HEADER = 4; // first free slot

// An Algol function: the program body or a procedure.
struct Func {
    ir::Function *fn{};
    Func *parent{};
    int slots{ SLOT_HEADER };
    const decl::Procedure *proc{}; // null for the program
};

// What a declaration is, and where it lives.
struct Symbol {
    enum Kind { VAR, ARRAY, PROC, LABEL, SWITCH, FORMAL, EXTERNAL } kind{};
    Type type{ Type::NONE };
    std::string name;
    Func *owner{};                         // the function whose frame or globals hold it
    int slot{ -1 };                        // in the owner's frame; -1 for a global
    std::string global;                    // @V.id.name
    ir::Block *block{};                    // LABEL: its block in the owner's function
    int dims{};                            // ARRAY
    std::vector<const stmt::Block *> path; // LABEL: the blocks it is in
    Func *func{};                          // PROC: its function
    Spec spec;                             // FORMAL
    bool by_value{};                       // FORMAL
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
        finish(main_);
        // Procedures, also those declared while generating others.
        for (size_t i = 0; i < funcs_.size(); i++)
            procedure(*funcs_[i]);
        return module_.text();
    }

private:
    const CodegenOptions &opt_;
    ir::Module module_;
    Func main_;
    std::map<int, Symbol> symbols_;
    std::vector<std::unique_ptr<Func>> funcs_; // procedures
    int thunks_{};
    std::vector<const stmt::Block *> path_; // blocks being declared

    // Blocks being generated, with the arrays they allocated.
    struct Active {
        const stmt::Block *block{};
        std::vector<int> arrays;
    };
    std::vector<Active> active_;

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
                    path_.push_back(&n);
                    for (const auto &d : n.decls)
                        declare(f, *d);
                    for (const auto &b : n.body)
                        declare(f, *b);
                    path_.pop_back();
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
                    s.path         = path_;
                    symbols_[n.id] = s;
                } else if constexpr (std::is_same_v<T, decl::Arrays>) {
                    for (const auto &a : n.arrays)
                        array(f, a.id, a.name, n.type, int(n.bounds.size()));
                } else if constexpr (std::is_same_v<T, decl::Procedure>) {
                    declare_procedure(f, n);
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

    void declare_procedure(Func &f, const decl::Procedure &p)
    {
        funcs_.push_back(std::make_unique<Func>());
        Func &pf  = *funcs_.back();
        pf.parent = &f;
        pf.proc   = &p;
        pf.fn     = &module_.function("P." + std::to_string(p.id) + "." + p.name, "i64",
                                      { "ptr", "ptr" }, "", "internal");
        Symbol s;
        s.kind         = Symbol::PROC;
        s.type         = p.type;
        s.name         = p.name;
        s.owner        = &f;
        s.func         = &pf;
        symbols_[p.id] = s;
        for (const auto &formal : p.formals) {
            Symbol fs;
            fs.kind     = Symbol::FORMAL;
            fs.type     = formal.spec.type;
            fs.name     = formal.name;
            fs.owner    = &pf;
            fs.spec     = formal.spec;
            fs.by_value = formal.by_value;
            fs.slot     = pf.slots;
            // A value: one slot; an array: its descriptor's address; a name
            // parameter, a procedure, a switch, a label: a pair.
            bool one = formal.spec.kind == Spec::ARRAY ||
                       (formal.spec.kind == Spec::SIMPLE && formal.by_value);
            pf.slots += one ? 1 : 2;
            symbols_[formal.id] = fs;
        }
        std::vector<const stmt::Block *> saved;
        std::swap(saved, path_);
        declare(pf, *p.body);
        std::swap(saved, path_);
    }

    void array(Func &f, int id, const std::string &name, Type type, int dims)
    {
        Symbol s;
        s.kind  = Symbol::ARRAY;
        s.type  = type;
        s.name  = name;
        s.owner = &f;
        s.dims  = dims;
        int n   = 3 + 2 * dims;
        if (&f == &main_) {
            s.global = ir::global_name("V." + std::to_string(id) + "." + name);
            module_.global(s.global + " = internal global [" + std::to_string(n) +
                           " x i64] zeroinitializer, align 8");
        } else {
            s.slot = f.slots;
            f.slots += n;
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
        switch (s.kind) {
        case Symbol::VAR:
            if (s.slot < 0)
                return { "ptr", s.global };
            return slot(s.owner, s.slot);
        case Symbol::PROC:
            // The value of a function, in its body.
            return slot(s.func, SLOT_RESULT);
        case Symbol::FORMAL:
            if (s.spec.kind == Spec::SIMPLE) {
                if (s.by_value)
                    return slot(s.owner, s.slot);
                return call_thunk(slot(s.owner, s.slot));
            }
            break;
        default:
            break;
        }
        unsupported("This variable");
    }

    // The cell of a thunk, from its pair {code, env} at ptr.
    ir::Value call_thunk(const ir::Value &pair)
    {
        ir::Value code = fn_->load("ptr", pair);
        ir::Value env  = fn_->load("ptr", fn_->gep("ptr", pair, ir::i64(1)));
        return fn_->call("ptr", code.ref, { env });
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
                } else if constexpr (std::is_same_v<T, expr::Element>) {
                    return load(e.type, element(n.array, n.subscripts));
                } else if constexpr (std::is_same_v<T, expr::Call>) {
                    return from_result(call(n.proc, n.args), e.type);
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
        auto libm = [&](const char *name) {
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
                if constexpr (std::is_same_v<T, stmt::Block>) {
                    block(n);
                } else if constexpr (std::is_same_v<T, stmt::Compound>) {
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
                } else if constexpr (std::is_same_v<T, stmt::Call>) {
                    call(n.proc, n.args);
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

    // A block: its arrays allocated on entry, freed on exit.
    void block(const stmt::Block &b)
    {
        active_.push_back({ &b, {} });
        for (const auto &d : b.decls) {
            auto a = std::get_if<decl::Arrays>(&d->node);
            if (!a)
                continue;
            std::vector<ir::Value> bounds;
            for (const auto &bp : a->bounds) {
                bounds.push_back(subscript(*bp.lower));
                bounds.push_back(subscript(*bp.upper));
            }
            for (const auto &name : a->arrays) {
                ir::Value desc = descriptor(name.id);
                for (size_t k = 0; k < bounds.size(); k++)
                    fn_->store(bounds[k], fn_->gep("i64", desc, ir::i64(int64_t(3 + k))));
                runtime("void", "algol_array_alloc", { desc, ir::i64(int64_t(a->bounds.size())) });
                active_.back().arrays.push_back(name.id);
            }
        }
        for (const auto &s : b.body)
            statement(*s);
        if (fn_->open())
            free_arrays(active_.back());
        active_.pop_back();
    }

    void free_arrays(const Active &a)
    {
        for (auto it = a.arrays.rbegin(); it != a.arrays.rend(); ++it)
            runtime("void", "algol_array_free", { descriptor(*it) });
    }

    // An integer subscript or bound; a real one is rounded.
    ir::Value subscript(const Expr &e)
    {
        ir::Value v = expr(e);
        return e.type == Type::REAL ? round(v) : v;
    }

    // The address of an array's descriptor.
    ir::Value descriptor(int id)
    {
        const Symbol &s = symbol(id);
        if (s.kind == Symbol::FORMAL && s.spec.kind == Spec::ARRAY)
            return fn_->load("ptr", slot(s.owner, s.slot));
        if (s.kind != Symbol::ARRAY)
            unsupported("This array");
        if (s.slot < 0)
            return { "ptr", s.global };
        return slot(s.owner, s.slot);
    }

    // The address of an element: row-major, 8-byte cells.
    ir::Value element(int id, const ExprList &subscripts)
    {
        std::vector<ir::Value> subs(subscripts.size());
        std::transform(subscripts.begin(), subscripts.end(), subs.begin(),
                       [this](const ExprPtr &e) { return subscript(*e); });
        ir::Value desc  = descriptor(id);
        ir::Value index = ir::i64(0);
        for (size_t k = 0; k < subs.size(); k++) {
            if (opt_.check)
                runtime("void", "algol_check_index", { desc, ir::i64(int64_t(k)), subs[k] });
            ir::Value lo = fn_->load("i64", fn_->gep("i64", desc, ir::i64(int64_t(3 + 2 * k))));
            ir::Value hi = fn_->load("i64", fn_->gep("i64", desc, ir::i64(int64_t(4 + 2 * k))));
            ir::Value n  = fn_->binop("add", fn_->binop("sub", hi, lo), ir::i64(1));
            index = fn_->binop("add", fn_->binop("mul", index, n), fn_->binop("sub", subs[k], lo));
        }
        ir::Value data = fn_->load("ptr", desc);
        return fn_->gep("i64", data, index);
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
        const Symbol &label = symbol(ref->label);
        for (auto it = active_.rbegin(); it != active_.rend(); ++it)
            if (std::find(label.path.begin(), label.path.end(), it->block) == label.path.end())
                free_arrays(*it);
        fn_->br(label.block);
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
        return symbol(std::get<left::Element>(control).array).type;
    }

    //
    // Procedures.
    //

    // The frame: its slots zeroed on entry.
    void finish(Func &f)
    {
        f.fn->stack_alloc("i64", f.slots, 16, "frame");
        module_.declare("void", "llvm.memset.p0.i64", "ptr, i8, i64, i1");
        f.fn->prologue("call void @llvm.memset.p0.i64(ptr %frame, i8 0, i64 " +
                       std::to_string(8 * f.slots) + ", i1 false)");
    }

    void procedure(Func &f)
    {
        const decl::Procedure &p = *f.proc;
        fn_                      = f.fn;
        scope_                   = &f;
        frame_                   = { "ptr", "%frame" };
        fn_->store(fn_->param(0), slot(&f, SLOT_LINK));
        ir::Value args = fn_->param(1);
        for (size_t k = 0; k < p.formals.size(); k++) {
            const Formal &formal = p.formals[k];
            const Symbol &s      = symbol(formal.id);
            ir::Value pair       = fn_->gep("ptr", args, ir::i64(int64_t(2 * k)));
            ir::Value cell       = slot(&f, s.slot);
            switch (formal.spec.kind) {
            case Spec::SIMPLE:
                if (formal.by_value) {
                    ir::Value v = fn_->load(mem_type(formal.spec.type), call_thunk(pair));
                    fn_->store(v, cell);
                } else {
                    copy_pair(pair, cell);
                }
                break;
            case Spec::ARRAY: {
                ir::Value desc = call_thunk(pair);
                if (formal.by_value)
                    desc = runtime("ptr", "algol_array_copy",
                                   { desc, fn_->load("i64", fn_->gep("i64", desc, ir::i64(2))) });
                fn_->store(desc, cell);
                break;
            }
            case Spec::PROCEDURE:
            case Spec::SWITCH:
                copy_pair(call_thunk(pair), cell);
                break;
            case Spec::LABEL:
                copy_pair(pair, cell);
                break;
            }
        }
        statement(*p.body);
        if (fn_->open()) {
            for (const auto &formal : p.formals)
                if (formal.spec.kind == Spec::ARRAY && formal.by_value)
                    runtime("void", "algol_array_release",
                            { fn_->load("ptr", slot(&f, symbol(formal.id).slot)) });
            ir::Value result = ir::i64(0);
            if (p.type != Type::NONE)
                result = to_result(load(p.type, slot(&f, SLOT_RESULT)), p.type);
            fn_->ret(result);
        }
        finish(f);
    }

    void copy_pair(const ir::Value &from, const ir::Value &to)
    {
        fn_->store(fn_->load("ptr", from), to);
        fn_->store(fn_->load("ptr", fn_->gep("ptr", from, ir::i64(1))),
                   fn_->gep("ptr", to, ir::i64(1)));
    }

    // A value as the i64 result of a procedure, and back.
    ir::Value to_result(const ir::Value &v, Type t)
    {
        switch (t) {
        case Type::REAL:
            return fn_->cast("bitcast", v, "i64");
        case Type::BOOLEAN:
            return fn_->cast("zext", v, "i64");
        case Type::STRING:
            return fn_->cast("ptrtoint", v, "i64");
        default:
            return v;
        }
    }

    ir::Value from_result(const ir::Value &v, Type t)
    {
        switch (t) {
        case Type::REAL:
            return fn_->cast("bitcast", v, "double");
        case Type::BOOLEAN:
            return fn_->cast("trunc", v, "i1");
        case Type::STRING:
            return fn_->cast("inttoptr", v, "ptr");
        default:
            return v;
        }
    }

    // A call: the actual parameters as thunks; the i64 result.
    ir::Value call(int id, const ExprList &args)
    {
        const Symbol &s                    = symbol(id);
        const std::vector<Formal> *formals = nullptr;
        if (s.kind == Symbol::PROC)
            formals = &s.func->proc->formals;
        ir::Value list = ir::null();
        if (!args.empty()) {
            list = fn_->stack_alloc("ptr", int64_t(2 * args.size()));
            for (size_t k = 0; k < args.size(); k++) {
                const Formal *formal = formals && k < formals->size() ? &(*formals)[k] : nullptr;
                auto [code, env]     = thunk(*args[k], formal);
                ir::Value pair       = fn_->gep("ptr", list, ir::i64(int64_t(2 * k)));
                fn_->store(code, pair);
                fn_->store(env, fn_->gep("ptr", pair, ir::i64(1)));
            }
        }
        if (s.kind == Symbol::PROC)
            return fn_->call("i64", ir::global_name(s.func->fn->name()),
                             { frame_of(s.func->parent), list });
        if (s.kind == Symbol::FORMAL && s.spec.kind == Spec::PROCEDURE) {
            ir::Value pair = slot(s.owner, s.slot);
            ir::Value code = fn_->load("ptr", pair);
            ir::Value sl   = fn_->load("ptr", fn_->gep("ptr", pair, ir::i64(1)));
            return fn_->call("i64", code.ref, { sl, list });
        }
        unsupported("This call");
    }

    //
    // Thunks: an actual parameter as {code, env}, the code giving the
    // address of its cell (doc/codegen.md).
    //
    std::pair<ir::Value, ir::Value> thunk(const Expr &e, const Formal *formal)
    {
        // A name parameter passed on: its own pair.
        if (auto v = std::get_if<expr::Var>(&e.node)) {
            const Symbol &s = symbol(v->decl);
            if (s.kind == Symbol::FORMAL && !s.by_value &&
                (s.spec.kind == Spec::SIMPLE || s.spec.kind == Spec::LABEL)) {
                ir::Value pair = slot(s.owner, s.slot);
                return { fn_->load("ptr", pair),
                         fn_->load("ptr", fn_->gep("ptr", pair, ir::i64(1))) };
            }
        }
        std::string name = "T." + std::to_string(thunks_++);
        auto &tf         = module_.function(name, "ptr", { "ptr" }, "", "internal");

        // Generate it in the scope of the call, with env as the frame.
        auto saved_fn     = fn_;
        auto saved_frame  = frame_;
        auto saved_active = std::move(active_);
        active_.clear();
        fn_    = &tf;
        frame_ = tf.param(0);
        fn_->ret(thunk_cell(e, formal));
        fn_     = saved_fn;
        frame_  = saved_frame;
        active_ = std::move(saved_active);
        return { { "ptr", ir::global_name(name) }, frame_ };
    }

    ir::Value thunk_cell(const Expr &e, const Formal *formal)
    {
        auto var = std::get_if<expr::Var>(&e.node);
        if (var) {
            const Symbol &s = symbol(var->decl);
            if (s.kind == Symbol::ARRAY || (s.kind == Symbol::FORMAL && s.spec.kind == Spec::ARRAY))
                return descriptor(var->decl);
            if (s.kind == Symbol::PROC) {
                ir::Value cell = temp(2);
                fn_->store({ "ptr", ir::global_name(s.func->fn->name()) }, cell);
                fn_->store(frame_of(s.func->parent), fn_->gep("ptr", cell, ir::i64(1)));
                return cell;
            }
            if (s.kind == Symbol::FORMAL && s.spec.kind == Spec::PROCEDURE)
                return slot(s.owner, s.slot);
            if (s.kind == Symbol::VAR || s.kind == Symbol::FORMAL)
                return address(var->decl);
        }
        if (auto el = std::get_if<expr::Element>(&e.node))
            return element(el->array, el->subscripts);
        if (e.type == Type::NONE)
            unsupported("This actual parameter");
        // An expression: its value in a temporary, in the formal's type.
        Type t      = formal && formal->spec.kind == Spec::SIMPLE && formal->spec.type != Type::NONE
                          ? formal->spec.type
                          : e.type;
        ir::Value v = expr(e);
        if (t != e.type && (t == Type::REAL || t == Type::INTEGER))
            v = convert(v, e.type, t);
        else
            t = e.type;
        ir::Value cell = temp(1);
        store(t, v, cell);
        return cell;
    }

    // n temporary slots in the frame of the current scope.
    ir::Value temp(int n)
    {
        int k = scope_->slots;
        scope_->slots += n;
        return fn_->gep("i64", frame_, ir::i64(k));
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
        const auto &el = std::get<left::Element>(lp);
        return { element(el.array, el.subscripts), symbol(el.array).type };
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
        auto var = std::get_if<expr::Var>(&e.node);
        if (var && symbol(var->decl).kind == Symbol::ARRAY) {
            runtime("void", "algol_print_array",
                    { descriptor(var->decl), ir::i64(int64_t(e.type)) });
            return;
        }
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
