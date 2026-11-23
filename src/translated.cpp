#include "translated.h"

#include <algorithm>

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

constexpr unsigned SP = Machine::SP;

//
// G27114…G27213: the stream module, as the original instruction by
// instruction (doc/passes.md). Cursors at 02000 (read) and 02001 (write);
// the value in and out in A, return through M13.
//

// 27173: page I/O, op in M14: 0 read forward, 1 write forward, 2 read
// backward, 3 write backward. M11, M12 and A are kept.
unsigned page_io(Machine &m)
{
    m.its(0, 013);
    m.its(0, 014);
    m.its(0, 016);
    m.utc(0, 027232);
    m.aax(0, 0);
    m.ati(0, 013);
    m.ita(0, 016);
    m.asn(0, 0101);
    m.ati(0, 014);
    m.utm(12, 027214);
    m.xta(11, 02000);
    m.arx(12, 0);
    m.atx(11, 02000);
    m.vtm(12, 027114);
    m.asn(0, 0117);
    m.atx(SP, 0);
    m.yta(0, 0);
    m.aex(SP, 0);
    m.apx(12, 0130);
    m.aux(12, 0122);
    m.xts(11, 02000);
    m.aax(12, 0126);
    m.aex(SP, 0);
    m.atx(11, 02000);
    m.apx(12, 0123);
    m.aux(12, 0115);
    m.utc(0, 027216);
    m.aex(11, 0);
    m.atx(SP, 1);
    m.extracode(070, SP, 1);
    m.xta(SP, 0);
    m.sti(0, 014);
    m.sti(0, 013);
    return m.M[13];
}

// 27123: after a backward write, the page when its first word is passed.
unsigned write_back_end(Machine &m)
{
    m.ita(0, 016);
    m.asn(0, 032);
    if (m.u1a())
        return m.M[13];
    m.vtm(14, 3);
    return page_io(m);
}

// 27114: write a half, backward.
unsigned write_back(Machine &m)
{
    if (m.on_entry)
        m.on_entry(m, 027114);
    m.xts(0, 02001);
    m.ati(0, 016);
    m.utc(0, 027240);
    m.arx(0, 0);
    m.atx(0, 02001);
    if (!m.uza()) {
        m.xta(SP, 0);
        m.atx(14, 0);
        return m.M[13];
    }
    m.xta(SP, 0);
    m.asn(0, 050);
    m.aex(14, 0);
    m.atx(14, 0);
    return write_back_end(m);
}

// 27126: write a half, forward.
unsigned write_forward(Machine &m)
{
    if (m.on_entry)
        m.on_entry(m, 027126);
    m.xts(0, 02001);
    m.ati(0, 016);
    m.utc(0, 027241);
    m.arx(0, 0);
    m.atx(0, 02001);
    if (!m.u1a()) {
        m.xta(SP, 0);
        m.asn(0, 050);
        m.atx(14, 0);
        return m.M[13];
    }
    m.xta(SP, 0);
    m.aex(14, 0);
    m.atx(14, 0);
    m.xta(0, 02001);
    m.asn(0, 032);
    if (m.u1a())
        return m.M[13];
    m.vtm(14, 1);
    return page_io(m);
}

// 27137: write a word, backward.
unsigned write_word_back(Machine &m)
{
    if (m.on_entry)
        m.on_entry(m, 027137);
    m.xts(0, 02001);
    m.ati(0, 016);
    m.utc(0, 027243);
    m.arx(0, 0);
    m.atx(0, 02001);
    m.xta(SP, 0);
    m.atx(14, 0);
    return write_back_end(m);
}

// 27146: the right half of the word at M14.
unsigned right_half(Machine &m)
{
    m.xta(14, 0);
    m.utc(0, 027235);
    m.aax(0, 0);
    return m.M[13];
}

// 27143: read a half, backward.
unsigned read_back(Machine &m)
{
    m.xta(0, 02000);
    m.ati(0, 016);
    m.utc(0, 027240);
    m.arx(0, 0);
    m.atx(0, 02000);
    if (!m.uza())
        return right_half(m);
    m.ita(0, 016);
    m.asn(0, 032);
    bool more = m.u1a();
    m.xta(14, 0);
    m.asn(0, 0130);
    if (more)
        return m.M[13];
    m.vtm(14, 2);
    return page_io(m);
}

// 27154: read a half, forward.
unsigned read_forward(Machine &m)
{
    m.xta(0, 02000);
    m.ati(0, 016);
    m.utc(0, 027241);
    m.arx(0, 0);
    m.atx(0, 02000);
    if (!m.u1a()) {
        m.xta(14, 0);
        m.asn(0, 0130);
        return m.M[13];
    }
    m.xta(0, 02000);
    m.asn(0, 032);
    if (m.u1a())
        return right_half(m);
    m.xta(14, 0);
    m.utc(0, 027235);
    m.aax(0, 0);
    m.vtm(14, 0);
    return page_io(m);
}

// 27165: read a word, backward.
unsigned read_word_back(Machine &m)
{
    m.xta(0, 02000);
    m.ati(0, 016);
    m.utc(0, 027243);
    m.arx(0, 0);
    m.atx(0, 02000);
    m.ita(0, 016);
    m.asn(0, 032);
    bool more = !m.uza();
    m.xta(14, 0);
    if (more)
        return m.M[13];
    m.vtm(14, 2);
    return page_io(m);
}

//
// AG01644: print the text first..last (on the stack, popped with the word
// under them, which is returned in A), as the original instruction by
// instruction: unless setftn:one,long, a text of 11 words or more goes out
// as 10 words, then the rest without trailing blank words, indented by 15.
// The date header (01524) is printed before any pass runs.
//
unsigned print_text(Machine &m)
{
    m.rte(0, 07777);
    m.atx(0, 01736);
    m.xta(0, 0);
    m.atx(0, 01740);
    for (;;) {
        m.ita(0, 015);
        m.asn(0, 061);
        m.aox(SP, 0);
        m.asn(0, 061);
        m.aox(SP, 0);
        m.atx(0, 01737);
        m.xta(0, DATED);
        m.aox(0, NOLISFL);
        if (!m.u1a())
            throw std::runtime_error("01644: no date header");
        m.xta(0, 01737);
        m.ati(0, 016);
        m.asn(0, 0117);
        m.ati(0, 015);
        m.xta(0, REGIM);
        m.aax(0, 01733);
        m.aex(0, 01733);
        bool split = false;
        if (!m.uza()) {
            m.ita(0, 015);
            m.ntr(0, 3);
            m.its(0, 016);
            m.arith(SP, 0, 006); // x-a
            m.arith(0, 01732, 005);
            split = m.uza();
        }
        if (!split) {
            m.ita(0, 016);
            m.its(0, 015);
            m.xts(0, 01740);
            m.call(ASMPRINT, 13, 01667);
            break;
        }
        m.xta(0, 01737);
        m.arx(0, 01732);
        m.atx(0, 01737);
        m.ita(0, 016);
        m.utm(14, 011);
        m.its(0, 016);
        m.xts(0, 01740);
        m.call(ASMPRINT, 13, 01676);
        m.xts(0, 01737);
        m.ati(0, 016);
        m.asn(0, 0117);
        m.ati(0, 015);
        bool blank = false;
        for (;;) { // trailing blank words
            m.xta(13, 0);
            m.aax(0, 01734);
            if (m.u1a())
                break;
            m.ita(0, 015);
            m.its(0, 016);
            m.aex(SP, 0);
            if (m.uza()) {
                m.stx(0, 0);
                blank = true;
                break;
            }
            m.utm(13, 077777);
        }
        if (blank)
            break;
        m.xta(0, 01735);
        m.atx(0, 01740);
        m.ita(0, 016);
        m.its(0, 015);
        m.xts(0, 01740);
        m.xts(0, 01737);
        m.asn(0, 0136);
        m.sti(0, 015);
    }
    m.xts(0, 01737);
    m.asn(0, 0136);
    m.sti(0, 015);
    m.xtr(0, 01736);
    return m.M[13];
}

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

TranslatedPass::TranslatedPass(Context &ctx) : ctx_(ctx)
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
    m.natives[PRINT]    = print_text;
    m.natives[027114]   = write_back;
    m.natives[027126]   = write_forward;
    m.natives[027137]   = write_word_back;
    m.natives[027143]   = read_back;
    m.natives[027154]   = read_forward;
    m.natives[027165]   = read_word_back;
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
        m.run(entry);
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
    unsigned pc = m.run(entry);
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

Stream stream_left(Context &ctx)
{
    TranslatedPass pass(ctx);
    Machine &m          = pass.m;
    Word back           = m.mem[02001] + 0'7777'7777'7757'7777; // a zone back, cyclic
    m.mem[02000]        = (back >> 48) ? (back + 1) & WORD_MASK : back;
    Stream out;
    m.call(027154, 13, Machine::RETURN); // priming read
    for (;;) {
        m.call(027154, 13, Machine::RETURN);
        out.push_back(Item{ uint32_t(m.acc & 077777777) });
        if ((m.acc & 0177) == 075)
            return out;
        if (out.size() > 1000000)
            throw std::runtime_error("no end of the stream on drum");
    }
}
