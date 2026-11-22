#include "stdarray.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>

static const std::array<const char *, 64> text_chars = {
    " ", ".", "Б", "Ц", "Д", "Ф", "Г", "И", "(", ")", "*", "Й", "Л", "Я", "Ж", "/",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Ь", ",", "П", "-", "+", "Ы",
    "З", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Ш", "Э", "Щ", "Ч", "Ю",
};

std::string text_to_utf8(Word w, bool trim)
{
    std::string s;
    for (int shift = 42; shift >= 0; shift -= 6)
        s += text_chars[(w >> shift) & 077];
    if (trim)
        s.erase(s.find_last_not_of(' ') + 1);
    return s;
}

Word utf8_to_text(const std::string &s)
{
    Word w     = 0;
    unsigned n = 0;
    for (std::size_t i = 0; i < s.size() && n < 8; n++) {
        // Length of UTF-8 sequence.
        unsigned char c = s[i];
        std::size_t len = (c < 0x80) ? 1 : (c < 0xe0) ? 2 : (c < 0xf0) ? 3 : 4;
        std::string ch  = s.substr(i, len);
        i += len;

        auto it       = std::find(text_chars.begin(), text_chars.end(), ch);
        unsigned code = (it == text_chars.end()) ? 0 : unsigned(it - text_chars.begin());
        w             = (w << 6) | code;
    }
    return w << (6 * (8 - n));
}

Symbol Symbol::decode(Word w)
{
    return { unsigned(w >> 24), unsigned((w >> 15) & 0777), unsigned(w & 077777) };
}

Word Symbol::encode() const
{
    return (Word(ref & 077777777) << 24) | (Word(type & 0777) << 15) | (addr & 077777);
}

StdArray StdArray::read_cards(std::istream &in)
{
    StdArray obj;
    std::string line;
    unsigned lineno  = 0;
    unsigned in_card = WORDS_PER_CARD; // words seen in current card

    while (std::getline(in, line)) {
        lineno++;
        auto fail = [&](const std::string &msg) {
            throw StdArrayError("line " + std::to_string(lineno) + ": " + msg);
        };
        if (line.empty())
            continue;
        if (line[0] != '`')
            fail("expected '`'");

        std::size_t pos = 1;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '7')
            pos++;
        if (pos == 1)
            fail("expected octal number");
        Word w = std::stoull(line.substr(1, pos - 1), nullptr, 8);

        if (w == CARD_MARK && pos == 6) {
            // Card header: "`77761 LABEL___ N"
            if (in_card != WORDS_PER_CARD)
                fail("short card");
            in_card = 0;
            if (obj.label.empty() && pos + 1 < line.size()) {
                std::string rest = line.substr(pos + 1);
                auto end         = rest.find_last_of(' ');
                if (end != std::string::npos)
                    obj.label = rest.substr(0, end);
            }
            continue;
        }
        if (in_card >= WORDS_PER_CARD)
            fail("word outside of card");
        if (w > WORD_MASK)
            fail("word too large");
        obj.words.push_back(w);
        in_card++;
    }
    if (in_card != WORDS_PER_CARD)
        throw StdArrayError("short last card");
    if (obj.words.empty())
        throw StdArrayError("no cards");
    return obj;
}

StdArray StdArray::read_cards_file(const std::string &filename)
{
    std::ifstream in(filename);
    if (!in.is_open())
        throw StdArrayError(filename + ": cannot open");
    try {
        return read_cards(in);
    } catch (const StdArrayError &e) {
        throw StdArrayError(filename + ": " + e.what());
    }
}

void StdArray::write_cards(std::ostream &out) const
{
    std::string lab = label;
    if (lab.empty()) {
        Word name = words.empty() ? 0 : words[0];
        lab       = text_to_utf8(name, false);
    }
    unsigned ncards = (words.size() + WORDS_PER_CARD - 1) / WORDS_PER_CARD;
    for (unsigned card = 0; card < ncards; card++) {
        // Card number in octal, as dubna prints it.
        out << "`77761 " << lab << ' ' << std::oct << (card + 1) << '\n';
        for (unsigned i = 0; i < WORDS_PER_CARD; i++) {
            std::size_t k = card * WORDS_PER_CARD + i;
            Word w        = (k < words.size()) ? words[k] : 0;
            out << '`' << std::oct << std::setfill('0') << std::setw(16) << w << '\n';
        }
    }
    out << std::dec;
}

Layout StdArray::layout() const
{
    Layout lay;
    auto at = [&](unsigned i) -> Word {
        if (i >= words.size())
            throw StdArrayError("truncated object at word " + std::to_string(i));
        return words[i];
    };

    // Entries: pairs with nonzero mark in bits 48-46.
    unsigned i = 0;
    while ((at(i + 1) >> 45) != 0) {
        lay.entries.push_back({ at(i), at(i + 1) });
        i += 2;
    }
    lay.head_off = i;

    unsigned *fields[HEADER_LEN] = { &lay.head_len, &lay.sym_len, &lay.word2,   &lay.debug_len,
                                     &lay.long_len, &lay.cmd_len, &lay.bss_len, &lay.const_len,
                                     &lay.data_len, &lay.set_len };
    for (unsigned k = 0; k < HEADER_LEN; k++) {
        Word w = at(i + k);
        if (w > 077777 && k != 2)
            throw StdArrayError("bad header word " + std::to_string(k));
        *fields[k] = unsigned(w);
    }

    lay.cmd_off   = i + HEADER_LEN;
    lay.const_off = lay.cmd_off + lay.cmd_len;
    lay.data_off  = lay.const_off + lay.const_len;
    lay.set_off   = lay.data_off + lay.data_len;
    lay.table_off = lay.set_off + lay.set_len;
    lay.sym_off   = lay.table_off + lay.head_len;
    lay.long_off  = lay.sym_off + lay.sym_len;
    lay.debug_off = lay.long_off + lay.long_len;
    lay.end_off   = lay.debug_off + lay.debug_len;
    if (lay.end_off > words.size())
        throw StdArrayError("parts exceed object size");
    return lay;
}

Word StdArray::table_row(const Layout &lay, unsigned row) const
{
    unsigned rows = lay.head_len + lay.sym_len + lay.long_len;
    if (row >= rows)
        throw StdArrayError("description table row " + std::to_string(row) + " out of range");
    return words[lay.table_off + row];
}
