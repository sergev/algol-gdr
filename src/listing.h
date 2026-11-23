#ifndef ALGOL_LISTING_H
#define ALGOL_LISTING_H

#include <string>

#include "stdarray.h"

//
// The listing as dubna prints it: the compiler's ISO (KOI-7) characters go to
// the printer in GOST, and dubna writes them in UTF-8. Measured with a test
// program; see doc/encoding.md.
//

// One character; some print as nothing.
const char *listing_char(unsigned c);

// Characters bytes[first…last) of ISO words, 6 per word; code 003 ends the line.
std::string listing_text(const Word *words, unsigned first, unsigned last);

// Without trailing blanks.
std::string rtrim(std::string s);

#endif
