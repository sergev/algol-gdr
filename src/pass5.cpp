#include "pass5.h"

#include <cstdio>
#include <ostream>
#include <stdexcept>
#include <string>


namespace {

// Workspace cells (D02000+n), see doc/passes.md.
constexpr unsigned D_CONST_LIM = 02003; // 01633 - number of constants
constexpr unsigned D_END_LABEL = 02020; // label at the end of the code, 1nnnn
constexpr unsigned D_CMD       = 02054; // code length in words
constexpr unsigned D_CONST     = 02055; // number of constants
constexpr unsigned CONSTS      = 03777; // constants, downward
constexpr unsigned ROWS        = 03000; // description table
constexpr unsigned RTNAMES     = 02400; // run-time names, then their rows
constexpr unsigned LONG_NAMES  = 04000;
constexpr unsigned PAGE        = 04000; // page read from drum 026 zone 0
constexpr unsigned COMTAB      = 027246;

constexpr Word BIT48 = 04000000000000000;

const char *const HEADING = "CTPOKИ             OTHOCИTEЛЬHЫE AДPECA";

// Names of the run-time routines (G30120), in TEXT at 02400…: code address
// 04000+k calls routine k.
const char *const RUNTIME_NAMES[] = {
    "A/MINUS", "A/NOT", "A/RTOI", "A/SIGN", "A/AND", "A/EQUIV", "A/I0", "A/CALLS", "A/CALLP",
    "A/PARCON", "A/PARVAR", "A/PARPAR", "A/START", "A/PARLAB", "A/PARVAL", "A/PAREXP", "A/PROCRT",
    "A/ARRD/V", "A/ARRD/K", "A/RHOCHI", "A/IHOCHI", "POWER", "A/IMPL", "A/PARSTR", "SIN*", "COS*",
    "SQRT*", "LN*", "ARCTAN*", "EXP*", "BCDDEC*", "A/SIGN", "ENTIER*", "A/RPRI", "A/IPRI", "A/BPRI",
    "A/SPRI", "A/LPRI", "A/NPAGE", "A/STOP", "A/RAPRI", "A/IAPRI", "A/BAPRI", "A/SAPRI", "A/LAPRI",
    "BINREAD*", "BINWRIT*", "IOAC*", "A/GETARR", "A/PUTARR", "IOEND*", "*BACKS", "*REWIN", "*ENDFI",
    "A/LOADGO", "LOADGO", "A/SLTS", "A/SLES", "A/SGES", "A/SGTS", "A/SEQS", "A/SNES", "A/SPLUSS",
    "A/SSULPS", "A/SLICE", "BCDREAD*", "A/RREAD", "A/IREAD", "A/BREAD", "A/SREAD", "A/LREAD",
    "A/NLINE", "BCDWRIT*", "A/ARGSTR", "A/RNLINE", "BCDENC*", "A/SUBCH", "A/OPTLIN", "A/VLM1",
    "A/VLMX", "A/NOTL", "A/LHOCHR", "A/LHOCHI", "A/RAREAD", "A/IAREAD", "A/BAREAD", "A/SAREAD",
    "A/LAREAD", "LENGTH", "A/COMMON",
};

// Label table access as in the original: 074400 with C = h & 077777.
unsigned label_addr(unsigned h)
{
    return (074400 + (h & 077777)) & 077777;
}

// Run-time name entry: 076400 + (h & 07777), so 04000+k is 02400+k.
unsigned rtname_addr(unsigned h)
{
    return (076400 + (h & 07777)) & 077777;
}

//
// Line table: rows of 8 source lines, each cell the address of the line's code.
//
class LineTable {
public:
    explicit LineTable(Listing *out) : out_(out) { clear(); }

    // Line n starts at code address addr.
    void mark(unsigned n, unsigned addr)
    {
        while (n > first_ + 7) {
            print();
            first_ += 8;
            clear();
        }
        if (n == 0)
            return;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%05o", addr & 077777);
        cells_[n - first_] = buf;
    }

    void print() const
    {
        if (!out_)
            return;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%4u - ", first_);
        std::string line = buf;
        for (unsigned i = 0; i < 8; i++)
            line += cells_[i] + " ";
        line.erase(line.find_last_not_of(' ') + 1);
        out_->print(line);
    }

private:
    Listing *out_;
    unsigned first_{ 1 };
    std::string cells_[8];

    void clear()
    {
        for (auto &c : cells_)
            c = "     ";
    }
};

} // namespace

void restore_rtnames(Workspace &ws)
{
    unsigned i = 0;
    for (const char *name : RUNTIME_NAMES)
        ws[RTNAMES + i++] = utf8_to_text(name);
}

Stream pass5(Context &ctx, const Stream &input)
{
    Workspace &ws = ctx.ws;
    Listing *list = ctx.options.list ? ctx.listing : nullptr;
    if (list) {
        blank_line(ctx);
        list->print(HEADING);
    }
    LineTable lines(list);

    // Constants to the top of the page saved by pass 2, which goes to drum 027 zone 033.
    for (unsigned i = 0; i < 1024; i++)
        ws[PAGE + i] = ws.drum(026, 0, i);
    const unsigned nconst = 01633 - ws[D_CONST_LIM];
    ws[D_CONST]           = nconst;
    for (unsigned i = 0; i < nconst; i++)
        ws[PAGE + 01777 - i] = ws[CONSTS - i];
    for (unsigned i = 0; i < 1024; i++)
        ws.drum(027, 033, i) = ws[PAGE + i];
    restore_rtnames(ws);
    for (unsigned a = 04400; a < 05777; a++)
        ws[a] = 0;

    Stream out;
    out.push_back(Item{ 0 }); // written before the stream proper
    unsigned halves   = 0;    // M3
    bool word_start   = true; // D4
    unsigned longname = LONG_NAMES;
    unsigned row      = 1; // M5
    auto rows         = [&](unsigned r) -> Word         &{ return ws[ROWS + r]; };

    auto emit = [&](Word h) {
        word_start = !word_start;
        halves++;
        ws[D_CMD] = halves >> 1;
        if (ws[D_CMD] == 034000)
            throw ProgramTooLong();
        out.push_back(Item{ uint32_t(h & Item::MASK) });
    };

    // A name of the run-time table gets its row on first use; the reference
    // becomes the row number.
    auto rtname = [&](unsigned h) {
        const unsigned at = rtname_addr(h);
        if (ws[at] >> 11) {
            Word name = ws[at];
            if ((name & 07700000000000000) == 01200000000000000) {
                // *NAME: a 'COMMON' block, find it in the table.
                Word key = Word(h & 07777) << 24;
                for (unsigned i = 0;; i += 3) {
                    Word w = ws[COMTAB + i];
                    if (w == 0)
                        break;
                    if ((w & 07777000000000000) || (w & 0000777700000000) != key)
                        continue;
                    Word addr = w & 077777;
                    if ((name << 24) & 07777777777777777) {
                        ws[longname++] = name;
                        rows(row)      = 067000000 ^ addr;
                    } else {
                        rows(row) = name ^ 047000000 ^ addr;
                    }
                    goto done;
                }
            }
            if ((name << 24) & 07777777777777777) {
                ws[longname++] = name;
                rows(row)      = 0; // placeholder, see pass 6
            } else {
                rows(row) = name ^ 043000000;
            }
        done:
            ws[at] = row++;
        }
        Word v = (h & 077774000) ^ ws[at];
        return (v & 02000000) ? v ^ 070000 : v;
    };

    // Absolute value: a row of type 0400, shared by equal values.
    auto absolute = [&](unsigned h) {
        Word value;
        if (h & 02000000) {
            h ^= 070000;
            value = h & 077777;
        } else {
            value = h & 07777;
        }
        const Word key = value ^ 040000000;
        unsigned r     = 0;
        while (r < row && rows(r) != key)
            r++;
        if (r == row) {
            rows(row) = key;
            r         = row++;
        }
        Word v = (h & 077774000) ^ r;
        return (v & 02000000) ? v : v ^ 01000000;
    };

    auto label_ref = [&](unsigned h) -> Word {
        const unsigned at   = label_addr(h);
        const Word v        = ws[at];
        const bool in_range = ((ws[D_END_LABEL] - (h & 07777)) & 010000) != 0;
        if (v == 0) {
            if (!in_range)
                ws[at] = BIT48;
            return h;
        }
        if (!in_range)
            ws[at] = 0;
        if (v & 040000)
            return (h & 077700000) ^ 070000 ^ (v >> 24);
        return ((h & 077700000) ^ 040000) | v;
    };

    for (Item item : input) {
        const unsigned h = item.bits;
        switch (h & 03700000) {
        case 03300000: // source line
            if (list)
                lines.mark(h & 077777, ws[D_CMD]);
            continue;
        case 02000000: // align
            if (!word_start)
                emit((h & 077777) | 02200000);
            continue;
        case 02100000: { // label definition
            // At a word start it takes no space, else it fills the right half.
            unsigned d = h;
            if (word_start) {
                halves--;
                word_start = false;
            } else {
                d = (h & 077777) ^ 02000000;
            }
            const unsigned at = label_addr(d);
            if (ws[at] & BIT48) {
                ws[at] = 0;
                emit(d);
            } else {
                emit(d);
                Word v = ws[D_CMD] & 077777;
                if (ws[D_CMD] & 040000) {
                    v ^= Word(row) << 24;
                    rows(row++) = ws[D_CMD] ^ 041000000;
                }
                ws[at] = v ^ 02000000;
            }
            if ((d & 077777) == ws[D_END_LABEL]) {
                lines.mark(0, 0);
                lines.print();
                if (list)
                    blank_line(ctx);
                ctx.regs[5] = row;
                return out;
            }
            continue;
        }
        }
        if (!(h & 02004000) || (h & 02074000) == 02000000) {
            emit(h);
        } else if ((h & 02007700) == 07700) {
            emit(absolute(h));
        } else if ((h & 02000000) && (h & 070000)) {
            emit(label_ref(h));
        } else if ((h & 02000000) && (h & 07700) == 07700) {
            emit(absolute(h));
        } else {
            emit(rtname(h));
        }
    }
    throw std::runtime_error("pass 5: no end label in the stream");
}
