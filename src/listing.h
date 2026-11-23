#ifndef ALGOL_LISTING_H
#define ALGOL_LISTING_H

#include <iosfwd>
#include <string>
#include <vector>

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
// The printout of the compiler (AG01644, AG01713 and ASMPRINT). A text of 11
// words or more is split after 10 words, the rest indented by 15 (not with
// setftn:one,long). Rows are gathered into pages of two columns, 65 rows each
// on the first page and 66 after; a blank row prints nothing. A page that
// fills its left column only prints as plain lines, otherwise each row is the
// left cell padded to 65 characters and the right one, and the page ends with
// a form feed. With setftn:one rows are printed at once, blank ones too, and
// the printout ends with an empty line.
//
class Listing {
public:
    Listing(std::ostream &out, bool one_column, bool long_lines)
        : out_(out), one_column_(one_column), split_(!(one_column && long_lines))
    {
    }

    void print(const std::string &text, unsigned indent = 0); // AG01644
    void blank();                                             // AG01713
    void row(const std::string &cell);                        // ASMPRINT: one row
    void finish();                                            // G44504: the last page

private:
    std::ostream &out_;
    const bool one_column_;
    const bool split_;
    std::vector<std::string> rows_; // the page
    unsigned pages_{};

    void print_page();
    unsigned rows_per_column() const { return pages_ ? 66 : 65; }
};

#endif
