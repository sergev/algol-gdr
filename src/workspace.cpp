//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "workspace.h"

#include <stdexcept>
#include <string>

#include "card.h"

namespace {

// The name table (02144 + slot): the keywords with their symbol codes and the
// standard identifiers (code 0), entered in this order (the order of their
// words in the original) by the lexer's hash with linear probing. A name
// keeps its first 6 characters.
struct Name {
    const char *name;
    unsigned code;
};

// clang-format off
const Name NAMES[] = {
    { "ARRAY", 040 },     { "ARCTAN", 0 },     { "ABS", 0 },        { "ATOS", 0 },
    { "ASSIGN", 0 },      { "ALGOL", 073 },    { "AND", 003 },      { "BACKSPACE", 071 },
    { "BEGIN", 036 },     { "BITS", 0101 },    { "BOOLEAN", 054 },  { "COMMENT", 0177 },
    { "CODE", 074 },      { "COS", 0 },        { "COMMON", 0377 },  { "DECODE", 066 },
    { "DO", 033 },        { "EQUIV", 043 },    { "EQ", 011 },       { "EQUAL", 011 },
    { "EXP", 0 },         { "ELSE", 031 },     { "END", 037 },      { "ENCODE", 067 },
    { "FALSE", 077 },     { "EOP", 075 },      { "ENDFILE", 072 },  { "ENTIER", 0 },
    { "FOR", 032 },       { "GREATER", 010 },  { "GT", 010 },       { "GE", 007 },
    { "GOTO", 026 },      { "IF", 027 },       { "IMPL", 001 },     { "INTEGER", 055 },
    { "LABEL", 060 },     { "LT", 005 },       { "LESS", 005 },     { "LE", 006 },
    { "LENGTH", 0 },      { "LN", 0 },         { "LOAD", 0 },       { "LOADGO", 0 },
    { "NE", 012 },        { "NEWLINE", 0 },    { "NEWPAGE", 0 },    { "NOT", 004 },
    { "NOTLESS", 007 },   { "OR", 002 },       { "NOTEQUAL", 012 }, { "NOTGREATER", 006 },
    { "OWN", 053 },       { "PROCEDURE", 042 }, { "PRINT", 063 },   { "PUNCH", 064 },
    { "POWER", 017 },     { "REAL", 056 },     { "READ", 062 },     { "REWIND", 070 },
    { "SQRT", 0 },        { "STEP", 034 },     { "STRING", 057 },   { "SWITCH", 041 },
    { "SIN", 0 },         { "SIGN", 0 },       { "STOP", 0 },       { "STOA", 0 },
    { "TRUE", 076 },      { "THEN", 030 },     { "UNTIL", 052 },    { "VALUE", 061 },
    { "WRITE", 065 },     { "WHILE", 035 },
};
// clang-format on

constexpr Word KEYWORD = 07760000000000000; // the word after a keyword: | its code

// The preset constants at 03771… (slots 01625…), the first of each object's
// constant block: TRUE, then the integers 52, 51, 50, 2, 1, 0.
constexpr Word INTEGER        = 06400000000000000;
const Word PRESET_CONSTANTS[] = {
    00020000000000000, INTEGER | 52, INTEGER | 51, INTEGER | 50, INTEGER | 2, INTEGER | 1, INTEGER,
};

// The D-list descriptors of the standard identifiers at 05754…: the bits
// of the descriptor and the name, whose slot fills the low 12 bits.
struct Standard {
    Word bits;
    const char *name;
};

const Standard STANDARD[] = {
    { 06402340000000000, "ASSIGN" },  { 02402000000000000, "ATOS" },
    { 06442300000000000, "LENGTH" },  { 02402000000000000, "STOA" },
    { 06422040000100000, "SIN" },     { 06422040000100000, "COS" },
    { 06422040000100000, "SQRT" },    { 06422040000100000, "LN" },
    { 06422040000100000, "ARCTAN" },  { 06422040000100000, "EXP" },
    { 06422040000100000, "ABS" },     { 06442040000100000, "SIGN" },
    { 06442040000100000, "ENTIER" },  { 06402100000000000, "PRINT" },
    { 06402140000000000, "READ" },    { 06402700000000000, "STOP" },
    { 06002200000000000, "NEWLINE" }, { 06002240000000000, "NEWPAGE" },
    { 02402000000000000, "LOAD" },    { 06402000000000000, "LOADGO" },
};

constexpr unsigned NAMES_BASE = 02144;

// A name's word: its first 6 characters in ISO.
Word name_word(const char *name)
{
    return iso_word(std::string(name).substr(0, 6));
}

// The home slot of a name (AG15414): from its first two characters.
unsigned hash(Word w)
{
    return ((w >> 40) & 037) << 4 | ((w >> 32) & 017);
}

} // namespace

void Workspace::reset()
{
    low_.fill(0);
    comtab_.fill(0);
    Workspace &w = *this;
    w[02000]     = 051;         // the symbol: ';'
    w[02003]     = 01623;       // the next free constant slot
    w[02004]     = 01634 << 12; // the string pool
    w[02030]     = 040;         // the character to skip
    w[02031]     = iso_word("      ");
    w[02050]     = 01634 << 12; // the end of the string pool
    w[02063]     = iso_word("ERROR ");

    for (const auto &n : NAMES) {
        Word name     = name_word(n.name);
        unsigned slot = hash(name);
        while (w[NAMES_BASE + slot])
            slot++;
        w[NAMES_BASE + slot] = name;
        if (n.code)
            w[NAMES_BASE + slot + 1] = KEYWORD | n.code;
    }
    unsigned a = 03771;
    for (Word c : PRESET_CONSTANTS)
        w[a++] = c;
    a = 05754;
    for (const auto &s : STANDARD) {
        Word name     = name_word(s.name);
        unsigned slot = hash(name);
        while (w[NAMES_BASE + slot] != name)
            slot++;
        w[a++] = s.bits | slot;
    }
}

Word *Workspace::word(unsigned addr)
{
    if (addr >= LOW_BEGIN && addr < LOW_END)
        return &low_[addr - LOW_BEGIN];
    if (addr >= COMTAB && addr < COMTAB_END)
        return &comtab_[addr - COMTAB];
    throw std::out_of_range("workspace address " + std::to_string(addr) + " out of range");
}
