//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_OPTIONS_H
#define ALGOL_OPTIONS_H

#include <string>

//
// Compiler settings, from the command line.
//
struct Options {
    std::string source_file; // input .alg file
    std::string object_file; // output standard array (default: NAME.o)
    bool full_listing{};     // like *full card
    bool list{};             // listing; off: like *no list card
    bool check_mode{};       // like *check card: CHECKFL*
    int stop_after{};        // print stream of this pass and stop, 1-5
};

#endif
