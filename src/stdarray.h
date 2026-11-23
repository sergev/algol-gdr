#ifndef ALGOL_STDARRAY_H
#define ALGOL_STDARRAY_H

#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

//
// Standard array: BESM-6 object module, as punched by Dubna compilers.
// See doc/stdarray.md.
//
using Word = uint64_t;

constexpr Word WORD_MASK          = 0xffffffffffffull; // 48 bits
constexpr unsigned WORDS_PER_CARD = 16;
constexpr unsigned HEADER_LEN     = 10;
constexpr unsigned CARD_MARK      = 077761;

// Symbol types used by Algol-GDR.
constexpr unsigned SYM_ABS          = 0400;
constexpr unsigned SYM_EXT_SHORT    = 0430;
constexpr unsigned SYM_EXT_LONG     = 0630;
constexpr unsigned SYM_COMMON_SHORT = 0470; // 'COMMON' block, address = its length
constexpr unsigned SYM_COMMON_LONG  = 0670;

struct StdArrayError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

//
// Description table entry.
//
struct Symbol {
    unsigned ref;  // bits 48-25: short name or 04000+row
    unsigned type; // bits 24-16
    unsigned addr; // bits 15-1

    static Symbol decode(Word w);
    Word encode() const;
};

//
// Entry point: name and info word.
//
struct Entry {
    Word name;
    Word info;
};

//
// Decoded layout: lengths and offsets of all parts.
//
struct Layout {
    std::vector<Entry> entries;
    unsigned head_off{};

    // Header words, in order.
    unsigned head_len{}, sym_len{}, word2{}, debug_len{}, long_len{};
    unsigned cmd_len{}, bss_len{}, const_len{}, data_len{}, set_len{};

    unsigned cmd_off{}, const_off{}, data_off{}, set_off{};
    unsigned table_off{}, sym_off{}, long_off{}, debug_off{}, end_off{};
};

class StdArray {
public:
    std::vector<Word> words; // contents, with padding
    std::string label;       // card label, 8 chars UTF-8 (columns 77-80)

    // Card text form, as in dubna's stdarray.out.
    static StdArray read_cards(std::istream &in);
    static StdArray read_cards_file(const std::string &filename);
    void write_cards(std::ostream &out) const;

    // Decode entries and header. Throws StdArrayError when malformed.
    Layout layout() const;

    // Row of description table.
    Word table_row(const Layout &lay, unsigned row) const;
};

// TEXT code (6 bits per char, 8 chars per word) to UTF-8.
std::string text_to_utf8(Word w, bool trim = true);
// UTF-8 to TEXT code; unknown chars and padding become blanks.
Word utf8_to_text(const std::string &s);

#endif
