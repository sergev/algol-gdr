#ifndef ALGOL_PASSES_H
#define ALGOL_PASSES_H

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
};

struct NotImplemented : std::runtime_error {
    using std::runtime_error::runtime_error;
};

//
// Run pass n (1-5) on the stream of the previous pass (ignored by pass 1)
// and return its own stream.
//
Stream run_pass(int n, Context &ctx, const Stream &input);

#endif
