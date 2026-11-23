//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// Lexer of pass 1: AG15414 and the character reader AG16225, translated label
// by label. Labels *nnnnB are offsets in os-dubna/algol/ag14504.asm.
//
#include <cstdio>
#include <ostream>

#include "listing.h"
#include "pass1.h"

namespace {

// The class of each character (16514…): the lexer's dispatch, the code of a
// one-character symbol, and where the character goes in each context.
struct CharClass {
    unsigned dispatch;  // 0 letter, 1 quote, 5 digit, 9 one-character symbol, …
    unsigned code;      // the symbol of a one-character symbol
    unsigned quoted;    // after a quote: 0 '', 1 a keyword, 2 a letter, 3 a digit
    bool name;          // a letter or digit in a name
    unsigned after_end; // in the text after END: 0 ';', 1 '.', 2 other, 3 quote, 4 letter or digit
    bool not_letter;    // after a quote in the text after END: no keyword
    unsigned number;    // in a number: 0 end, 1 exponent sign, 2 '.', 3 digit
    unsigned exponent;  // after the exponent sign: 0 error, 1 '-', 2 '+', 3 digit
    bool octal;         // an octal digit
};

// clang-format off
const CharClass CHAR_CLASSES[0200] = {
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 000
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 001
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 002
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 003
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 004 !
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 005 Ъ
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 006 ×
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 007 ×
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 010 !
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 011 !
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 012
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 013 !
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 014 !
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 015
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 016 ⩽
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 017 ⩾
    {  9, 0002, 1, 0, 2, 1, 0, 0, 0 }, // 020 ‘
    {  9, 0003, 1, 0, 2, 1, 0, 0, 0 }, // 021
    {  9, 0001, 1, 0, 2, 1, 0, 0, 0 }, // 022
    {  9, 0004, 1, 0, 2, 1, 0, 0, 0 }, // 023
    {  9, 0043, 1, 0, 2, 1, 0, 0, 0 }, // 024
    {  9, 0044, 1, 0, 2, 1, 0, 0, 0 }, // 025 ―
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 026 ↑
    {  5, 0000, 1, 0, 2, 1, 1, 0, 0 }, // 027 ⏨
    {  9, 0012, 1, 0, 2, 1, 0, 0, 0 }, // 030 ≠
    {  9, 0006, 1, 0, 2, 1, 0, 0, 0 }, // 031 °
    {  9, 0007, 1, 0, 2, 1, 0, 0, 0 }, // 032 ÷
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 033 ’
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 034 ⊃
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 035 ≡
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 036 ∨
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 037 ¬
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 040 ' '
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 041 !
    {  8, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 042 "
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 043 ≠
    {  5, 0000, 1, 0, 2, 1, 1, 0, 0 }, // 044 ◇
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 045 %
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 046 ∧
    {  1, 0000, 0, 0, 3, 1, 0, 0, 0 }, // 047 ′
    {  6, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 050 (
    {  9, 0045, 1, 0, 2, 1, 0, 0, 0 }, // 051 )
    {  4, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 052 *
    {  9, 0013, 1, 0, 2, 1, 0, 2, 0 }, // 053 +
    { 11, 0047, 1, 0, 2, 1, 0, 0, 0 }, // 054 ,
    {  9, 0014, 1, 0, 2, 1, 0, 1, 0 }, // 055 -
    {  2, 0000, 1, 0, 1, 1, 2, 0, 0 }, // 056 .
    {  7, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 057 /
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 060 0
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 061 1
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 062 2
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 063 3
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 064 4
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 065 5
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 066 6
    {  5, 0000, 3, 1, 4, 1, 3, 3, 1 }, // 067 7
    {  5, 0000, 3, 1, 4, 1, 3, 3, 0 }, // 070 8
    {  5, 0000, 3, 1, 4, 1, 3, 3, 0 }, // 071 9
    {  3, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 072 :
    {  9, 0051, 1, 0, 0, 1, 0, 0, 0 }, // 073 ;
    {  9, 0005, 1, 0, 2, 1, 0, 0, 0 }, // 074 <
    {  9, 0011, 1, 0, 2, 1, 0, 0, 0 }, // 075 =
    {  9, 0010, 1, 0, 2, 1, 0, 0, 0 }, // 076 >
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 077 ↑
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 100 °
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 101 A
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 102 B
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 103 C
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 104 D
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 105 E
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 106 F
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 107 G
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 110 H
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 111 I
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 112 J
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 113 K
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 114 L
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 115 M
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 116 N
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 117 O
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 120 P
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 121 Q
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 122 R
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 123 S
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 124 T
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 125 U
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 126 V
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 127 W
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 130 X
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 131 Y
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 132 Z
    {  9, 0021, 1, 0, 2, 1, 0, 0, 0 }, // 133 [
    {  9, 0015, 1, 0, 2, 1, 0, 0, 0 }, // 134 ‾
    {  9, 0046, 1, 0, 2, 1, 0, 0, 0 }, // 135 ]
    {  9, 0017, 1, 0, 2, 1, 0, 0, 0 }, // 136 |
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 137 _
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 140 Ю
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 141 A
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 142 Б
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 143 Ц
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 144 Д
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 145 E
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 146 Ф
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 147 Г
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 150 X
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 151 И
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 152 Й
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 153 K
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 154 Л
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 155 M
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 156 H
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 157 O
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 160 П
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 161 Я
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 162 P
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 163 C
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 164 T
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 165 Y
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 166 Ж
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 167 B
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 170 Ь
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 171 Ы
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 172 З
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 173 Ш
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 174 Э
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 175 Щ
    {  0, 0000, 2, 1, 4, 0, 0, 0, 0 }, // 176 Ч
    { 10, 0000, 1, 0, 2, 1, 0, 0, 0 }, // 177
};
// clang-format on

const CharClass &chclass(unsigned c)
{
    return CHAR_CLASSES[c & 0177];
}

} // namespace

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
    return chclass(like).code;
}

void Pass1::error(unsigned n)
{
    sym_ = 0;
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
    line_       = arx(line_, 1);
    card_count_ = arx(card_count_, 1) | INT0;
    if (next_card_ >= cards_.size() || cards_[next_card_][0] == 01244251621020106) // *END F
        throw NoEop{};
    read_card();
    if (list_)
        print_card();
    m5_ = 077765;
    m6_ = 077772;
L1545:
    word_ = card_.at((m5_ + 013) & R15);
    if (word_ == blanks_)
        goto L1523;
L1547: {
    int pos    = int(m6_) - 0100000; // -6…-1
    unsigned c = (word_ >> (8 * (-pos - 1))) & 0377;
    char_      = c;
    if (c == blank_) {
        m6_ = (m6_ + 1) & R15;
        goto L1522;
    }
    return c;
}
}

Pass1::Pass1(Context &ctx)
    : ctx_(ctx), tables_(ctx.tables), strings_(ctx.strings), cards_(read_cards(ctx.source))
{
    // From the start of AG13666.
    list_   = ctx.options.list ? 1 : 0;
    pool_   = 04420276740004000;
    blanks_ = BLANKS;
}

void Pass1::read_card()
{
    const Card &card = cards_[next_card_++];
    for (unsigned i = 0; i < CARD_WORDS; i++)
        card_[i] = card[i];
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
        card[i] = card_[i];
    char num[16];
    std::snprintf(num, sizeof(num), "%6u.     ", unsigned(line_ & 077777777));
    print_line(num + listing_text(card, 0, 80));
}

// G27220: put a word into the string pool; a full sector of 256 words goes
// to the string pool on drum.
void Pass1::pool_put(Word w)
{
    pool_page_.at((pool_ & R15) - 04000) = w;
    pool_                                = arx(pool_, 1);
    if ((pool_ << 40) & MASK48)
        return;
    pool_ = arx(pool_, 037777400);
    for (unsigned i = 0; i < 256; i++)
        strings_.push_back(pool_page_[i]);
}

// *1102B: a word of a string to the pool.
void Pass1::store_string_word()
{
    pool_end_   = arx(pool_end_, 010000);
    unsigned m9 = ((pool_end_ >> 12) - 01634 - 06000) & R15;
    if (m9 == 0)
        error(12);
    pool_put(name_);
}

//
// *1660B: put a constant into the table, or find an equal one; returns its slot.
// The slots go down from D_CONST; the presets are at the top.
//
unsigned Pass1::intern(Word value, unsigned slot)
{
    key_               = value;
    tables_.name(slot) = value;
    unsigned m14       = 01633;
    while (tables_.name((m14)&R15) != key_)
        m14 = (m14 - 1) & R15;
    if (m14 != tables_.free_const)
        return m14;
    slot               = (slot - 1) & R15;
    tables_.free_const = slot;
    if (tables_.name((slot)&R15))
        error(7);
    return slot + 1;
}

void Pass1::lex()
{
    Word a;
    unsigned c, m8 = 0, m10 = 0, m11 = 0;
    Word stack = 0; // the one word the octal escapes keep on the stack

    c = nextch(false);
    if (afterend_)
        goto L1461;
L0712:
    m11 = char_;
    switch (chclass(m11).dispatch) {
    case 0: // letter
        a = 0;
        goto L0743;
    case 1: // quote
        c   = nextch();
        m11 = c;
        switch (chclass(c).quoted) {
        case 0: // '': a string
            a = tables_.free_const;
            goto L1015;
        case 1: // '(' ')' string, or an error
            if (c != '(')
                error(2);
            c = nextch();
            if (c != '\'')
                error(2);
            a = tables_.free_const;
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
        point_ = c ^ ',';
        frac_  = c ^ ',';
        a      = 0;
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
        a      = 0;
        point_ = 0;
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
        a = chclass(m11).code;
        goto L1012;
    case 012: // ERROR 01, except for the Unicode operators (an extension)
        if (unsigned op = unicode_operator(m11)) {
            a = op;
            goto L1012;
        }
        error(1);
    case 013: // ',', or ';' after '.,' at the end of a number
        if (!semi_) {
            a = 047;
            goto L1012;
        }
        m4_   = 051;
        semi_ = 0;
        goto L0766;
    default:
        error(1);
    }

L0746: // '<octal digits>'
    m10 = (m10 - 16) & R15;
    if (m10 == 0)
        error(14);
    m10 = (m10 + 021) & R15;
    if (!chclass(char_).octal)
        error(15);
    stack = ((stack << 3) & MASK48) | (char_ & 7);
    if (nextch() != '\'')
        goto L0746;
    nextch();
    m8   = tables_.free_const;
    a    = Word(intern(stack, m8)) << 12;
    m4_  = 0105;
    sym_ = a | 0105;
    return;

L0766:
    sym_ = m4_;
L0767:
    m6_ = (m6_ + 1) & R15;
    return;
L0777:
    sym_ = m4_;
    return;
L1012:
    sym_ = a;
    m4_  = a & R15;
    m6_  = (m6_ + 1) & R15;
    return;

L0743:
    keyword_ = a;

    // *1111B: a name of letters and digits, the first 6 characters count.
    name_ = Word(m11) << 40;
    m10   = 077773;
L1113:
    c   = nextch();
    m11 = c;
    if (chclass(c).name) {
        if (m10 == 0)
            goto L1113;
        int k = int(m10) - 0100000; // -5…-1
        name_ |= Word(c) << (8 * (-k - 1));
        m10 = (m10 + 1) & R15;
        goto L1113;
    }
    a  = name_;
    m8 = ((a >> 40) & 037) << 4 | ((a >> 32) & 017);
    for (;;) {
        Word w = tables_.name((m8)&R15);
        if (w == 0) {
            tables_.name((m8)&R15) = name_;
            if (m8 == tables_.free_const)
                error(3);
            break;
        }
        if (w == name_)
            break;
        m8 = (m8 + 1) & R15;
    }
    if (!keyword_) {
        sym_ = Word(m8) << 12;
        m4_  = 0;
        return;
    }

    // *1142B: a keyword in quotes.
    if (char_ != '\'') {
        m8 = 4;
        if (afterend_)
            goto L1475;
        error(4);
    }
    if (afterend_) {
        Word w = tables_.name((m8 + 1) & R15);
        if ((w >> 40) != 0377)
            goto L1475;
        m4_ = w & R15;
        if (m4_ == 037) {
            warned_ = 0;
            goto L0766;
        }
        if (m4_ == 031) {
            afterend_ = 0;
            goto L0766;
        }
        if (m4_ != 075)
            goto L1475;
        goto L0777;
    }
    {
        Word w = tables_.name((m8 + 1) & R15);
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
    afterend_ = char_;
    goto L0766;

L1427: // 'COMMENT': after ';' or 'BEGIN' only; skipped up to ';' or '.,'
    if (sym_ != 051 && sym_ != 036) {
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
    c = char_;
    goto L1433;
L1440:
    nextch();
    goto L0712;

L1461: // *1461B: the text after END, up to ';', END, ELSE or EOP
    switch (chclass(char_).after_end) {
    case 0: // ';'
        goto L1472;
    case 1: // '.'
        goto L1466;
    case 2:
        goto L1475;
    case 3: // quote: a keyword?
        c = nextch();
        if (chclass(c).not_letter)
            goto L1475;
        m11 = c;
        a   = char_;
        goto L0743;
    default: // letter or digit
        nextch();
        goto L1461;
    }
L1466:
    if (nextch() == ',')
        goto L1472;
L1475:
    if (!warned_) {
        if (!list_ && line_ != printed_) {
            print_card();
            printed_ = line_;
        }
        print_line("WARNING SYMBOL AFTER END");
        warned_ = char_;
    }
    if (char_ == ';')
        goto L1472;
    if (char_ == '.')
        goto L1466;
    nextch();
    goto L1461;
L1472:
    afterend_ = 0;
    warned_   = 0;
    m4_       = 051;
    goto L0766;

L1015: // *1015B: a string; blanks are kept
    blank_  = a;
    blanks_ = ONES;
    sym_    = pool_end_;
L1020:
    name_ = 0;
    m10   = 077773;
L1022:
    c = nextch();
    if (c != '\'') {
        if (c == 05) {
            blanks_ = BLANKS;
            blank_  = 040;
            goto L1030;
        }
        goto L1035;
    }
    blanks_ = BLANKS;
    blank_  = 040;
    c       = nextch();
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
            if (!chclass(char_).octal)
                error(8);
            v = (v << 3) | (char_ & 7);
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
        char_   = v;
        blanks_ = ONES;
        blank_  = 0377;
    }
L1035: {
    int k = int(m10 ? m10 - 0100000 : 0); // -5…0
    name_ |= char_ << (8 * -k);
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
    sym_ |= 025;
    goto L0767;

L1557: // *1556B: a number
    lost_   = a;
    exp_    = a;
    last_   = a;
    value_  = a;
    expneg_ = a;
    m10     = 0;
L1562:
    switch (chclass(char_).number) {
    case 0: // end
        a = last_;
        goto L1641;
    case 1: // exponent sign
        if (last_)
            goto L16324;
        if (point_) {
            if (frac_)
                error(9);
        }
        value_ = REAL1;
    L16324:
        if (exp_)
            error(9);
        point_ = 0;
        exp_   = char_;
        mant_  = value_;
        value_ = 0;
        c      = nextch();
        switch (chclass(c).exponent) {
        case 0:
            error(9);
        case 1: // '-'
            expneg_ = 1;
            nextch();
            last_ = 0;
            break;
        case 2: // '+'
            expneg_ = 0;
            nextch();
            last_ = 0;
            break;
        default: // digit
            expneg_ = 0;
            break;
        }
        lost_ = 0;
        goto L1562;
    case 2: // '.'
        c = nextch();
        if (c == ',') {
            semi_ = ';';
            a     = last_;
            goto L1641;
        }
        if (point_)
            error(9);
        last_  = 0;
        point_ = c;
        frac_  = c;
        if (exp_)
            error(9);
        goto L1562;
    default: // digit
        if (lost_) {
            m10 = (m10 + 1) & R15;
        } else {
            Arith x(value_, rau_);
            x.mul(INT10);
            x.add((char_ & 017) | INT0);
            next_ = x.acc;
            x.sub(TWO40);
            if (!(x.acc & 0'0020'0000'0000'0000)) {
                // *1651B: beyond 2^40
                if (exp_)
                    error(9);
                lost_ = char_;
                m10   = (m10 + 1) & R15;
            } else {
                value_ = next_;
            }
        }
        if (point_) {
            m10   = (m10 - 1) & R15;
            frac_ = 0;
        }
        last_ = char_;
        nextch();
        goto L1562;
    }

L1641: // *1641B: the end of a number
    if (a == 0)
        error(9);
    m8 = tables_.free_const;
    if (exp_ || point_) {
        if (!exp_) {
            mant_  = value_;
            value_ = 0;
        }
        if (!mant_) {
            a = 0;
        } else {
            // *1704B: scale the mantissa by the decimal exponent.
            rau_        = 2;
            unsigned m9 = to_integer(value_) & R15;
            unsigned check; // the value *1740B tests
            if (m10 & 040000) {
                if (expneg_) {
                    check = neg15(m10);
                    m10   = (check + m9) & R15;
                    goto L1740;
                }
                goto L1734;
            }
            if (expneg_) {
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
            a = mant_;
            while (m10) {
                Arith x(a, rau_);
                x.div(REAL10);
                a   = x.acc;
                m10 = (m10 - 1) & R15;
            }
            goto L1676;
        L1712:
            a = mant_;
            if (m10 == 0)
                goto L1676;
            if (m10 & 077740)
                error(11);
            m9        = (((mant_ << 1) & MASK48) >> 42) & R15;
            m9        = (m9 + 3 * m10) & R15;
            Word mant = mant_ & 04017777777777777;
            key_      = INT1;
            while (m10) {
                Arith x(C1_25, rau_);
                x.mul(key_);
                key_ = x.acc;
                m10  = (m10 - 1) & R15;
            }
            Arith x(key_, rau_);
            x.mul(mant);
            power_ = x.acc;
            Word e = ((x.acc << 1) & MASK48) >> 42;
            if ((m9 + e) & 077700)
                error(11);
            a = arx(Word(m9) << 41, power_);
        }
    L1676:
        a    = Word(intern(a, m8)) << 12;
        m4_  = 023;
        sym_ = a | 023;
        return;
    }
    if (lost_)
        error(10);
    a    = Word(intern(to_integer(value_), m8)) << 12;
    rau_ = 2;
    m4_  = 024;
    sym_ = a | 024;
}

//
// *1155B: 'COMMON' /BLOCK/ NAME, NAME(D1,D2,D3), ...; read by the lexer
// itself. Fills the table at 27245, three words per entry: the name, then
// D-list pointer << 36 | slot << 24 and the size (for a block: its length).
//
namespace {
constexpr unsigned COMTAB = 027245;
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
        while (tables_.name((m4)&R15) != 0)
            m4 = (m4 + 1) & R15;
        cell(016111) = Word(m4) << 24;
        unsigned m11 = 0;
        for (;; m11 += 3) {
            if (tables_.common((m11)&R15) == 0) {
                cell(016111) = 0;
                return m4;
            }
            if (((tables_.common((1 + m11) & R15) & 0000077770000000) ^ cell(016111)) == 0)
                break;
        }
        m4 = (m4 + 1) & R15;
    }
}

// The 'COMMON' error messages (16045…).
const char *const COMMON_ERRORS[] = {
    "HET KOCOЙ ЧEPTЫ", "ИMЯ COMMON > 6 CИM", "COMMON ЭЛ-TOB>ДOП", "ПЛOXOЙ CИMBOЛ",
    "ИMЯ ЭЛ-TA >6-TИ", "HET * , *ИЛИ* ; *",  "ЗAЧEM ЗДECЬ * . *",
};

// *1330B: the message n, then ERROR 05. (The original finds it at 3n in
// floating point, leaving the mode 3.)
void Pass1::common_error(unsigned n)
{
    rau_ = 3;
    print_line(COMMON_ERRORS[n]);
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
        common_pack(tables_.common((cell(016125)) & R15), char_);
        if (cell(016131) == 070) {
            cell(016131) = 0;
            common_error(1);
        }
    }

    // *1174B
    unsigned idx           = cell(016125) & R15;
    unsigned m4            = common_slot(tables_.common((idx)&R15));
    tables_.name((m4)&R15) = tables_.common((idx)&R15);
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
    if (char_ == ',' || char_ == ';')
        goto L1230;
    if (char_ == '.') {
        if (nextch() != ',')
            common_error(3);
        char_ = ';';
        goto L1230;
    }
    common_pack(cell(016111), char_);
    if (nextch() != '(')
        goto L1212;
L1230:
    tables_.common((cell(016125)) & R15) = cell(016111);
    cell(016125)                         = arx(cell(016125), 3);
    if (cell(016125) == 0377)
        common_error(2);
    m4                              = common_slot(cell(016111));
    idx                             = cell(016125) & R15;
    tables_.common((idx - 2) & R15) = asn(Word(m4), 050) | cell(016132);
    tables_.common((idx - 1) & R15) = 1;
    cell(016132)                    = arx(cell(016132), D_MASK) & D_MASK;
    cell(016131)                    = 0;
    cell(016127)                    = arx(cell(016127), Word(1) << 24);
    switch (char_) {
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
            cell(016130) = asn((char_ & 017) | cell(016130), 074);
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
    } while (char_ != ')');
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
        Word &size   = tables_.common((idx - 1) & R15);
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
    cell(016131)                                     = 0;
    tables_.common((1 + (cell(016127) & R15)) & R15) = asn(cell(016127), 0130);
    cell(016115)                                     = 0;
    cell(016127)                                     = 0;
}
