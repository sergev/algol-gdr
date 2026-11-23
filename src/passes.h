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
#include "stdarray.h"
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
    std::vector<Word> constants;   // drum 27 zone 33 from its end: the numbers (pass 5)
    std::vector<Word> strings;     // drum 27 zones 34..: the string pool (pass 1)

    // Index registers a pass leaves for the next: M3-M6 after pass 1 (pass 2
    // starts with them, M3 the outer block), M5 after pass 5 (the number of
    // symbol rows, for pass 6).
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
// Run pass n (1-5) on the stream of the previous pass (ignored by pass 1)
// and return its own stream.
//
Stream run_pass(int n, Context &ctx, const Stream &input);

//
// The driver (01614): the passes in turn; pass 3 not in check mode, none
// after pass 2 when there are errors. The listing ends with the summary line.
// Returns false when no object is made.
//
bool compile(Context &ctx, StdArray &object);

#endif
