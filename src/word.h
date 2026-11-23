//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_WORD_H
#define ALGOL_WORD_H

#include <cstdint>

// BESM-6 word: 48 bits.
using Word = uint64_t;

constexpr Word WORD_MASK = 0xffffffffffffull;

#endif
