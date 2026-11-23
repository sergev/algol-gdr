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
#include "tables.h"

//
// State shared by the passes.
//
struct Context {
    Options options;
    std::string source;          // program text, UTF-8 (pass 1)
    Tables tables;               // shared by passes 1 and 2
    Listing *listing{ nullptr }; // listing output, or none

    // What the original keeps on its scratch drums.
    std::vector<Word> saved_names; // the name table by slot as pass 2 starts (drum 26 zone 0)
    std::vector<Word> strings;     // drum 27 zones 34..: the string pool (pass 1)
    std::vector<Word> dlist;       // 04400-05777 as pass 1 leaves it

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
