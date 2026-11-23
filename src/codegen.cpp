//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "codegen.h"

std::string codegen(const ast::Program &, const CodegenOptions &opt)
{
    return "; ModuleID = '" + opt.module_name +
           "'\n\n"
           "define void @algol_main() {\n"
           "  ret void\n"
           "}\n";
}
