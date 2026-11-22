#ifndef ALGOL_ITEM_H
#define ALGOL_ITEM_H

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

//
// Item of a pass stream: a 24-bit half-word.
// Passes 1-3: value (12 bits) | count (5 bits) | code (7 bits).
// Passes 4-5: instruction half-words and pseudo-items, see doc/passes.md.
//
struct Item {
    uint32_t bits{};

    static constexpr uint32_t MASK = 077777777;

    static Item make(unsigned code, unsigned count = 0, unsigned value = 0)
    {
        return Item{ (value & 07777) << 12 | (count & 037) << 7 | (code & 0177) };
    }
    unsigned code() const { return bits & 0177; }
    unsigned count() const { return (bits >> 7) & 037; }
    unsigned value() const { return (bits >> 12) & 07777; }

    bool operator==(const Item &) const = default;
};

using Stream = std::vector<Item>;

//
// Text form (NAME.sN): one item per line, 8 octal digits.
//
Stream read_stream(std::istream &in);
Stream read_stream_file(const std::string &filename);
void write_stream(std::ostream &out, const Stream &stream);

#endif
