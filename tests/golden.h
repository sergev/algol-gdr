#ifndef ALGOL_TEST_GOLDEN_H
#define ALGOL_TEST_GOLDEN_H

#include <string>
#include <vector>

#include "passes.h"

// Names of the test programs tests/algol/NAME.alg, sorted.
std::vector<std::string> program_names();

// Path of an expected output: tests/algol/NAME.EXT.
std::string path(const std::string &name, const std::string &ext);

// Set the workspace, drums and registers as at the start of `pass`, from NAME.mem.
void load_mem(Context &ctx, const std::string &filename, int pass);

#endif
