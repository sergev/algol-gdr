#ifndef ALGOL_OPTIONS_H
#define ALGOL_OPTIONS_H

#include <string>

//
// Compiler settings, from the command line.
//
struct Options {
    std::string source_file;                   // input .alg file
    std::string object_file{ "stdarray.out" }; // output standard array
    bool full_listing{};                       // like *full card
    bool no_listing{};                         // like *no list card
};

#endif
