#ifndef ALGOL_TRANSLATED_H
#define ALGOL_TRANSLATED_H

#include "machine.h"
#include "passes.h"

//
// A pass run on a Machine set up from the Context and written back to it at
// the end; the shared routines of the overlay (stream module, 01644, 27733,
// 30001) are natives.
//
class TranslatedPass {
public:
    static constexpr unsigned CHECKFL = 00521; // check mode: no pass 3

    explicit TranslatedPass(Context &ctx);

    Machine m;

    // Write a stream as the passes do, through the stream helper entry
    // (27126 forward, 27114 backward halves), from the cursor in 02001.
    // With keep_cursors, 02000 and 02001 stay as the previous pass left them.
    void write_stream(const Stream &s, unsigned entry, Word cursor, bool keep_cursors = false);

    // Run the pass from its entry, called by the driver with the return
    // address ret; returns the stream it wrote through the helpers, in
    // memory order.
    Stream run(unsigned entry, unsigned ret);

private:
    Context &ctx_;
};

//
// The stream a pass left for the next one, read as pass 4 reads it: from a
// zone before the write cursor 02001, after a priming read, up to EOP (075).
//
Stream stream_left(Context &ctx);

#endif
