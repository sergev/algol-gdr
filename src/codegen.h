//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_CODEGEN_H
#define ALGOL_CODEGEN_H

#include <string>

#include "ast.h"

struct CodegenOptions {
    std::string module_name; // source file
    std::string target;      // triple, or empty for the host
    bool check{};            // run-time checks (--check)
};

//
// LLVM IR (text) of a checked program; see doc/codegen.md.
//
std::string codegen(const ast::Program &program, const CodegenOptions &opt);

#endif
