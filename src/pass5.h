#ifndef ALGOL_PASS5_H
#define ALGOL_PASS5_H

#include <stdexcept>

#include "passes.h"

//
// Pass 5 (G26260): assign addresses. Reads the stream of pass 4; resolves
// labels, makes the symbol rows (03000…) of run-time names and absolute
// values, queues long names (04000…); prints the line table. Leaves the
// number of rows in regs[5] and the constants on drum 027 zone 033.
//
Stream pass5(Context &ctx, const Stream &input);

// G30120: restore the run-time name table at 02400.
void restore_rtnames(Workspace &ws);

struct ProgramTooLong : std::runtime_error {
    ProgramTooLong() : std::runtime_error("PROGRAM TOO LONG") {}
};

#endif
