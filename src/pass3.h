#ifndef ALGOL_PASS3_H
#define ALGOL_PASS3_H

#include "passes.h"

//
// Pass 3 (G22162): the optimizer; reads the stream of pass 2 forward and
// writes its own forward. For now the code of the original, translated by
// scripts/gen-pass.py (pass3_gen.cpp).
//
Stream pass3(Context &ctx, const Stream &input);

#endif
