#ifndef ALGOL_CARD_H
#define ALGOL_CARD_H

#include <array>
#include <string>
#include <vector>

#include "stdarray.h"

//
// Source cards as the original compiler reads them (READ* into 02010…):
// 80 characters in KOI-7, padded with blanks, six per word, so 13 words and
// 2 characters. Only columns 1-72 (words 0-11) are program text.
//
constexpr unsigned CARD_COLUMNS = 80;
constexpr unsigned CARD_WORDS   = 14;

using Card = std::array<Word, CARD_WORDS>;

// KOI-7 code of a Unicode character, as dubna converts job files, except the
// Algol symbols ∨ ∧ ⊃ ¬ ≡ ≠ ≤ ≥ ⏨ get the codes the lexer expects, × is *,
// and @ is ⏨.
// 0 when the character is dropped.
unsigned char unicode_to_koi7(unsigned u);

// UTF-8 line to KOI-7: lower case to upper, unknown characters dropped,
// ↑ to **, at most 80 characters.
std::string line_to_koi7(const std::string &line);

// Card from a source line.
Card make_card(const std::string &line);

// Cards of a whole source text, one per line.
std::vector<Card> read_cards(const std::string &text);

#endif
