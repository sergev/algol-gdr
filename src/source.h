//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_SOURCE_H
#define ALGOL_SOURCE_H

#include <stdexcept>
#include <string>

//
// Read a UTF-8 source file. Throws std::runtime_error on failure.
//
std::string read_source(const std::string &filename);

//
// Check UTF-8 validity. Returns byte offset of the first bad sequence, or npos.
//
std::size_t find_invalid_utf8(const std::string &text);

// UTF-8 to code points; a bad byte gives U+FFFD.
std::u32string decode_utf8(const std::string &text);

// Append a code point in UTF-8.
void append_utf8(std::string &out, char32_t c);

#endif
