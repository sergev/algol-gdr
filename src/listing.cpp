//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "listing.h"

#include <ostream>

namespace {

// clang-format off
const char *const koi7_listing[128] = {
    "", "", "", "", "!", "Ъ", "×", "×",
    "!", "!", "", "!", "!", "", "⩽", "⩾",
    "‘", "", "", "", "", "―", "↑", "⏨",
    "≠", "°", "÷", "’", "⊃", "≡", "∨", "¬",
    " ", "!", "\"", "≠", "◇", "%", "∧", "′",
    "(", ")", "*", "+", ",", "-", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", ":", ";", "<", "=", ">", "↑",
    "°", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "[", "‾", "]", "|", "_",
    "Ю", "A", "Б", "Ц", "Д", "E", "Ф", "Г",
    "X", "И", "Й", "K", "Л", "M", "H", "O",
    "П", "Я", "P", "C", "T", "Y", "Ж", "B",
    "Ь", "Ы", "З", "Ш", "Э", "Щ", "Ч", "",
};
// clang-format on

} // namespace

const char *listing_char(unsigned c)
{
    return koi7_listing[c & 0177];
}

std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(' ') + 1);
    return s;
}

void Listing::print(const std::string &text)
{
    out_ << rtrim(text) << '\n';
}

void Listing::blank()
{
    out_ << '\n';
}

void Listing::finish()
{
    out_ << '\n';
}
