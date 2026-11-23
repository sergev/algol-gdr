#include "tables.h"

#include <string>

#include "card.h"

namespace tables {

std::vector<Word> iso_words(std::string_view text)
{
    std::vector<Word> words;
    unsigned n = 0;
    auto put   = [&](unsigned char c) {
        if (n % 6 == 0)
            words.push_back(0);
        words.back() |= Word(c) << (40 - 8 * (n % 6));
        n++;
    };
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        if (c < 0xc0) {
            put(c); // ASCII, or an escape byte
            i++;
            continue;
        }
        size_t len = c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        unsigned u = c & (0x7f >> len);
        for (size_t k = 1; k < len && i + k < text.size(); k++)
            u = u << 6 | (text[i + k] & 077);
        i += len;
        unsigned char koi = unicode_to_koi7(u);
        if (!koi)
            throw std::invalid_argument("no ISO code for U+" + std::to_string(u));
        put(koi);
    }
    return words;
}

Word iso_word(std::string_view text)
{
    auto words = iso_words(text);
    if (words.size() > 1)
        throw std::invalid_argument("more than 6 characters: " + std::string(text));
    return words.empty() ? 0 : words[0];
}

} // namespace tables
