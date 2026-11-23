#ifndef ALGOL_LISTING_H
#define ALGOL_LISTING_H

#include <iosfwd>
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

//
// The printout of the compiler (AG01644, AG01713 and ASMPRINT), as the
// original prints it with *call setftn:one,long: rows go out at once, in one
// column, long ones unsplit, blank ones too, and after pass 6 the printout
// ends with an empty line.
//
class Listing {
public:
    explicit Listing(std::ostream &out) : out_(out) {}

    void print(const std::string &text); // AG01644, ASMPRINT
    void blank();                        // AG01713
    void finish();                       // G44504

private:
    std::ostream &out_;
};

#endif
