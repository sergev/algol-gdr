#ifndef ALGOL_TEST_UTIL_H
#define ALGOL_TEST_UTIL_H

#include <string>

//
// Result of running a shell command.
//
struct RunResult {
    int exit_code{ -1 };
    std::string output; // stdout and stderr
};

RunResult run_command(const std::string &cmd);
std::string file_contents(const std::string &filename);

// Compiler options for the control cards in a NAME.pre file: --check
std::string pre_options(const std::string &filename);

#endif
