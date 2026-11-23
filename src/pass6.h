#ifndef ALGOL_PASS6_H
#define ALGOL_PASS6_H

#include <string>

#include "passes.h"
#include "stdarray.h"

//
// Pass 6 (G26666): assemble the object from the stream of pass 5, the symbol
// rows (03000…), the long names (04000…), the label table (04400…) and the
// numbers and strings of ctx.constants and ctx.strings.
// The original writes the object backward to drum 026; here it is built in memory.
//
StdArray pass6(Context &ctx, const Stream &input);

// Summary line of G30257: PROGRAM ДЛИHA: … (time always 0.00).
std::string summary_line(const Context &ctx);

#endif
