//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_DRIVER_H
#define ALGOL_DRIVER_H

#include <string>
#include <vector>

#include "options.h"

//
// Compile the inputs as the options say: .alg through the front end and
// the code generator to LLVM IR, the rest to clang. Returns the exit status.
//
int compile(const Options &opt);

// The clang to run: $ALGC_CLANG or clang from PATH.
std::string clang_path();

// Where libalgol.a is, for an algc in exe_dir; empty when not found.
std::string runtime_dir(const std::string &exe_dir, const std::string &target);

#endif
