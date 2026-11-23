//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_CARD_H
#define ALGOL_CARD_H

#include <string>


//
// Source cards as the original compiler reads them: a line of 80
// characters in KOI-7. Only columns 1-72 are program text.
//
constexpr unsigned CARD_COLUMNS = 80;

// KOI-7 code of a Unicode character, as dubna converts job files (the lexer
// takes those of ∨ ∧ ⊃ ¬ ≡ ≤ ≥ × ↑ as operators); 0 when it is dropped.
unsigned char unicode_to_koi7(unsigned u);

// UTF-8 line to KOI-7: lower case to upper, unknown characters dropped, at
// most 80 characters.
std::string line_to_koi7(const std::string &line);

#endif
