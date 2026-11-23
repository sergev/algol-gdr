//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "source.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

std::size_t find_invalid_utf8(const std::string &text)
{
    const auto *s       = reinterpret_cast<const unsigned char *>(text.data());
    const std::size_t n = text.size();
    for (std::size_t i = 0; i < n;) {
        unsigned c = s[i];
        unsigned len;
        unsigned min;
        if (c < 0x80) {
            i++;
            continue;
        } else if ((c & 0xe0) == 0xc0) {
            len = 2;
            min = 0x80;
            c &= 0x1f;
        } else if ((c & 0xf0) == 0xe0) {
            len = 3;
            min = 0x800;
            c &= 0x0f;
        } else if ((c & 0xf8) == 0xf0) {
            len = 4;
            min = 0x10000;
            c &= 0x07;
        } else {
            return i;
        }
        if (i + len > n)
            return i;
        for (unsigned k = 1; k < len; k++) {
            if ((s[i + k] & 0xc0) != 0x80)
                return i;
            c = (c << 6) | (s[i + k] & 0x3f);
        }
        if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return i;
        i += len;
    }
    return std::string::npos;
}

std::string read_source(const std::string &filename)
{
    std::ifstream input(filename, std::ios::binary);
    if (!input.is_open())
        throw std::runtime_error(filename + ": " + std::strerror(errno));

    std::ostringstream buf;
    buf << input.rdbuf();
    std::string text = buf.str();

    auto pos = find_invalid_utf8(text);
    if (pos != std::string::npos) {
        // Report line number of the bad byte.
        auto line = 1 + std::count(text.begin(), text.begin() + pos, '\n');
        throw std::runtime_error(filename + ":" + std::to_string(line) +
                                 ": invalid UTF-8 sequence");
    }
    return text;
}
