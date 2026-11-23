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
constexpr int SLOT_JMPBUF = 1; // the jmp_buf of a function with non-local labels
constexpr int SLOT_LABEL  = 2; // where a non-local goto goes
constexpr int SLOT_RESULT = 3; // the value of a function
constexpr int SLOT_HEADER = 4; // first free slot

// An Algol function: the program body or a procedure.
struct Func {
    ir::Function *fn{};
    Func *parent{};
    int slots{ SLOT_HEADER };
    const decl::Procedure *proc{}; // null for the program
    int labels{};                  // numbered from 1
    std::vector<int> far_labels;   // reached by longjmp
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
    int index{};                           // LABEL: its number in the owner
    bool far{};                            // LABEL: reached by longjmp
    std::string common;                    // VAR, ARRAY in a _common block: its global
    int offset{};                          // and the place there, in words
    const decl::Switch *sw{};              // SWITCH
    ir::Function *sfn{};                   // SWITCH: its function
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
        for (const auto &e : program.externals) {
            Symbol s;
            s.kind         = Symbol::EXTERNAL;
            s.type         = e.type;
            s.name         = e.name;
            s.by_value     = e.kind == External::CODE; // C convention
            symbols_[e.id] = s;
        }
        declare(main_, *program.block);
        scan(&main_, *program.block);
        far_entry(main_);
        statement(*program.block);
        fn_->ret_void();
        finish(main_);
        for (auto &f : funcs_)
            procedure(*f);
        for (int id : switches_)
            switch_function(symbol(id));
        for (const auto &[global, words] : commons_)
            module_.global(global + " = common global [" + std::to_string(words) +
                           " x i64] zeroinitializer, align 8");
        return module_.text();
    }

private:
    const CodegenOptions &opt_;
    ir::Module module_;
    Func main_;
    std::map<int, Symbol> symbols_;
    std::vector<std::unique_ptr<Func>> funcs_; // procedures
    std::vector<int> switches_;
    std::map<const decl::Procedure *, Func *> proc_funcs_;
    std::map<std::string, int> commons_; // global → words
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
                    for (const auto &d : n.decls)
                        if (auto c = std::get_if<decl::Common>(&d->node))
                            common(*c);
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
                    s.index        = ++f.labels;
                    symbols_[n.id] = s;
                } else if constexpr (std::is_same_v<T, decl::Arrays>) {
                    for (const auto &a : n.arrays)
                        array(f, a.id, a.name, n.type, int(n.bounds.size()));
                } else if constexpr (std::is_same_v<T, decl::Procedure>) {
                    declare_procedure(f, n);
                } else if constexpr (std::is_same_v<T, decl::Common>) {
                    // After the other declarations of the block.
                } else if constexpr (std::is_same_v<T, decl::Switch>) {
                    Symbol s;
                    s.kind  = Symbol::SWITCH;
                    s.name  = n.name;
                    s.owner = &f;
                    s.sw    = &n;
                    s.sfn   = &module_.function("S." + std::to_string(n.id) + "." + n.name, "void",
                                                { "ptr", "i64", "ptr" }, "", "internal");
                    symbols_[n.id] = s;
                    switches_.push_back(n.id);
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
        Func &pf        = *funcs_.back();
        pf.parent       = &f;
        pf.proc         = &p;
        proc_funcs_[&p] = &pf;
        pf.fn           = &module_.function("P." + std::to_string(p.id) + "." + p.name, "i64",
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

    // A _common block: its elements at their places in one global.
    void common(const decl::Common &c)
    {
        std::string global = ir::global_name(c.name);
        int offset         = 0;
        for (const auto &e : c.elements) {
            Symbol &s = symbols_.at(e.decl);
            s.common  = global;
            s.offset  = offset;
            offset += e.size;
        }
        commons_[global] = std::max(commons_[global], offset);
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
            if (!s.common.empty())
                return fn_->gep("i64", { "ptr", s.common }, ir::i64(s.offset));
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
        // A Boolean goes to C as an i8 0 or 1, as _Bool.
        std::vector<ir::Value> list;
        std::string params;
        for (const auto &a : args) {
            list.push_back(a.type == "i1" ? fn_->cast("zext", a, "i8") : a);
            params += (params.empty() ? "" : ", ") + list.back().type;
        }
        module_.declare(ret, name, params, attrs);
        return fn_->call(ret, "@" + name, list);
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
                } else if constexpr (std::is_same_v<T, expr::StringElement>) {
                    ir::Value s = expr(*n.string);
                    return runtime("ptr", "algol_string_element", { s, subscript(*n.index) });
                } else if constexpr (std::is_same_v<T, expr::Slice>) {
                    ir::Value s = expr(*n.string);
                    ir::Value a = subscript(*n.first);
                    return runtime("ptr", "algol_string_slice", { s, a, subscript(*n.last) });
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
                return runtime("ptr", "algol_string_concat", { l, r });
            return fn_->binop(real ? "fadd" : "add", l, r);
        case BinaryOp::SUB:
            return fn_->binop(real ? "fsub" : "sub", l, r);
        case BinaryOp::MUL:
            return fn_->binop(real ? "fmul" : "mul", l, r);
        case BinaryOp::DIV:
            return fn_->binop("fdiv", l, r);
        case BinaryOp::INT_DIV:
            return runtime("i64", "algol_idiv", { l, r });
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
                ir::Value dims  = ir::i64(int64_t(a->bounds.size()));
                const Symbol &s = symbol(name.id);
                if (!s.common.empty()) {
                    runtime(
                        "void", "algol_array_bind",
                        { desc, dims, fn_->gep("i64", { "ptr", s.common }, ir::i64(s.offset)) });
                    continue;
                }
                runtime("void", "algol_array_alloc", { desc, dims });
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

    //
    // Labels reached from other functions: a label value {frame, index},
    // longjmp to the frame's jmp_buf, and a switch on the index there.
    //

    // Labels used other than as the target of a goto in their own function.
    void scan(Func *f, const Stmt &s)
    {
        std::visit(
            [&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, stmt::Block>) {
                    for (const auto &d : n.decls)
                        scan(f, *d);
                    for (const auto &b : n.body)
                        scan(f, *b);
                } else if constexpr (std::is_same_v<T, stmt::Compound>) {
                    for (const auto &b : n.body)
                        scan(f, *b);
                } else if constexpr (std::is_same_v<T, stmt::Labeled>) {
                    scan(f, *n.stmt);
                } else if constexpr (std::is_same_v<T, stmt::Goto>) {
                    auto ref = std::get_if<expr::LabelRef>(&n.target->node);
                    if (!ref || symbol(ref->label).owner != f)
                        scan(*n.target);
                } else if constexpr (std::is_same_v<T, stmt::If>) {
                    scan(*n.condition);
                    scan(f, *n.then_stmt);
                    if (n.else_stmt)
                        scan(f, *n.else_stmt);
                } else if constexpr (std::is_same_v<T, stmt::For>) {
                    scan(n.control);
                    scan(n.elements);
                    scan(f, *n.body);
                } else if constexpr (std::is_same_v<T, stmt::Assign>) {
                    for (const auto &t : n.targets)
                        scan(t);
                    scan(*n.value);
                } else if constexpr (std::is_same_v<T, stmt::Call> ||
                                     std::is_same_v<T, stmt::StdProc>) {
                    for (const auto &a : n.args)
                        scan(*a);
                } else if constexpr (std::is_same_v<T, stmt::Io>) {
                    for (const auto &a : n.params)
                        scan(*a);
                    scan(n.items);
                }
            },
            s.node);
    }

    void scan(Func *f, const Decl &d)
    {
        if (auto a = std::get_if<decl::Arrays>(&d.node)) {
            for (const auto &b : a->bounds) {
                scan(*b.lower);
                scan(*b.upper);
            }
        } else if (auto sw = std::get_if<decl::Switch>(&d.node)) {
            for (const auto &e : sw->elements)
                scan(*e);
        } else if (auto p = std::get_if<decl::Procedure>(&d.node)) {
            scan(proc_funcs_.at(p), *p->body);
        }
        (void)f;
    }

    void scan(const IoList &items)
    {
        for (const auto &item : items) {
            if (auto it = std::get_if<io::Item>(&item.node))
                scan(*it->expr);
            else if (auto fr = std::get_if<io::For>(&item.node)) {
                scan(fr->control);
                scan(fr->elements);
                scan(fr->body);
            }
        }
    }

    void scan(const LeftPart &lp)
    {
        if (auto el = std::get_if<left::Element>(&lp))
            for (const auto &e : el->subscripts)
                scan(*e);
    }

    void scan(const std::vector<ForElement> &elements)
    {
        for (const auto &el : elements)
            std::visit(
                [&](const auto &x) {
                    using T = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<T, for_elem::Value>) {
                        scan(*x.value);
                    } else if constexpr (std::is_same_v<T, for_elem::While>) {
                        scan(*x.value);
                        scan(*x.condition);
                    } else {
                        scan(*x.init);
                        scan(*x.step);
                        scan(*x.until);
                    }
                },
                el);
    }

    void scan(const Expr &e)
    {
        std::visit(
            [&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, expr::LabelRef>) {
                    Symbol &label = symbols_.at(n.label);
                    if (label.kind == Symbol::LABEL && !label.far) {
                        label.far = true;
                        label.owner->far_labels.push_back(n.label);
                    }
                } else if constexpr (std::is_same_v<T, expr::Element>) {
                    for (const auto &a : n.subscripts)
                        scan(*a);
                } else if constexpr (std::is_same_v<T, expr::Call> ||
                                     std::is_same_v<T, expr::StdFunc>) {
                    for (const auto &a : n.args)
                        scan(*a);
                } else if constexpr (std::is_same_v<T, expr::Unary>) {
                    scan(*n.operand);
                } else if constexpr (std::is_same_v<T, expr::Binary>) {
                    scan(*n.left);
                    scan(*n.right);
                } else if constexpr (std::is_same_v<T, expr::Cond>) {
                    scan(*n.condition);
                    scan(*n.then_expr);
                    scan(*n.else_expr);
                } else if constexpr (std::is_same_v<T, expr::Convert>) {
                    scan(*n.expr);
                } else if constexpr (std::is_same_v<T, expr::StringElement>) {
                    scan(*n.string);
                    scan(*n.index);
                } else if constexpr (std::is_same_v<T, expr::Slice>) {
                    scan(*n.string);
                    scan(*n.first);
                    scan(*n.last);
                } else if constexpr (std::is_same_v<T, expr::SwitchElement>) {
                    scan(*n.index);
                }
            },
            e.node);
    }

    // On entry to a function with far labels: setjmp, and after a longjmp
    // a switch to the label.
    void far_entry(const Func &f)
    {
        if (f.far_labels.empty())
            return;
        ir::Value jb = fn_->stack_alloc("i64", 64, 16);
        fn_->store(jb, slot(&f, SLOT_JMPBUF));
        module_.declare("i32", "_setjmp", "ptr", "returns_twice");
        ir::Value r = fn_->call("i32", "@_setjmp", { jb }, " returns_twice");
        auto *go    = fn_->new_block("far");
        auto *body  = fn_->new_block("body");
        fn_->cond_br(fn_->icmp("ne", r, { "i32", "0" }), go, body);
        fn_->set_block(go);
        std::vector<std::pair<int64_t, const ir::Block *>> cases(f.far_labels.size());
        std::transform(f.far_labels.begin(), f.far_labels.end(), cases.begin(), [this](int id) {
            return std::pair<int64_t, const ir::Block *>(symbol(id).index, symbol(id).block);
        });
        auto *bad = fn_->new_block("far.bad");
        fn_->switch_(fn_->load("i64", slot(&f, SLOT_LABEL)), bad, cases);
        fn_->set_block(bad);
        fn_->unreachable();
        fn_->set_block(body);
    }

    // The label value of a designational expression: {frame, index}.
    std::pair<ir::Value, ir::Value> label_value(const Expr &e)
    {
        auto ref = std::get_if<expr::LabelRef>(&e.node);
        auto var = std::get_if<expr::Var>(&e.node);
        if (ref && symbol(ref->label).kind == Symbol::LABEL) {
            const Symbol &label = symbol(ref->label);
            return { frame_of(label.owner), ir::i64(label.index) };
        }
        if (ref || var) {
            // A formal label.
            const Symbol &s = symbol(ref ? ref->label : var->decl);
            ir::Value cell  = call_thunk(slot(s.owner, s.slot));
            return { fn_->load("ptr", cell), fn_->load("i64", fn_->gep("ptr", cell, ir::i64(1))) };
        }
        if (auto se = std::get_if<expr::SwitchElement>(&e.node)) {
            ir::Value index = subscript(*se->index);
            const Symbol &s = symbol(se->switch_);
            ir::Value code, frame;
            if (s.kind == Symbol::SWITCH) {
                code  = { "ptr", ir::global_name(s.sfn->name()) };
                frame = frame_of(s.owner);
            } else {
                ir::Value pair = slot(s.owner, s.slot);
                code           = fn_->load("ptr", pair);
                frame          = fn_->load("ptr", fn_->gep("ptr", pair, ir::i64(1)));
            }
            ir::Value out = fn_->stack_alloc("i64", 2);
            fn_->call("void", code.ref, { frame, index, out });
            return { fn_->load("ptr", out), fn_->load("i64", fn_->gep("ptr", out, ir::i64(1))) };
        }
        if (auto c = std::get_if<expr::Cond>(&e.node)) {
            ir::Value cond = expr(*c->condition);
            auto *yes      = fn_->new_block("des.then");
            auto *no       = fn_->new_block("des.else");
            auto *done     = fn_->new_block("des.end");
            fn_->cond_br(cond, yes, no);
            fn_->set_block(yes);
            auto a      = label_value(*c->then_expr);
            auto *a_end = fn_->block();
            fn_->br(done);
            fn_->set_block(no);
            auto b      = label_value(*c->else_expr);
            auto *b_end = fn_->block();
            fn_->br(done);
            fn_->set_block(done);
            return { fn_->phi("ptr", { { a.first, a_end }, { b.first, b_end } }),
                     fn_->phi("i64", { { a.second, a_end }, { b.second, b_end } }) };
        }
        unsupported("This designational expression");
    }

    // A switch: element k's label value to out.
    void switch_function(const Symbol &s)
    {
        fn_            = s.sfn;
        scope_         = s.owner;
        frame_         = fn_->param(0);
        ir::Value out  = fn_->param(2);
        auto *bad      = fn_->new_block("bad");
        auto *dispatch = fn_->block();
        std::vector<std::pair<int64_t, const ir::Block *>> cases;
        for (size_t k = 0; k < s.sw->elements.size(); k++) {
            auto *b = fn_->new_block("elem");
            cases.push_back({ int64_t(k + 1), b });
            fn_->set_block(b);
            auto [frame, index] = label_value(*s.sw->elements[k]);
            fn_->store(frame, out);
            fn_->store(index, fn_->gep("ptr", out, ir::i64(1)));
            fn_->ret_void();
        }
        fn_->set_block(dispatch);
        fn_->switch_(fn_->param(1), bad, cases);
        fn_->set_block(bad);
        runtime("void", "algol_switch_error", { fn_->param(1) }, "noreturn");
        fn_->unreachable();
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
        if (!ref || symbol(ref->label).kind != Symbol::LABEL ||
            symbol(ref->label).owner != scope_ || fn_ != scope_->fn) {
            auto [frame, index] = label_value(target);
            runtime("void", "algol_goto", { frame, index }, "noreturn");
            fn_->unreachable();
            return;
        }
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
        far_entry(f);
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
        const Symbol &s = symbol(id);
        if (s.kind == Symbol::EXTERNAL && s.by_value)
            return c_call(s, args);
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
        if (s.kind == Symbol::EXTERNAL && !s.by_value) {
            module_.declare("i64", s.name, "ptr, ptr");
            return fn_->call("i64", ir::global_name(s.name), { ir::null(), list });
        }
        if (s.kind == Symbol::FORMAL && s.spec.kind == Spec::PROCEDURE) {
            ir::Value pair = slot(s.owner, s.slot);
            ir::Value code = fn_->load("ptr", pair);
            ir::Value sl   = fn_->load("ptr", fn_->gep("ptr", pair, ir::i64(1)));
            return fn_->call("i64", code.ref, { sl, list });
        }
        unsupported("This call");
    }

    // A _code procedure: a C function with the address of each argument
    // (the data of an array); the result in the i64 form.
    ir::Value c_call(const Symbol &s, const ExprList &args)
    {
        std::vector<ir::Value> list;
        std::string params;
        for (const auto &a : args) {
            ir::Value cell;
            auto var = std::get_if<expr::Var>(&a->node);
            if (var && (symbol(var->decl).kind == Symbol::ARRAY ||
                        (symbol(var->decl).kind == Symbol::FORMAL &&
                         symbol(var->decl).spec.kind == Spec::ARRAY))) {
                cell = fn_->load("ptr", descriptor(var->decl));
            } else if (var && (symbol(var->decl).kind == Symbol::VAR ||
                               symbol(var->decl).kind == Symbol::FORMAL)) {
                cell = address(var->decl);
            } else if (auto el = std::get_if<expr::Element>(&a->node)) {
                cell = element(el->array, el->subscripts);
            } else {
                cell = fn_->stack_alloc("i64");
                store(a->type, expr(*a), cell);
            }
            list.push_back(cell);
            params += (params.empty() ? "" : ", ") + std::string("ptr");
        }
        std::string ret = s.type == Type::NONE ? "void" : reg_type(s.type);
        module_.declare(ret, s.name, params);
        ir::Value r = fn_->call(ret, ir::global_name(s.name), list);
        return s.type == Type::NONE ? ir::i64(0) : to_result(r, s.type);
    }

    //
    // Thunks: an actual parameter as {code, env}, the code giving the
    // address of its cell (doc/codegen.md).
    //
    std::pair<ir::Value, ir::Value> thunk(const Expr &e, const Formal *formal)
    {
        // A name parameter passed on: its own pair.
        auto v   = std::get_if<expr::Var>(&e.node);
        auto ref = std::get_if<expr::LabelRef>(&e.node);
        if (v || ref) {
            const Symbol &s = symbol(v ? v->decl : ref->label);
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
        if (var && symbol(var->decl).kind == Symbol::SWITCH) {
            const Symbol &s = symbol(var->decl);
            ir::Value cell  = temp(2);
            fn_->store({ "ptr", ir::global_name(s.sfn->name()) }, cell);
            fn_->store(frame_of(s.owner), fn_->gep("ptr", cell, ir::i64(1)));
            return cell;
        }
        if (var && symbol(var->decl).kind == Symbol::FORMAL &&
            symbol(var->decl).spec.kind == Spec::SWITCH)
            return slot(symbol(var->decl).owner, symbol(var->decl).slot);
        if (auto el = std::get_if<expr::Element>(&e.node))
            return element(el->array, el->subscripts);
        if (e.type == Type::NONE) {
            // A designational expression: its label value.
            auto [frame, index] = label_value(e);
            ir::Value cell      = temp(2);
            fn_->store(frame, cell);
            fn_->store(index, fn_->gep("ptr", cell, ir::i64(1)));
            return cell;
        }
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

    // PRINT, _write with a format, READ.
    enum class IoMode { PRINT, FORMAT, READ } io_mode_{};

    void io(const stmt::Io &s)
    {
        switch (s.op) {
        case IoOp::PRINT:
            io_mode_ = IoMode::PRINT;
            io_items(s.items);
            return;
        case IoOp::WRITE:
        case IoOp::PUNCH: {
            ir::Value format = ir::null();
            if (!s.params.empty() && s.params.back()->type == Type::STRING)
                format = expr(*s.params.back());
            io_mode_ = IoMode::FORMAT;
            runtime("void", "algol_format_begin", { format });
            io_items(s.items);
            runtime("void", "algol_format_end", {});
            return;
        }
        case IoOp::READ:
            io_mode_ = IoMode::READ;
            io_items(s.items);
            return;
        default:
            unsupported(std::string("'_") + to_string(s.op) + "'");
        }
    }

    void io_items(const IoList &items)
    {
        for (const auto &item : items) {
            std::visit(
                [&](const auto &n) {
                    using T = std::decay_t<decltype(n)>;
                    if constexpr (std::is_same_v<T, io::Item>) {
                        io_item(*n.expr);
                    } else if constexpr (std::is_same_v<T, io::NewLine>) {
                        runtime("void", "algol_newline", {});
                    } else if constexpr (std::is_same_v<T, io::NewPage>) {
                        runtime("void", "algol_newpage", {});
                    } else {
                        IoMode mode = io_mode_;
                        for_loop(n.control, n.elements, [&] {
                            io_mode_ = mode;
                            io_items(n.body);
                        });
                    }
                },
                item.node);
        }
    }

    void io_item(const Expr &e)
    {
        static const char *const names[]  = { "integer", "real", "boolean", "string", "bits" };
        static const char *const prefix[] = { "algol_print_", "algol_format_", "algol_read_" };
        const char *pre                   = prefix[int(io_mode_)];
        auto var                          = std::get_if<expr::Var>(&e.node);
        if (var && (symbol(var->decl).kind == Symbol::ARRAY ||
                    (symbol(var->decl).kind == Symbol::FORMAL &&
                     symbol(var->decl).spec.kind == Spec::ARRAY))) {
            runtime("void", std::string(pre) + "array",
                    { descriptor(var->decl), ir::i64(int64_t(e.type)) });
            return;
        }
        if (e.type == Type::NONE || e.type == Type::BITS) {
            if (io_mode_ == IoMode::PRINT && e.type == Type::BITS) {
                runtime("void", "algol_print_bits", { expr(e) });
                return;
            }
            unsupported("This item of input or output");
        }
        std::string name = std::string(pre) + names[int(e.type)];
        if (io_mode_ != IoMode::READ) {
            runtime("void", name, { expr(e) });
            return;
        }
        ir::Value ptr;
        if (var)
            ptr = address(var->decl);
        else if (auto el = std::get_if<expr::Element>(&e.node))
            ptr = element(el->array, el->subscripts);
        else
            unsupported("READ of an expression");
        store(e.type, runtime(reg_type(e.type), name, {}), ptr);
    }
};

} // namespace

std::string codegen(const Program &program, const CodegenOptions &opt)
{
    return Codegen(opt).run(program);
}
