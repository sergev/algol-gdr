#include "card.h"

namespace {

// Latin-1 range, from dubna/encoding.cpp.
const unsigned char tab0[256] = {
    // clang-format off
    0,    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x1d, 0x5d, 0x5c, 0x5f,
    0,    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x0e, 0x5e, 0x0f, 0x1f, 0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x1f, 0,    0,    0,
    0x19, 0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0x06, 0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0x1a, 0,    0,    0,    0,    0,    0,    0,    0,
    // clang-format on
};

// Cyrillic 0x0410-0x044f, upper and lower case alike, from dubna/encoding.cpp.
const unsigned char cyrillic[32] = {
    // А    Б     В    Г     Д     Е    Ж     З     И     Й     К    Л     М    Н    О    П
    'A',
    0x62,
    'B',
    0x67,
    0x64,
    'E',
    0x76,
    0x7a,
    0x69,
    0x6a,
    'K',
    0x6c,
    'M',
    'H',
    'O',
    0x70,
    // Р  С    Т    У    Ф     Х    Ц     Ч     Ш     Щ     Ъ     Ы     Ь     Э     Ю     Я
    'P',
    'C',
    'T',
    'Y',
    0x66,
    'X',
    0x63,
    0x7e,
    0x7b,
    0x7d,
    0x05,
    0x79,
    0x78,
    0x7c,
    0x60,
    0x71,
};

unsigned utf8_next(const std::string &s, size_t &i)
{
    unsigned c1 = (unsigned char)s[i++];
    if (!(c1 & 0x80) || i >= s.size())
        return c1;
    unsigned c2 = (unsigned char)s[i++];
    if (!(c1 & 0x20) || i >= s.size())
        return (c1 & 0x1f) << 6 | (c2 & 0x3f);
    unsigned c3 = (unsigned char)s[i++];
    return (c1 & 0x0f) << 12 | (c2 & 0x3f) << 6 | (c3 & 0x3f);
}

} // namespace

unsigned char unicode_to_koi7(unsigned u)
{
    switch (u) {
    case '@': // exponent
        return 027;
    case 0xac: // ¬
        return 023;
    case 0xd7: // ×
        return '*';
    }
    if (u < 0x100)
        return tab0[u];
    if (u >= 0x410 && u < 0x450)
        return cyrillic[(u - 0x410) & 037];
    switch (u) {
    case 0x401: // Ё
    case 0x404: // Є
    case 0x451: // ё
    case 0x454: // є
        return 'E';
    case 0x406: // І
    case 0x407: // Ї
    case 0x456: // і
    case 0x457: // ї
        return 'I';
    case 0x490: // Ґ
    case 0x491: // ґ
        return 0x67;
    case 0x2015: // ―
        return 025;
    case 0x2018: // ‘
        return 020;
    case 0x2019: // ’
        return 033;
    case 0x2028:
        return 0x0a;
    case 0x2032: // ′
        return '\'';
    case 0x203e: // ‾
        return '\\';
    case 0x212f:
        return 'E';
    case 0x2191: // ↑
        return 026;
    case 0x2227: // ∧
        return 021;
    case 0x2228: // ∨
        return 020;
    case 0x2260: // ≠
        return 030;
    case 0x2261: // ≡
        return 024;
    case 0x2264: // ≤
    case 0x2a7d: // ⩽
        return 031;
    case 0x2265: // ≥
    case 0x2a7e: // ⩾
        return 032;
    case 0x2283: // ⊃
        return 022;
    case 0x23e8: // ⏨
        return 027;
    case 0x25c7:
        return '$';
    }
    return 0;
}

std::string line_to_koi7(const std::string &line)
{
    std::string out;
    for (size_t i = 0; i < line.size() && out.size() < CARD_COLUMNS;) {
        unsigned u = utf8_next(line, i);
        if (u == '\n' || u == '\r')
            break;
        if (u == 0x2191) {
            out += "**";
            continue;
        }
        unsigned char c = unicode_to_koi7(u);
        if (c)
            out += char(c);
    }
    if (out.size() > CARD_COLUMNS)
        out.resize(CARD_COLUMNS);
    return out;
}

Card make_card(const std::string &line)
{
    std::string text = line_to_koi7(line);
    text.resize(CARD_COLUMNS, ' ');
    Card card{};
    for (unsigned i = 0; i < CARD_COLUMNS; i++)
        card[i / 6] |= Word((unsigned char)text[i]) << (8 * (5 - i % 6));
    return card;
}

std::vector<Card> read_cards(const std::string &text)
{
    std::vector<Card> cards;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        cards.push_back(make_card(text.substr(pos, end - pos)));
        pos = end + 1;
    }
    return cards;
}
