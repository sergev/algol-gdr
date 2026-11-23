#ifndef ALGOL_TRANSLATED_H
#define ALGOL_TRANSLATED_H

#include "machine.h"
#include "passes.h"

//
// A pass run as code translated from the overlay (scripts/gen-pass.py), on a
// Machine set up from the Context and written back to it at the end.
//
class TranslatedPass {
public:
    TranslatedPass(Context &ctx, Machine::Code code);

    Machine m;

    // Write a stream as the passes do, through the stream helper entry
    // (27126 forward, 27114 backward halves), from the cursor in 02001.
    void write_stream(const Stream &s, unsigned entry, Word cursor);

    // Run the pass from its entry, called by the driver with the return
    // address ret; returns the stream it wrote through the helpers, in
    // memory order.
    Stream run(unsigned entry, unsigned ret);

private:
    Context &ctx_;
    Machine::Code code_;
};

#endif
