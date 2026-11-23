//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "pass6.h"

#include <algorithm>
#include <cstdio>
#include <ostream>
#include <vector>

namespace {

// Workspace cells (D02000+n), see doc/passes.md.
constexpr unsigned D_NAME      = 02005; // module name, TEXT
constexpr unsigned D_LIST      = 02006; // name for the summary line, ISO
constexpr unsigned D_STRINGS   = 02050; // string pool end << 12
constexpr unsigned D_CMD       = 02054; // code length
constexpr unsigned D_CONST     = 02055; // constants count
constexpr unsigned D_NROWS     = 02057; // symbol rows
constexpr unsigned D_RENAME    = 02137; // module name override
constexpr unsigned ROWS        = 03000; // description table
constexpr unsigned LONG_NAMES  = 04000;
constexpr unsigned LABELS      = 04400;
constexpr unsigned CARDS       = 01741; // card count, as integer
constexpr unsigned STRING_BASE = 01634; // address of the first string word

constexpr Word INFO = 02000000000000000; // entry info word

// Half-word label table access as in the original: ATX/XTA 074400 with C = h & 077777.
unsigned label_addr(unsigned h)
{
    return (074400 + (h & 077777)) & 077777;
}

} // namespace

StdArray pass6(Context &ctx, const Stream &input)
{
    Workspace &ws = ctx.ws;
    auto row      = [&](unsigned i) -> Word      &{ return ws[ROWS + i]; };

    // Row 0 = the module name.
    row(0) = ws[D_RENAME] ? ws[D_RENAME] : ws[D_NAME];

    // Placeholders of long names get their references 04000+rows+k.
    const unsigned nrows = ctx.regs[5];
    ws[D_NROWS]          = nrows;
    unsigned nlong       = 0;
    for (unsigned r = 0; r < nrows; r++) {
        Word a = row(r);
        if (a == 0) {
            a      = (Word(nlong + nrows) << 24) ^ 0000400063000000;
            row(r) = a;
            nlong++;
        }
        if ((a & 077000000) == 067000000) {
            row(r) |= (Word(nlong + nrows) << 24) ^ 0000400000000000;
            nlong++;
        }
    }

    // Constants: the numbers, then the strings.
    const unsigned nstr   = (ws[D_STRINGS] >> 12) - STRING_BASE;
    const unsigned nconst = ws[D_CONST] + nstr;
    ws[D_CONST]           = nconst;
    const unsigned ncmd   = ws[D_CMD];
    if ((ncmd + nconst) & ~Word(077777)) {
        if (ctx.listing)
            ctx.listing->print("PROGRAM TOO LONG");
        return {};
    }

    // Words in the order the original writes them: backward.
    std::vector<Word> out;
    for (unsigned k = nlong; k > 0; k--)
        out.push_back(ws[LONG_NAMES + k - 1]);
    for (unsigned r = nrows; r > 0; r--)
        out.push_back(row(r - 1));
    auto at = [](const std::vector<Word> &v, size_t i) { return i < v.size() ? v[i] : 0; };
    for (unsigned i = nstr; i > 0; i--)
        out.push_back(at(ctx.strings, i - 1));
    for (unsigned i = 0; i < nconst - nstr; i++)
        out.push_back(at(ctx.constants, i));

    // Code: the stream of pass 5, backward, a right half first.
    unsigned addr = ncmd; // words left to write
    bool left     = false;
    Word right    = 0;
    auto emit     = [&](unsigned h) {
        if (!left) {
            right = h;
        } else {
            out.push_back(Word(h) << 24 | right);
            addr--;
        }
        left = !left;
    };
    for (auto it = input.rbegin(); it != input.rend() && addr != 0; ++it) {
        unsigned h  = it->bits;
        bool define = false;
        if ((h & 03700000) == 02000000) {
            emit(02200000); // align: UTC 0
            define = true;
        } else if ((h & 02000000) && (h & 030000) && !(h & 040000)) {
            if ((h & 03700000) == 02100000) {
                define = true;
            } else {
                // Reference to label n.
                Word v = Workspace::contains(label_addr(h)) ? ws[label_addr(h)] : 0;
                if (!(v & 040000)) {
                    h = ((h & 077700000) ^ v ^ 040000) & Item::MASK;
                } else {
                    Word key   = v ^ 041000000;
                    unsigned r = 1;
                    while (row(r) != key)
                        r++;
                    h = (h & 077700000) ^ 070000 ^ r;
                }
                emit(h);
            }
        } else {
            emit(h);
        }
        if (define && Workspace::contains(label_addr(h)))
            ws[label_addr(h)] = addr;
    }

    // Header.
    for (Word w : { Word(0), Word(0), Word(nconst), Word(0), Word(ncmd), Word(nlong), Word(0),
                    Word(0), Word(nrows - 1), Word(1) })
        out.push_back(w);

    StdArray obj;
    obj.words.push_back(row(0));
    obj.words.push_back(INFO);
    if (ws[D_RENAME]) {
        obj.words.push_back(ws[D_NAME]);
        obj.words.push_back(INFO);
    }
    obj.words.insert(obj.words.end(), out.rbegin(), out.rend());
    obj.words.resize((obj.words.size() + WORDS_PER_CARD - 1) / WORDS_PER_CARD * WORDS_PER_CARD);
    obj.label = text_to_utf8(row(0), false);
    return obj;
}

std::string summary_line(const Context &ctx)
{
    const Workspace &ws = ctx.ws;
    if (ws[D_LIST])
        throw NotImplemented("summary line with a module name");
    unsigned len   = ws[D_CMD] + ws[D_CONST];
    unsigned cards = ws[CARDS] & 0xffffff;
    char buf[100];
    std::snprintf(buf, sizeof(buf), "PROGRAM   ДЛИHA:%5u %05oB BPEMЯ:  0.00 CEK. KAPT:%4u", len,
                  len, cards);
    return buf;
}
