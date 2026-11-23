#include "translated.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "listing.h"

namespace {

constexpr unsigned STACK    = 053401; // M17 when the driver calls a pass
constexpr unsigned REGIM    = 00523;  // *REGIM//: bit 48 setftn:one, bit 47 setftn:long
constexpr unsigned FLGEX    = 00702;
constexpr unsigned NOLISFL  = 00705;
constexpr unsigned DATED    = 01730;  // the header with the date is printed
constexpr unsigned ASMPRINT = 012000; // a row: first and last word on the stack, indent in A;
                                      // returns the word under them in A
constexpr unsigned CURSOR_W = 02001;
constexpr unsigned PRINT    = 01644; // text first..last on the stack, indent in A
constexpr unsigned FORMAT   = 027733;
constexpr unsigned ITOA     = 030001;
constexpr unsigned ITOA_TXT = 00027; // the text of G30001

// Bytes of a format from the word at addr: 0200+k inserts the text of data
// word k (a format itself), 0 ends it.
void expand(const Machine &m, unsigned addr, unsigned data, std::vector<unsigned> &out)
{
    for (;; addr++)
        for (int shift = 40; shift >= 0; shift -= 8) {
            unsigned c = (m.mem[addr & 077777] >> shift) & 0377;
            if (!c)
                return;
            if (c & 0200)
                expand(m, data + c - 0200, data, out);
            else
                out.push_back(c);
        }
}

//
// G27733: print a line, indent 1: the format at [M15-1], with the data at A.
// Builds it on the stack as the original, saves and restores M1-M4, M12,
// M13, pops the format and the word under it; A = the data address.
//
unsigned format_line(Machine &m)
{
    unsigned format = m.mem[(m.M[Machine::SP] - 1) & 077777] & 077777;
    unsigned data   = m.acc & 077777;
    for (unsigned r : { 12u, 3u, 4u, 1u, 2u, 13u }) {
        m.push(m.acc);
        m.acc = m.M[r];
    }
    m.push(m.acc);
    unsigned frame = m.M[Machine::SP];
    std::vector<unsigned> chars;
    expand(m, format, data, chars);
    unsigned last = frame + chars.size() / 6;
    for (unsigned a = frame; a <= last; a++)
        m.mem[a] = 0;
    for (size_t i = 0; i < chars.size(); i++)
        m.mem[frame + i / 6] |= Word(chars[i]) << (40 - 8 * (i % 6));
    m.set_m(Machine::SP, frame + 026);
    m.push(0);
    m.push(frame);
    m.push(last);
    m.set_m(14, 1);
    m.acc = 1;
    m.call(PRINT, 13, 027747);
    m.set_m(Machine::SP, frame);
    m.xta(Machine::SP, 0);
    for (unsigned r : { 13u, 2u, 1u, 4u, 3u, 12u })
        m.sti(0, r);
    m.set_m(Machine::SP, m.M[Machine::SP] - 2);
    return m.M[13];
}

//
// G30001: A = six ISO characters of the number in A, decimal, leading zeros
// blank: "%6u" of (n & 06777777) % 1000000 below a million (above it the
// first digit is the quotient & 017). Leaves the fill character in M14 and the
// text without the last '0' in cell 027; its float cells 025, 026 are not kept
// (nothing else reads them).
//
unsigned itoa6(Machine &m)
{
    unsigned rest = unsigned(m.acc & 06777777);
    Word text     = m.mem[ITOA_TXT];
    unsigned fill = 040;
    for (unsigned d = 100000; d; d /= 10) {
        unsigned q = rest / d;
        rest -= q * d;
        unsigned digit = q & 017;
        unsigned c     = fill;
        if (digit) {
            fill = 060;
            c    = 060 ^ digit;
        }
        text = ((text << 8) ^ c) & WORD_MASK;
    }
    m.mem[ITOA_TXT] = text;
    m.set_m(14, fill);
    m.acc = text | 060;
    m.rmr = 0;
    m.rau = 002;
    m.set_logical();
    return m.M[13];
}

} // namespace

TranslatedPass::TranslatedPass(Context &ctx, Machine::Code code) : ctx_(ctx), code_(code)
{
    m.drums = &ctx.ws;
    m.load_image();
    for (unsigned i = 1; i < 16; i++)
        m.M[i] = ctx.regs[i] & 077777;
    m.M[Machine::SP] = STACK;
    m.mem[FLGEX]     = ctx.flgex;
    m.mem[NOLISFL]   = ctx.options.no_listing;
    m.mem[CHECKFL]   = ctx.options.check();
    m.mem[REGIM]     = (ctx.options.setftn_one ? 04000000000000000 : 0) |
                   (ctx.options.setftn_long ? 02000000000000000 : 0);
    m.mem[DATED]        = ctx.options.no_listing ? 0 : 06000000000000000;
    m.natives[FORMAT]   = format_line;
    m.natives[ITOA]     = itoa6;
    m.natives[ASMPRINT] = [this](Machine &mm) {
        mm.M[Machine::SP] = (mm.M[Machine::SP] - 1) & 077777;
        unsigned last     = mm.mem[mm.M[Machine::SP]] & 077777;
        mm.M[Machine::SP] = (mm.M[Machine::SP] - 1) & 077777;
        unsigned first    = mm.mem[mm.M[Machine::SP]] & 077777;
        unsigned indent   = mm.acc & 0177;
        if (ctx_.listing && last >= first)
            ctx_.listing->row(std::string(indent, ' ') +
                              listing_text(&mm.mem[first], 0, 6 * (last - first + 1)));
        mm.set_m(14, 0);
        mm.xta(Machine::SP, 0); // the caller's A, under the arguments
        return mm.M[13];
    };
}

void TranslatedPass::write_stream(const Stream &s, unsigned entry, Word cursor, bool keep_cursors)
{
    Word saved[2]   = { m.mem[CURSOR_W - 1], m.mem[CURSOR_W] };
    m.mem[CURSOR_W] = cursor;
    for (const Item &item : s) {
        m.acc = item.bits;
        m.set_m(13, Machine::RETURN);
        m.run(code_, entry);
    }
    if (keep_cursors) {
        m.mem[CURSOR_W - 1] = saved[0];
        m.mem[CURSOR_W]     = saved[1];
    }
}

Stream TranslatedPass::run(unsigned entry, unsigned ret)
{
    std::vector<std::pair<unsigned, Word>> writes;
    m.on_entry = [&](Machine &mm, unsigned addr) { writes.emplace_back(addr, mm.acc); };
    m.set_m(13, ret);
    // ALGOL_TRACE=file: address, half and A before each instruction.
    // ALGOL_COVERAGE=file: the halves run, appended.
    std::FILE *tf = nullptr;
    if (const char *name = std::getenv("ALGOL_TRACE"))
        tf = std::fopen(name, "w");
    std::vector<bool> run_halves;
    const char *cov_name = std::getenv("ALGOL_COVERAGE");
    if (cov_name)
        run_halves.resize(2 * Machine::MEMSIZE);
    if (tf || cov_name)
        m.trace = [tf, &run_halves](const Machine &mm, unsigned addr, bool right) {
            if (tf)
                std::fprintf(tf, "%05o %c %016llo\n", addr, right ? 'R' : 'L',
                             (unsigned long long)mm.acc);
            if (!run_halves.empty())
                run_halves[2 * addr + right] = true;
        };
    unsigned pc = m.run(code_, entry);
    m.trace     = nullptr;
    if (tf)
        std::fclose(tf);
    if (cov_name) {
        if (std::FILE *cf = std::fopen(cov_name, "a")) {
            for (unsigned i = 0; i < run_halves.size(); i++)
                if (run_halves[i])
                    std::fprintf(cf, "%05o %c\n", i / 2, i % 2 ? 'R' : 'L');
            std::fclose(cf);
        }
    }
    m.on_entry = nullptr;
    if (pc != ret)
        throw std::runtime_error("translated pass left at " + std::to_string(pc));

    for (unsigned a = Workspace::LOW_BEGIN; a < Workspace::LOW_END; a++)
        ctx_.ws[a] = m.mem[a];
    for (unsigned a = Workspace::COMTAB; a < Workspace::COMTAB_END; a++)
        ctx_.ws[a] = m.mem[a];
    for (unsigned i = 1; i < 16; i++)
        ctx_.regs[i] = m.M[i];
    ctx_.flgex = unsigned(m.mem[FLGEX]);

    Stream out;
    bool backward = false;
    for (auto &[addr, value] : writes) {
        backward = addr != 027126;
        out.push_back(Item{ uint32_t(value & 077777777) });
    }
    if (backward)
        std::reverse(out.begin(), out.end());
    return out;
}
