//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_OPTIONS_H
#define ALGOL_OPTIONS_H

#include <string>
#include <vector>

enum class Output {
    EXECUTABLE, // default
    OBJECT,     // -c
    ASSEMBLY,   // -S
    IR,         // --emit-llvm
    NONE,       // --syntax-only, --dump-ast
};

//
// Compiler settings, from the command line.
//
struct Options {
    std::string arg0{ "algc" };
    std::vector<std::string> inputs; // .alg, .ll, .s, .o, .a
    std::string output_file;         // -o
    Output output{ Output::EXECUTABLE };
    int optimize{ 1 };                 // -O0..3
    bool debug_info{};                 // -g
    bool verbose{};                    // -v
    bool save_temps{};                 // --save-temps
    std::vector<std::string> lib_dirs; // -L
    std::vector<std::string> libs;     // -l
    std::string target;                // --target=TRIPLE
    bool list{};                       // listing
    bool check_mode{};                 // run-time checks, like *check
    bool dump_ast{};                   // print the AST (YAML)
};

#endif
