//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PASS3_H
#define ALGOL_PASS3_H

#include "passes.h"

//
// Pass 3 (G22162): the optimizer; reads the stream of pass 2 forward and
// writes its own forward.
//
Stream pass3(Context &ctx, const Stream &input);

#endif
