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
    bool setftn_one{};                         // *call setftn:one: empty lines printed
    bool setftn_long{};                        // *call setftn:long
    bool check_mode{};                         // like *check card
    int stop_after{};                          // print stream of this pass and stop, 1-5

    // CHECKFL*, set by *check and by *call setftn: run-time checks, no pass 3.
    bool check() const { return check_mode || setftn_one || setftn_long; }
};

#endif
