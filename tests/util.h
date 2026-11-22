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

#endif
