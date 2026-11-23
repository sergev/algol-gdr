//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "printm.h"

#include <cstdio>

#include "arith.h"
#include "besm6.h"
#include "listing.h"

namespace {

constexpr Word MASK48 = 0'7777'7777'7777'7777;
constexpr Word BIT41  = 0'0020'0000'0000'0000;

// Constants of AG31341 (at 31254 + offset).
constexpr Word SMALL  = 0'0056'6036'2247'6670; // 222B: below it, the number is 0
constexpr Word TEN18  = 0'7615'7013'3263'5166; // 224B: 1e18
constexpr Word TEN    = 0'4212'0000'0000'0000; // 220B
constexpr Word ONE    = 0'4050'0000'0000'0000; // 221B
constexpr Word TENTH  = 0'0631'4631'4631'4632; // 301B: the fraction after 9.99… rounds up
constexpr Word LOW255 = 0377;                  // 316B: or-ed into the fraction

std::string octal(Word v, int width)
{
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%0*llo", width, (unsigned long long)v);
    return buf;
}

// AG31255: a number in [0.1, 1) as a fraction of 2^48: the mantissa shifted
// by its exponent.
Word fraction(Word x)
{
    unsigned e = (x >> 41) & 0177;
    Word m     = (x << 7) & MASK48;
    int n      = 63 - int(e); // ASN by 0177 - e
    return n >= 0 ? m >> n : (m << -n) & MASK48;
}

// AG31262: the next decimal digit of the fraction f: f * 10 as f/8 + f/2
// (ARX, end-around carry), shifted left by 4; the digit comes out in Y.
unsigned next_digit(Word &f)
{
    Word s = (f >> 3) + (f >> 1);
    if (s > MASK48)
        s = (s & MASK48) + 1;
    unsigned digit = unsigned(s >> 44);
    f              = (s << 4) & MASK48;
    return digit;
}

// BESM-6 operations in the mode of AG31341 (NTR 22B: no rounding).
Word divide(Word a, Word x)
{
    Arith r(a, Arith::ROUND_DISABLE);
    r.div(x);
    return r.acc;
}

bool negative_difference(Word a, Word x) // A - X < 0
{
    Arith r(a, Arith::ROUND_DISABLE);
    r.sub(x);
    return r.acc & BIT41;
}

} // namespace

//
// AG31341: the sign, d.ddddddddddd, the sign of the exponent and its two
// digits. The magnitude is scaled by powers of 10 from 1e18 down, in BESM-6
// arithmetic without rounding; the fraction gets 0377 in its low bits, so
// 0.6 prints as 6.00000000001-01.
//
std::string printm_real(Word w)
{
    std::string s = (w & BIT41) ? "-" : " ";
    Arith abs(w, Arith::ROUND_DISABLE);
    abs.sub_abs(0); // AMX
    Word x  = abs.acc;
    int exp = 0;
    Word f  = 0;
    if (!negative_difference(x, SMALL)) {
        exp    = 18;
        Word t = TEN18;
        while (negative_difference(x, t)) {
            t = divide(t, TEN);
            exp--;
        }
        x = divide(divide(x, t), TEN);
        if (!negative_difference(x, ONE)) {
            exp++;
            f = TENTH;
        } else {
            f = fraction(x) | LOW255;
        }
    }
    s += char('0' + next_digit(f));
    s += '.';
    for (int i = 0; i < 11; i++)
        s += char('0' + next_digit(f));
    unsigned e = exp < 0 ? -exp : exp;
    return s + (exp < 0 ? '-' : '+') + char('0' + e / 10 % 10) + char('0' + e % 10);
}

// ISO characters as the printer shows them, but 0 prints as a blank, a blank
// as ⊃ (034) and 001 as !.
std::string printm_text(Word w)
{
    std::string s;
    for (int shift = 40; shift >= 0; shift -= 8) {
        unsigned c = (w >> shift) & 0377;
        s += c == 0 ? " " : c == 040 ? listing_char(034) : c == 1 ? "!" : listing_char(c);
    }
    return s;
}

namespace {

// One instruction: register, mnemonic, address (5 digits long, 4 short).
std::string insn_text(unsigned cmd)
{
    auto insn = Insn::decode(cmd);
    char buf[32];
    std::string name = besm6_opname(insn.opcode);
    if (insn.reg)
        std::snprintf(buf, sizeof(buf), "%2u %-3s ", insn.reg, name.c_str());
    else
        std::snprintf(buf, sizeof(buf), "   %-3s ", name.c_str());
    return buf + (insn.is_long ? octal(insn.addr, 5) : " " + octal(insn.addr & 07777, 4));
}

// The value row of a constant: none for 0, an integer (exponent 0150), a
// real (a byte with bit 8), or text.
std::string value_text(Word w)
{
    if (!w)
        return "";
    if ((w >> 41) == 0150) {
        int64_t v = int64_t(w << 23) >> 23; // bits 41-1, signed
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%14lld", (long long)v);
        return buf;
    }
    if (w & 0'4010'0200'4010'0200)
        return printm_real(w);
    return printm_text(w);
}

// A row of the description table: names as 6 TEXT characters; symbols by type.
std::string table_text(Word w, bool symbol)
{
    if (!symbol)
        return text_to_utf8(w & ~Word(07777));
    auto sym = Symbol::decode(w);
    switch (sym.type) {
    case SYM_EXT_LONG:
        return "==>" + octal(sym.ref, 4);
    case SYM_EXT_SHORT:
        return text_to_utf8(Word(sym.ref) << 24);
    case SYM_ABS:
        return "-  " + octal(-sym.addr & 07777, 4);
    default:
        return "";
    }
}

class Rows {
public:
    std::vector<std::string> rows;

    // A word: its address (in full at the start of a part and at the top of
    // each column of 64 rows, else the last two digits), its octal value or
    // first instruction, then the value row.
    void word(unsigned base, unsigned i, const std::string &first, const std::string &second)
    {
        bool full = i == 0 || rows.size() % 64 == 0;
        std::string addr =
            full ? " " + octal(base + i, 5) : "    " + octal((base + i) & 077, 2);
        rows.push_back(rtrim(addr + "  " + first));
        rows.push_back(rtrim(std::string(8, ' ') + second));
    }
};

} // namespace

std::vector<std::string> printm_rows(const StdArray &obj)
{
    auto lay = obj.layout();
    Rows out;
    for (unsigned i = 0; i < lay.cmd_len; i++) {
        Word w = obj.words[lay.cmd_off + i];
        out.word(040000, i, insn_text(unsigned(w >> 24)), insn_text(unsigned(w & 077777777)));
    }
    for (unsigned i = 0; i < lay.const_len; i++) {
        Word w = obj.words[lay.const_off + i];
        out.word(0, i, octal(w, 16), value_text(w));
    }
    unsigned nrows = lay.head_len + lay.sym_len + lay.long_len;
    for (unsigned row = 0; row < nrows; row++) {
        Word w      = obj.table_row(lay, row);
        bool symbol = row >= lay.head_len && row < lay.head_len + lay.sym_len;
        out.word(04000, row, octal(w, 16), table_text(w, symbol));
    }
    return out.rows;
}
