//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The code generator: the AST to LLVM IR. So far: PRINT of strings.
//

#include "codegen.h"

#include <stdexcept>

#include "ir.h"

using namespace ast;

namespace {

class Codegen {
public:
    explicit Codegen(const CodegenOptions &opt) : module_(opt.module_name)
    {
        module_.set_target(opt.target);
    }

    std::string run(const Program &program)
    {
        fn_ = &module_.function("algol_main", "void", {});
        statement(*program.block);
        fn_->ret_void();
        return module_.text();
    }

private:
    ir::Module module_;
    ir::Function *fn_{};

    [[noreturn]] static void unsupported(const std::string &what)
    {
        throw std::runtime_error(what + " is not supported yet");
    }

    void statement(const Stmt &s)
    {
        std::visit(
            [&](const auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, stmt::Block> || std::is_same_v<T, stmt::Compound>) {
                    if constexpr (std::is_same_v<T, stmt::Block>)
                        if (!n.decls.empty())
                            unsupported("A declaration");
                    for (const auto &b : n.body)
                        statement(*b);
                } else if constexpr (std::is_same_v<T, stmt::Dummy>) {
                } else if constexpr (std::is_same_v<T, stmt::Io>) {
                    io(n);
                } else {
                    unsupported("This statement");
                }
            },
            s.node);
    }

    void io(const stmt::Io &s)
    {
        if (s.op != IoOp::PRINT || !s.params.empty())
            unsupported("This input or output");
        for (const auto &item : s.items) {
            auto it = std::get_if<io::Item>(&item.node);
            auto sc = it ? std::get_if<expr::StringConst>(&it->expr->node) : nullptr;
            if (!sc)
                unsupported("This item of PRINT");
            module_.declare("void", "algol_print_string", "ptr");
            fn_->call("void", "@algol_print_string", { module_.string(sc->value) });
        }
    }
};

} // namespace

std::string codegen(const Program &program, const CodegenOptions &opt)
{
    return Codegen(opt).run(program);
}
