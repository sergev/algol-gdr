//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PASS4_H
#define ALGOL_PASS4_H

#include "passes.h"

//
// Pass 4 (G23523): code generation; reads the stream of pass 3 (of pass 2 in
// check mode) and writes instruction halves and pseudo-items.
//
Stream pass4(Context &ctx, const Stream &input);

#endif
