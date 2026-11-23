#ifndef ALGOL_TEST_GOLDEN_H
#define ALGOL_TEST_GOLDEN_H

#include <string>
#include <vector>

#include "passes.h"

// Names of the test programs tests/algol/NAME.alg, sorted.
std::vector<std::string> program_names();

// Path of an expected output: tests/algol/NAME.EXT.
std::string path(const std::string &name, const std::string &ext);

// Options from NAME.pre (*check, *no list); the listing is on by default.
void load_options(Options &opt, const std::string &name);

// Set the workspace, drums and registers as at the start of `pass`, from NAME.mem.
void load_mem(Context &ctx, const std::string &filename, int pass);

// The word of ctx for a drum word of NAME.mem (created zero), or none: the
// stream zones and the page pass 2 swaps out are not kept.
Word *drum_word(Context &ctx, unsigned unit, unsigned zone, unsigned offset);

// A workspace address of NAME.mem that ctx does not keep: the stream pages.
inline bool stream_page(unsigned addr)
{
    return addr >= 06000 && addr < 012000;
}

#endif
