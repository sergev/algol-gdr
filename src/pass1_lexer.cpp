//
// Lexer of pass 1: AG15414 and the character reader AG16225, translated label
// by label. Labels *nnnnB are offsets in os-dubna/algol/ag14504.asm.
//
#include <cstdio>
#include <ostream>

#include "listing.h"
#include "pass1.h"
#include "tables.h"

namespace {

constexpr Word MASK48 = 07777777777777777;
constexpr Word ONES   = 07777777777777777;
constexpr Word BLANKS = 01002004010020040; // six blanks, ISO
constexpr Word INT0   = 06400000000000000; // integer zero: exponent 064, unnormalized
constexpr Word INT1   = 06400000000000001;
constexpr Word INT10  = 06400000000000012;
constexpr Word TWO40  = 06450000000000000; // 2^40
constexpr Word REAL1  = 04050000000000000; // 1.0
constexpr Word REAL10 = 04212000000000000; // 10.0
constexpr Word C1_25  = 04052000000000000; // 10/8

constexpr unsigned R15 = 077777;

// ARX: add with end-around carry.
Word arx(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & MASK48;
    return s;
}

unsigned neg15(unsigned m)
{
    return arx(m ^ R15, 1) & R15;
}

// Number conversion runs in the mode of NTR 2, with exceptions as in the original.
Word to_integer(Word a)
{
    Arith x(a, Arith::NORM_DISABLE | Arith::ROUND_DISABLE);
    x.add(INT0);
    return x.acc;
}

} // namespace

//
// The symbol of a Unicode operator that dubna encodes where the original
// lexer has none (ERROR 01): ∧ ∨ ⊃ ¬ ≡ ≤ ≥ as the one-character symbols at
// 021, 020, 022, 023, 024, 031, 032; × as *, ↑ as **. 0 for other codes.
//
unsigned Pass1::unicode_operator(unsigned c)
{
    unsigned like = 0;
    switch (c) {
    case '&': // ∧
        like = 021;
        break;
    case 036: // ∨
        like = 020;
        break;
    case 034: // ⊃
        like = 022;
        break;
    case 037: // ¬
        like = 023;
        break;
    case 035: // ≡
        like = 024;
        break;
    case 016: // ≤
        like = 031;
        break;
    case 017: // ≥
        like = 032;
        break;
    case 006: // ×
        return 015;
    case 026: // ↑
        return 017;
    default:
        return 0;
    }
    return (chclass(like) & 077400017) >> 17;
}

Word Pass1::chclass(unsigned c)
{
    return tables::chclass.words[c & 0177];
}

void Pass1::error(unsigned n)
{
    ws_[D_SYM] = 0;
    throw SyntaxError{ n };
}

//
// Next character of the source (AG16225, *1521B); with advance=false the
// current one again (*1522B). Blanks are skipped, outside strings.
//
unsigned Pass1::nextch(bool advance)
{
    if (advance)
        m6_ = (m6_ + 1) & R15;
L1522:
    if (m6_ != 0)
        goto L1547;
L1523:
    m6_ = 077772;
    if (m5_ != 0) {
        m5_ = (m5_ + 1) & R15;
        goto L1545;
    }
    ws_[D_LINE] = arx(ws_[D_LINE], 1);
    ws_[CARDS]  = arx(ws_[CARDS], 1) | INT0;
    if (next_card_ >= cards_.size() || cards_[next_card_][0] == 01244251621020106) // *END F
        throw NoEop{};
    read_card();
    if (ws_[D_LIST])
        print_card();
    m5_ = 077765;
    m6_ = 077772;
L1545:
    ws_[D_WORD] = ws_[(02023 + m5_) & R15];
    if (ws_[D_WORD] == ws_[D_BLANKS])
        goto L1523;
L1547: {
    int pos     = int(m6_) - 0100000; // -6…-1
    unsigned c  = (ws_[D_WORD] >> (8 * (-pos - 1))) & 0377;
    ws_[D_CHAR] = c;
    if (c == ws_[D_BLANK]) {
        m6_ = (m6_ + 1) & R15;
        goto L1522;
    }
    return c;
}
}

Pass1::Pass1(Context &ctx) : ctx_(ctx), ws_(ctx.ws), cards_(read_cards(ctx.source))
{
    // From the start of AG13666.
    ws_[D_LIST] = ctx.options.list ? 1 : 0;
    ws_[D_POOL] = 04420276740004000;
}

void Pass1::read_card()
{
    const Card &card = cards_[next_card_++];
    for (unsigned i = 0; i < CARD_WORDS; i++)
        ws_[CARD + i] = card[i];
}

//
// AG27652: the card in the listing: the line number and all 80 columns.
//
void Pass1::print_card()
{
    if (!ctx_.listing)
        return;
    Word card[CARD_WORDS];
    for (unsigned i = 0; i < CARD_WORDS; i++)
        card[i] = ws_[CARD + i];
    char num[16];
    std::snprintf(num, sizeof(num), "%6u.     ", unsigned(ws_[D_LINE] & 077777777));
    print_line(num + listing_text(card, 0, 80));
}

// G27220: put a word into the string pool; a full sector of 256 words goes
// to drum 027 zone 034.
void Pass1::pool_put(Word w)
{
    ws_[ws_[D_POOL] & R15] = w;
    ws_[D_POOL]            = arx(ws_[D_POOL], 1);
    if ((ws_[D_POOL] << 40) & MASK48)
        return;
    ws_[D_POOL] = arx(ws_[D_POOL], 037777400);
    unsigned n  = pool_sectors_++;
    for (unsigned i = 0; i < 256; i++)
        ws_.drum(027, 034 + n / 4, (n % 4) * 256 + i) = ws_[04000 + i];
}

// *1102B: a word of a string to the pool.
void Pass1::store_string_word()
{
    ws_[D_STRINGS] = arx(ws_[D_STRINGS], 010000);
    unsigned m9    = ((ws_[D_STRINGS] >> 12) - 01634 - 06000) & R15;
    if (m9 == 0)
        error(12);
    pool_put(ws_[D_NAME]);
}

//
// *1660B: put a constant into the table, or find an equal one; returns its slot.
// The slots go down from D_CONST; the presets are at the top.
//
unsigned Pass1::intern(Word value, unsigned slot)
{
    ws_[D_KEY]        = value;
    ws_[NAMES + slot] = value;
    unsigned m14      = 01633;
    while (ws_[(NAMES + m14) & R15] != ws_[D_KEY])
        m14 = (m14 - 1) & R15;
    if (m14 != ws_[D_CONST])
        return m14;
    slot         = (slot - 1) & R15;
    ws_[D_CONST] = slot;
    if (ws_[(NAMES + slot) & R15])
        error(7);
    return slot + 1;
}

void Pass1::lex()
{
    Word a;
    unsigned c, m8 = 0, m10 = 0, m11 = 0;
    Word stack = 0; // the one word the octal escapes keep on the stack

    c = nextch(false);
    if (ws_[D_AFTEREND])
        goto L1461;
L0712:
    m11 = ws_[D_CHAR];
    a   = chclass(m11) & 077400017;
    switch (a & 017) {
    case 0: // letter
        a = 0;
        goto L0743;
    case 1: // quote
        c   = nextch();
        m11 = c;
        switch ((chclass(c) & 060) >> 4) {
        case 0: // '': a string
            a = ws_[D_CONST];
            goto L1015;
        case 1: // '(' ')' string, or an error
            if (c != '(')
                error(2);
            c = nextch();
            if (c != '\'')
                error(2);
            a = ws_[D_CONST];
            goto L1015;
        case 2: // a keyword
            a = 2;
            goto L0743;
        default: // an octal constant
            m10   = 0;
            stack = 0;
            goto L0746;
        }
    case 2: // '.', or '.,' for ';'
        m4_ = 051;
        c   = nextch();
        if (c == ',')
            goto L0766;
        ws_[D_POINT] = c ^ ',';
        ws_[D_FRAC]  = c ^ ',';
        a            = 0;
        goto L1557;
    case 3: // ':' or ':='
        m4_ = 050;
        if (nextch() != '=')
            goto L0777;
        m4_ = 022;
        goto L0766;
    case 4: // '*' or '**'
        m4_ = 015;
        if (nextch() != '*')
            goto L0777;
        m4_ = 017;
        goto L0766;
    case 5: // number
        a            = 0;
        ws_[D_POINT] = 0;
        goto L1557;
    case 6: // '(' or '(/'
        m4_ = 020;
        if (nextch() != '/')
            goto L0777;
        m4_ = 021;
        goto L0766;
    case 7: // '/' or '/)'
        m4_ = 016;
        if (nextch() != ')')
            goto L0777;
        m4_ = 046;
        goto L0766;
    case 010: // '"'
        a = 0377;
        goto L1015;
    case 011: // a one-character symbol
        a >>= 17;
        goto L1012;
    case 012: // ERROR 01, except for the Unicode operators (an extension)
        if (unsigned op = unicode_operator(m11)) {
            a = op;
            goto L1012;
        }
        error(1);
    case 013: // ',', or ';' after '.,' at the end of a number
        if (!ws_[D_SEMI]) {
            a = 047;
            goto L1012;
        }
        m4_         = 051;
        ws_[D_SEMI] = 0;
        goto L0766;
    default:
        error(1);
    }

L0746: // '<octal digits>'
    m10 = (m10 - 16) & R15;
    if (m10 == 0)
        error(14);
    m10 = (m10 + 021) & R15;
    if (!(chclass(ws_[D_CHAR]) & 0100000))
        error(15);
    stack = ((stack << 3) & MASK48) | (ws_[D_CHAR] & 7);
    if (nextch() != '\'')
        goto L0746;
    nextch();
    m8         = ws_[D_CONST];
    a          = Word(intern(stack, m8)) << 12;
    m4_        = 0105;
    ws_[D_SYM] = a | 0105;
    return;

L0766:
    ws_[D_SYM] = m4_;
L0767:
    m6_ = (m6_ + 1) & R15;
    return;
L0777:
    ws_[D_SYM] = m4_;
    return;
L1012:
    ws_[D_SYM] = a;
    m4_        = a & R15;
    m6_        = (m6_ + 1) & R15;
    return;

L0743:
    ws_[D_KEYWORD] = a;

    // *1111B: a name of letters and digits, the first 6 characters count.
    ws_[D_NAME] = Word(m11) << 40;
    m10         = 077773;
L1113:
    c   = nextch();
    m11 = c;
    if (chclass(c) & 0100) {
        if (m10 == 0)
            goto L1113;
        int k = int(m10) - 0100000; // -5…-1
        ws_[D_NAME] |= Word(c) << (8 * (-k - 1));
        m10 = (m10 + 1) & R15;
        goto L1113;
    }
    a  = ws_[D_NAME];
    m8 = ((a >> 40) & 037) << 4 | ((a >> 32) & 017);
    for (;;) {
        Word w = ws_[(NAMES + m8) & R15];
        if (w == 0) {
            ws_[(NAMES + m8) & R15] = ws_[D_NAME];
            if (m8 == ws_[D_CONST])
                error(3);
            break;
        }
        if (w == ws_[D_NAME])
            break;
        m8 = (m8 + 1) & R15;
    }
    if (!ws_[D_KEYWORD]) {
        ws_[D_SYM] = m8 << 12;
        m4_        = 0;
        return;
    }

    // *1142B: a keyword in quotes.
    if (ws_[D_CHAR] != '\'') {
        m8 = 4;
        if (ws_[D_AFTEREND])
            goto L1475;
        error(4);
    }
    if (ws_[D_AFTEREND]) {
        Word w = ws_[(NAMES + m8 + 1) & R15];
        if ((w >> 40) != 0377)
            goto L1475;
        m4_ = w & R15;
        if (m4_ == 037) {
            ws_[D_WARNED] = 0;
            goto L0766;
        }
        if (m4_ == 031) {
            ws_[D_AFTEREND] = 0;
            goto L0766;
        }
        if (m4_ != 075)
            goto L1475;
        goto L0777;
    }
    {
        Word w = ws_[(NAMES + m8 + 1) & R15];
        if ((w >> 40) != 0377) {
            m6_ = (m6_ + 1) & R15;
            error(5);
        }
        m4_ = w & R15;
    }
    if (m4_ == 0377) {
        common();
        goto L1440;
    }
    if (m4_ == 0177)
        goto L1427;
    if (m4_ != 037)
        goto L0766;
    ws_[D_AFTEREND] = ws_[D_CHAR];
    goto L0766;

L1427: // 'COMMENT': after ';' or 'BEGIN' only; skipped up to ';' or '.,'
    if (ws_[D_SYM] != 051 && ws_[D_SYM] != 036) {
        m6_ = (m6_ + 1) & R15;
        error(6);
    }
    c = nextch();
L1433:
    if (c == ';')
        goto L1440;
    if (c != '.') {
        c = nextch();
        goto L1433;
    }
    c = nextch();
    if (c == ',')
        goto L1440;
    c = ws_[D_CHAR];
    goto L1433;
L1440:
    nextch();
    goto L0712;

L1461: // *1461B: the text after END, up to ';', END, ELSE or EOP
    switch ((chclass(ws_[D_CHAR]) & 01600) >> 7) {
    case 0: // ';'
        goto L1472;
    case 1: // '.'
        goto L1466;
    case 2:
        goto L1475;
    case 3: // quote: a keyword?
        c = nextch();
        if ((chclass(c) & 02000) >> 10)
            goto L1475;
        m11 = c;
        a   = ws_[D_CHAR];
        goto L0743;
    default: // letter or digit
        nextch();
        goto L1461;
    }
L1466:
    if (nextch() == ',')
        goto L1472;
L1475:
    if (!ws_[D_WARNED]) {
        if (!ws_[D_LIST] && ws_[D_LINE] != ws_[D_PRINTED]) {
            print_card();
            ws_[D_PRINTED] = ws_[D_LINE];
        }
        print_line("WARNING SYMBOL AFTER END");
        ws_[D_WARNED] = ws_[D_CHAR];
    }
    if (ws_[D_CHAR] == ';')
        goto L1472;
    if (ws_[D_CHAR] == '.')
        goto L1466;
    nextch();
    goto L1461;
L1472:
    ws_[D_AFTEREND] = 0;
    ws_[D_WARNED]   = 0;
    m4_             = 051;
    goto L0766;

L1015: // *1015B: a string; blanks are kept
    ws_[D_BLANK]  = a;
    ws_[D_BLANKS] = ONES;
    ws_[D_SYM]    = ws_[D_STRINGS];
L1020:
    ws_[D_NAME] = 0;
    m10         = 077773;
L1022:
    c = nextch();
    if (c != '\'') {
        if (c == 05) {
            ws_[D_BLANKS] = BLANKS;
            ws_[D_BLANK]  = 040;
            goto L1030;
        }
        goto L1035;
    }
    ws_[D_BLANKS] = BLANKS;
    ws_[D_BLANK]  = 040;
    c             = nextch();
    if (c == '\'')
        goto L1030;
    if (c == ')') {
        if (nextch() == '\'')
            goto L1030;
    }
    // *1046B: 'ooo', a character by its octal code
    {
        Word v = 0;
        for (int i = 0;; i++) {
            if (!(chclass(ws_[D_CHAR]) & 0100000))
                error(8);
            v = (v << 3) | (ws_[D_CHAR] & 7);
            if (i == 2) {
                if (v & 0400)
                    error(8);
                if (nextch() != '\'')
                    error(8);
                break;
            }
            if (nextch() == '\'')
                break;
        }
        ws_[D_CHAR]   = v;
        ws_[D_BLANKS] = ONES;
        ws_[D_BLANK]  = 0377;
    }
L1035: {
    int k = int(m10 ? m10 - 0100000 : 0); // -5…0
    ws_[D_NAME] |= ws_[D_CHAR] << (8 * -k);
    if (m10 != 0) {
        m10 = (m10 + 1) & R15;
        goto L1022;
    }
    store_string_word();
    goto L1020;
}
L1030:
    store_string_word();
    m4_ = 025;
    ws_[D_SYM] |= 025;
    goto L0767;

L1557: // *1556B: a number
    ws_[D_LOST]   = a;
    ws_[D_EXP]    = a;
    ws_[D_LAST]   = a;
    ws_[D_VALUE]  = a;
    ws_[D_EXPNEG] = a;
    m10           = 0;
L1562:
    switch ((chclass(ws_[D_CHAR]) & 014000) >> 11) {
    case 0: // end
        a = ws_[D_LAST];
        goto L1641;
    case 1: // exponent sign
        if (ws_[D_LAST])
            goto L16324;
        if (ws_[D_POINT]) {
            if (ws_[D_FRAC])
                error(9);
        }
        ws_[D_VALUE] = REAL1;
    L16324:
        if (ws_[D_EXP])
            error(9);
        ws_[D_POINT] = 0;
        ws_[D_EXP]   = ws_[D_CHAR];
        ws_[D_MANT]  = ws_[D_VALUE];
        ws_[D_VALUE] = 0;
        c            = nextch();
        switch ((chclass(c) & 060000) >> 13) {
        case 0:
            error(9);
        case 1: // '-'
            ws_[D_EXPNEG] = 1;
            nextch();
            ws_[D_LAST] = 0;
            break;
        case 2: // '+'
            ws_[D_EXPNEG] = 0;
            nextch();
            ws_[D_LAST] = 0;
            break;
        default: // digit
            ws_[D_EXPNEG] = 0;
            break;
        }
        ws_[D_LOST] = 0;
        goto L1562;
    case 2: // '.'
        c = nextch();
        if (c == ',') {
            ws_[D_SEMI] = ';';
            a           = ws_[D_LAST];
            goto L1641;
        }
        if (ws_[D_POINT])
            error(9);
        ws_[D_LAST]  = 0;
        ws_[D_POINT] = c;
        ws_[D_FRAC]  = c;
        if (ws_[D_EXP])
            error(9);
        goto L1562;
    default: // digit
        if (ws_[D_LOST]) {
            m10 = (m10 + 1) & R15;
        } else {
            Arith x(ws_[D_VALUE], rau_);
            x.mul(INT10);
            x.add((ws_[D_CHAR] & 017) | INT0);
            ws_[D_NEXT] = x.acc;
            x.sub(TWO40);
            if (!(x.acc & 0'0020'0000'0000'0000)) {
                // *1651B: beyond 2^40
                if (ws_[D_EXP])
                    error(9);
                ws_[D_LOST] = ws_[D_CHAR];
                m10         = (m10 + 1) & R15;
            } else {
                ws_[D_VALUE] = ws_[D_NEXT];
            }
        }
        if (ws_[D_POINT]) {
            m10         = (m10 - 1) & R15;
            ws_[D_FRAC] = 0;
        }
        ws_[D_LAST] = ws_[D_CHAR];
        nextch();
        goto L1562;
    }

L1641: // *1641B: the end of a number
    if (a == 0)
        error(9);
    m8 = ws_[D_CONST];
    if (ws_[D_EXP] || ws_[D_POINT]) {
        if (!ws_[D_EXP]) {
            ws_[D_MANT]  = ws_[D_VALUE];
            ws_[D_VALUE] = 0;
        }
        if (!ws_[D_MANT]) {
            a = 0;
        } else {
            // *1704B: scale the mantissa by the decimal exponent.
            rau_        = 2;
            unsigned m9 = to_integer(ws_[D_VALUE]) & R15;
            unsigned check; // the value *1740B tests
            if (m10 & 040000) {
                if (ws_[D_EXPNEG]) {
                    check = neg15(m10);
                    m10   = (check + m9) & R15;
                    goto L1740;
                }
                goto L1734;
            }
            if (ws_[D_EXPNEG]) {
                m9 = neg15(m9);
                goto L1734;
            }
            m10 = (m10 + m9) & R15;
            goto L1712;
        L1734:
            m10 = (m10 + m9) & R15;
            if (!(m10 & 040000))
                goto L1712;
            m10   = neg15(m10);
            check = m10;
        L1740:
            if (check & 077740) {
                a = 0;
                goto L1676;
            }
            a = ws_[D_MANT];
            while (m10) {
                Arith x(a, rau_);
                x.div(REAL10);
                a   = x.acc;
                m10 = (m10 - 1) & R15;
            }
            goto L1676;
        L1712:
            a = ws_[D_MANT];
            if (m10 == 0)
                goto L1676;
            if (m10 & 077740)
                error(11);
            m9         = (((ws_[D_MANT] << 1) & MASK48) >> 42) & R15;
            m9         = (m9 + 3 * m10) & R15;
            Word mant  = ws_[D_MANT] & 04017777777777777;
            ws_[D_KEY] = INT1;
            while (m10) {
                Arith x(C1_25, rau_);
                x.mul(ws_[D_KEY]);
                ws_[D_KEY] = x.acc;
                m10        = (m10 - 1) & R15;
            }
            Arith x(ws_[D_KEY], rau_);
            x.mul(mant);
            ws_[D_POWER] = x.acc;
            Word e       = ((x.acc << 1) & MASK48) >> 42;
            if ((m9 + e) & 077700)
                error(11);
            a = arx(Word(m9) << 41, ws_[D_POWER]);
        }
    L1676:
        a          = Word(intern(a, m8)) << 12;
        m4_        = 023;
        ws_[D_SYM] = a | 023;
        return;
    }
    if (ws_[D_LOST])
        error(10);
    a          = Word(intern(to_integer(ws_[D_VALUE]), m8)) << 12;
    rau_       = 2;
    m4_        = 024;
    ws_[D_SYM] = a | 024;
}

//
// *1155B: 'COMMON' /BLOCK/ NAME, NAME(D1,D2,D3), ...; read by the lexer
// itself. Fills the table at 27245, three words per entry: the name, then
// D-list pointer << 36 | slot << 24 and the size (for a block: its length).
//
namespace {
constexpr unsigned COMTAB = 027245;
constexpr Word REAL3      = 04114000000000000; // 3.0
constexpr Word C0_3       = 03751463146314631; // 0.3
constexpr Word C0_9       = 04016314631463146; // 0.9
constexpr Word D_MASK     = 07777000000000000;

// ASN with the address n.
Word asn(Word a, unsigned n)
{
    int k = int(n & 0177) - 64;
    if (k >= 48 || k <= -48)
        return 0;
    return k >= 0 ? a >> k : (a << -k) & MASK48;
}
} // namespace

Word &Pass1::cell(unsigned addr)
{
    if (addr < 016111 || addr > 016132)
        throw NotImplemented("'COMMON': store into the code");
    return cells_[addr - 016111];
}

// The next character at bit 40 - K of w (K in 16131), K += 8.
void Pass1::common_pack(Word &w, unsigned c)
{
    w |= asn(Word(c), 030 + (cell(016131) & R15));
    cell(016131) = arx(cell(016131), 010);
}

// *1366B: the first free name slot from the hash of the name, not used by
// an entry of the table.
unsigned Pass1::common_slot(Word name)
{
    unsigned m4 = ((name >> 40) & 037) << 4 | ((name >> 32) & 017);
    for (;;) {
        while (ws_[(NAMES + m4) & R15] != 0)
            m4 = (m4 + 1) & R15;
        cell(016111) = Word(m4) << 24;
        unsigned m11 = 0;
        for (;; m11 += 3) {
            if (ws_[(COMTAB + m11) & R15] == 0) {
                cell(016111) = 0;
                return m4;
            }
            if (((ws_[(COMTAB + 1 + m11) & R15] & 0000077770000000) ^ cell(016111)) == 0)
                break;
        }
        m4 = (m4 + 1) & R15;
    }
}

// *1330B: the message n, then ERROR 05.
void Pass1::common_error(unsigned n)
{
    Arith x(Word(n) | INT0, rau_);
    x.mul(REAL3);
    x.add(C0_3);
    rau_  = 3;
    x.rau = rau_;
    x.add(INT0);
    unsigned addr = 016045 + unsigned(x.acc & R15);
    Word words[3];
    for (unsigned i = 0; i < 3; i++)
        words[i] = tables::commsg.at(addr + i);
    print_line(rtrim(listing_text(words, 0, 18)));
    m6_ = (m6_ + 1) & R15;
    error(5);
}

void Pass1::common()
{
    unsigned c;
    do
        c = nextch();
    while (c == ' ');
    if (c != '/')
        common_error(0);

    // *1161B: the name of the block, up to 6 characters.
    while ((c = nextch()) != '/') {
        common_pack(ws_[(COMTAB + cell(016125)) & R15], ws_[D_CHAR]);
        if (cell(016131) == 070) {
            cell(016131) = 0;
            common_error(1);
        }
    }

    // *1174B
    unsigned idx            = cell(016125) & R15;
    unsigned m4             = common_slot(ws_[(COMTAB + idx) & R15]);
    ws_[(NAMES + m4) & R15] = ws_[(COMTAB + idx) & R15];
    dpush(Word(m4) | 06402000000000000);
    cell(016132) = Word(m3_) << 36;
    cell(016127) = cell(016125);
    cell(016125) = arx(cell(016125), 3);
    if (cell(016125) == 0377)
        common_error(2);

    // *1207B: a variable.
L1207:
    cell(016131) = 0;
    cell(016111) = 0;
L1211:
    nextch();
L1212:
    if (ws_[D_CHAR] == ',' || ws_[D_CHAR] == ';')
        goto L1230;
    if (ws_[D_CHAR] == '.') {
        if (nextch() != ',')
            common_error(3);
        ws_[D_CHAR] = ';';
        goto L1230;
    }
    common_pack(cell(016111), ws_[D_CHAR]);
    if (nextch() != '(')
        goto L1212;
L1230:
    ws_[(COMTAB + cell(016125)) & R15] = cell(016111);
    cell(016125)                       = arx(cell(016125), 3);
    if (cell(016125) == 0377)
        common_error(2);
    m4                            = common_slot(cell(016111));
    idx                           = cell(016125) & R15;
    ws_[(COMTAB - 2 + idx) & R15] = asn(Word(m4), 050) | cell(016132);
    ws_[(COMTAB - 1 + idx) & R15] = 1;
    cell(016132)                  = arx(cell(016132), D_MASK) & D_MASK;
    cell(016131)                  = 0;
    cell(016127)                  = arx(cell(016127), Word(1) << 24);
    switch (ws_[D_CHAR]) {
    case '(':
        break;
    case ',':
        goto L1211;
    case ';':
        goto L1412;
    default:
        common_error(3);
    }

    // *1255B: the bounds, decimal digits; the size is their product.
    cell(016111) = cell(016112) = cell(016113) = REAL1;
    do {
        cell(016130) = 0;
        while ((c = nextch()) != ',' && c != ')')
            cell(016130) = asn((ws_[D_CHAR] & 017) | cell(016130), 074);
        cell(016126) = REAL1;
        cell(016124) = 0;
        for (;;) {
            Word a = asn(cell(016130), 0104);
            if (a == 0)
                break;
            cell(016130) = a;
            Arith x((a & 017) | INT0, rau_);
            x.mul(cell(016126));
            x.add(cell(016124));
            cell(016124) = x.acc;
            Arith p(cell(016126), rau_);
            p.mul(REAL10);
            cell(016126) = p.acc;
        }
        cell(016111 + (cell(016115) & R15)) = cell(016124);
        cell(016115)                        = arx(cell(016115), 1);
    } while (ws_[D_CHAR] != ')');
    {
        Arith x(cell(016111), rau_);
        x.mul(cell(016112));
        x.mul(cell(016113));
        x.sub(C0_9);
        rau_  = 3;
        x.rau = rau_;
        x.add(INT0);
        rau_         = 6;
        cell(016124) = x.acc;
        Word &size   = ws_[(COMTAB - 1 + idx) & R15];
        size         = arx(x.acc & R15, size);
        cell(016127) = arx(asn(cell(016124), 050), cell(016127));
    }
    cell(016131) = 0;
    cell(016115) = 0;
    c            = nextch();
    if (c == ',')
        goto L1207;
    if (c != ';') {
        if (c != '.')
            common_error(5);
        if (nextch() != ',')
            common_error(6);
    }

    // *1412B: the length of the block.
L1412:
    cell(016131)                                   = 0;
    ws_[(COMTAB + 1 + (cell(016127) & R15)) & R15] = asn(cell(016127), 0130);
    cell(016115)                                   = 0;
    cell(016127)                                   = 0;
}
