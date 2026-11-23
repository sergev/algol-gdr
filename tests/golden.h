//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_TEST_GOLDEN_H
#define ALGOL_TEST_GOLDEN_H

#include <string>
#include <vector>

#include "options.h"

// Names of the test programs tests/algol/NAME.alg, sorted.
std::vector<std::string> program_names();

// Path of an expected output: tests/algol/NAME.EXT.
std::string path(const std::string &name, const std::string &ext);

// Options from NAME.pre (*check, *no list); the listing is on by default.
void load_options(Options &opt, const std::string &name);

#endif
