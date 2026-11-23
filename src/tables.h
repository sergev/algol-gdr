//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_TABLES_H
#define ALGOL_TABLES_H

#include <array>

#include "word.h"

//
// Tables that passes 1 and 2 share, indexed as the stream items refer to
// them: the name/constant table by slot (word 02144+slot of the original),
// the D-list by its address (04400-05777), the 'COMMON' table by word
// (27245+i). See doc/tables.md.
//
class Tables {
public:
    static constexpr unsigned SLOTS       = 01634; // slots 0…01633
    static constexpr unsigned DLIST_BEGIN = 04400;
    static constexpr unsigned STANDARD    = 05754; // descriptors of the standard identifiers
    static constexpr unsigned DLIST_END   = 06000;
    static constexpr unsigned COMMON_SIZE = 0400;

    // As the driver's SET directives leave them.
    Tables();

    Word &name(unsigned slot) { return names_.at(slot); }
    Word name(unsigned slot) const { return names_.at(slot); }
    Word &dlist(unsigned addr) { return dlist_.at(addr - DLIST_BEGIN); }
    Word dlist(unsigned addr) const { return dlist_.at(addr - DLIST_BEGIN); }
    Word &common(unsigned i) { return common_.at(i); }
    Word common(unsigned i) const { return common_.at(i); }

    const std::array<Word, SLOTS> &names() const { return names_; }

    Word free_const{ 01623 }; // 02003: the lowest constant slot (low bits)

private:
    std::array<Word, SLOTS> names_{};
    std::array<Word, DLIST_END - DLIST_BEGIN> dlist_{};
    std::array<Word, COMMON_SIZE> common_{};
};

#endif
