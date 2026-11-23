//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PASSES_H
#define ALGOL_PASSES_H

#include <array>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

#include "item.h"
#include "listing.h"
#include "options.h"
#include "workspace.h"

//
// State shared by the passes.
//
struct Context {
    Options options;
    std::string source;          // program text, UTF-8 (pass 1)
    Workspace ws;                // tables at their original addresses
    Listing *listing{ nullptr }; // listing output, or none

    // What the original keeps on its scratch drums.
    std::vector<Word> saved_names; // drum 26 zone 0: page 02000-03777 as pass 2 starts
    std::vector<Word> strings;     // drum 27 zones 34..: the string pool (pass 1)

    // Index registers M3-M6 that pass 1 leaves for pass 2 (M3 the outer block).
    std::array<unsigned, 16> regs{};

    unsigned flgex{}; // FLGEX*: errors found
    bool finished{};  // the compilation ended early (AG01716)
};

struct NotImplemented : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// AG01713: an empty line in the listing.
void blank_line(const Context &ctx);

//
// Run pass n (1-2) on the stream of the previous pass (ignored by pass 1)
// and return its own stream.
//
Stream run_pass(int n, Context &ctx, const Stream &input);

//
// The driver (01614): passes 1 and 2. Returns the stream of pass 2, or
// false when there are errors.
//
bool compile(Context &ctx, Stream &stream);

#endif
