#ifndef ALGOL_PASSES_H
#define ALGOL_PASSES_H

#include <array>
#include <iosfwd>
#include <stdexcept>
#include <string>

#include "item.h"
#include "options.h"
#include "workspace.h"

//
// State shared by the passes.
//
struct Context {
    Options options;
    std::string source;               // program text, UTF-8 (pass 1)
    Workspace ws;                     // tables at their original addresses
    std::ostream *listing{ nullptr }; // listing output, or none

    // Index registers as the previous pass left them; some passes take data
    // in them (pass 6: M5 = number of symbol rows).
    std::array<unsigned, 16> regs{};

    unsigned flgex{}; // FLGEX*: errors found
    bool finished{};  // the compilation ended early (AG01716)
};

struct NotImplemented : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// AG01713: an empty line in the listing; the printer shows it only with setftn:one.
void blank_line(const Context &ctx);

//
// Run pass n (1-5) on the stream of the previous pass (ignored by pass 1)
// and return its own stream.
//
Stream run_pass(int n, Context &ctx, const Stream &input);

#endif
