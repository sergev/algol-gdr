#ifndef ALGOL_PASS2_H
#define ALGOL_PASS2_H

#include "passes.h"

//
// Pass 2 (G17223): reads the stream of pass 1 backward, binds identifiers
// and checks types; writes its stream backward. For now the code of the
// original, translated by scripts/gen-pass.py (pass2_gen.cpp).
//
Stream pass2(Context &ctx, const Stream &input);

#endif
