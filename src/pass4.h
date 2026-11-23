#ifndef ALGOL_PASS4_H
#define ALGOL_PASS4_H

#include "passes.h"

//
// Pass 4 (G23523): code generation; reads the stream of pass 3 (of pass 2 in
// check mode) and writes instruction halves and pseudo-items. For now the
// code of the original, translated by scripts/gen-pass.py (pass4_gen.cpp).
//
Stream pass4(Context &ctx, const Stream &input);

#endif
