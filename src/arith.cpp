//
// Ported from dubna/arithmetic.cpp:
// Copyright (c) 2022-2023 Leonid Broukhis, Serge Vakulenko. MIT license.
//
#include "arith.h"

namespace {

constexpr Word BIT41  = 0'0020'0000'0000'0000;
constexpr Word BIT42  = 0'0040'0000'0000'0000;
constexpr Word BITS40 = 0'0017'7777'7777'7777;
constexpr Word BITS41 = 0'0037'7777'7777'7777;
constexpr Word BITS42 = 0'0077'7777'7777'7777;

unsigned highest_bit(Word val)
{
    int n = 32, cnt = 0;
    do {
        Word tmp = val;
        if (tmp >>= n) {
            cnt += n;
            val = tmp;
        }
    } while (n >>= 1);
    return 48 - cnt;
}

struct ME {
    int64_t mantissa{};
    unsigned exponent{};

    ME() = default;
    explicit ME(Word val)
    {
        exponent = (val >> 41) & 0177;
        mantissa = val & BITS41;
        mantissa <<= 64 - 41;
        mantissa >>= 64 - 41;
    }
    bool is_negative() const { return (mantissa & BIT42) != 0; }
    bool is_denormal() const { return ((mantissa >> 40) ^ (mantissa >> 41)) & 1; }
    void negate() { mantissa = -mantissa; }
    void normalize_to_the_right()
    {
        mantissa >>= 1;
        ++exponent;
    }

    // Multiply by a signed 41-bit integer: upper 41 bits to the mantissa,
    // the lower 40 bits returned.
    uint64_t multiply(int64_t x)
    {
        unsigned negative = 0;
        if (mantissa < 0) {
            mantissa = -mantissa;
            negative ^= 1;
        }
        if (x < 0) {
            x = -x;
            negative ^= 1;
        }
        uint64_t a  = x >> 20;
        uint64_t b  = x & 0xfffff;
        uint64_t mr = mantissa * b;
        mantissa *= a;
        mr += (mantissa & 0xfffff) << 20;
        mantissa >>= 20;
        mantissa += mr >> 40;
        mr &= 0xfffff'fffff;
        if (negative) {
            mantissa = ~mantissa;
            mr ^= 0xfffff'fffff;
            mr += 1;
            mantissa += mr >> 40;
            mr &= 0xfffff'fffff;
        }
        return mr;
    }
};

inline int dbit(uint64_t x, int n)
{
    return (x >> (n - 1)) & 1;
}

// Division of mantissas as the arithmetic unit does it, see dubna.
uint64_t au_divide(uint64_t x, uint64_t y)
{
    const uint64_t lo   = BITS40 >> 2;
    const uint64_t lo39 = BITS40 >> 1;
    const int yneg      = dbit(y, 41);

    uint64_t s2  = x & lo39;
    uint64_t c2  = 0;
    int sud      = dbit(x, 42) << 2 | dbit(x, 41) << 1 | dbit(x, 40);
    int pud      = 0;
    int rud      = dbit(x, 41) << 1 | dbit(x, 40);
    bool zu_plus = !dbit(x, 41) && yneg;
    uint64_t pos = 0, neg = 0;

    for (int i = 0; i < 42; i++) {
        char act;
        if (!(sud & 4))
            act = zu_plus ? '+' : '-';
        else if ((sud & 3) + pud == 3)
            act = 's';
        else
            act = (((sud & 3) + pud >= 4) != yneg) ? '-' : '+';
        zu_plus = (act == '+');
        pos     = pos << 1 | (act == '-');
        neg     = neg << 1 | (act == '+');

        uint64_t lo_s, lo_c;
        if (act == 's') {
            int a = dbit(s2, 39);
            int b = dbit(c2, 39);
            sud   = 6 | (a ^ b);
            pud   = (a & b) << 1;
            a     = dbit(s2, 38);
            b     = dbit(c2, 38);
            pud |= a & b;
            lo_s = (s2 & (lo >> 1)) | (uint64_t)(a ^ b) << 37;
            lo_c = c2 & (lo >> 1);
        } else {
            uint64_t yv = (act == '+') ? y : ~y & BITS42;
            uint64_t sm = s2 ^ c2 ^ (yv & lo39);
            uint64_t cr = ((s2 & c2) | (s2 & yv & lo39) | (c2 & yv & lo39)) << 1;
            if (act == '-')
                cr |= 1;
            int t39 = rud & 1;
            int t40 = rud >> 1;
            sud     = (t40 ^ dbit(yv, 41) ^ (t39 & dbit(yv, 40))) << 2 | (t39 ^ dbit(yv, 40)) << 1 |
                  dbit(sm, 39);
            pud  = dbit(cr, 40) << 1 | dbit(cr, 39);
            lo_s = sm & lo;
            lo_c = cr & lo;
        }
        rud = ((sud & 3) + pud) & 3;
        s2  = lo_s << 1;
        c2  = lo_c << 1;
    }

    int pe = pos & 1;
    int ne = neg & 1;
    pos >>= 1;
    neg >>= 1;
    int p1  = pos & 1;
    int n1  = neg & 1;
    int dpr = pe || (!ne && !pe) || (!p1 && ne && !n1);
    int r1  = pe && (n1 || p1);
    return (pos + (~neg & BITS42) + dpr + r1) & BITS42;
}

} // namespace

void Arith::add(Word val, bool negate_acc, bool negate_val)
{
    ME a(acc);
    ME word(val);

    if (!negate_acc) {
        if (negate_val)
            word.negate();
    } else if (!negate_val) {
        a.negate();
    } else {
        if (a.is_negative())
            a.negate();
        if (!word.is_negative())
            word.negate();
    }

    ME a1, a2;
    int diff = a.exponent - word.exponent;
    if (diff < 0) {
        diff = -diff;
        a1   = a;
        a2   = word;
    } else {
        a1 = word;
        a2 = a;
    }

    Word mr         = 0;
    bool neg        = a1.is_negative();
    bool round_flag = false;
    if (diff == 0) {
    } else if (diff <= 40) {
        mr          = (a1.mantissa << (40 - diff)) & BITS40;
        round_flag  = (mr != 0);
        a1.mantissa = ((a1.mantissa >> diff) | (neg ? (~0ull << (40 - diff)) : 0)) & BITS42;
    } else if (diff <= 80) {
        diff -= 40;
        round_flag  = (a1.mantissa != 0);
        mr          = ((a1.mantissa >> diff) | (neg ? (~0ull << (40 - diff)) : 0)) & BITS40;
        a1.mantissa = neg ? BITS42 : 0;
    } else {
        round_flag = (a1.mantissa != 0);
        if (neg) {
            mr          = BITS40;
            a1.mantissa = BITS42;
        } else {
            mr = a1.mantissa = 0;
        }
    }
    ME r;
    r.exponent = a2.exponent;
    r.mantissa = a1.mantissa + a2.mantissa;
    if (r.is_denormal()) {
        round_flag = round_flag || (r.mantissa & 1);
        mr         = (mr >> 1) | ((r.mantissa & 1) << 39);
        r.normalize_to_the_right();
    }
    normalize_and_round({ r.mantissa, r.exponent }, mr, round_flag);
}

void Arith::normalize_and_round(Value v, Word mr, bool round_flag)
{
    ME a;
    a.mantissa = v.mantissa;
    a.exponent = v.exponent;
    Word rr    = 0;
    Word r;

    if (rau & NORM_DISABLE)
        goto chk_rnd;

    switch ((a.mantissa >> 39) & 3) {
    case 0:
        r = a.mantissa & BITS40;
        if (r) {
            int cnt = highest_bit(r) - 9;
            r <<= cnt;
            rr         = mr >> (40 - cnt);
            a.mantissa = r | rr;
            mr <<= cnt;
            a.exponent -= cnt;
            goto chk_zero;
        }
        r = mr & BITS40;
        if (r) {
            int cnt = highest_bit(r) - 9;
            rr      = mr;
            r <<= cnt;
            a.mantissa = r;
            mr         = 0;
            a.exponent -= 40 + cnt;
            goto chk_zero;
        }
        goto zero;
    case 3:
        r = ~a.mantissa & BITS40;
        if (r) {
            int cnt    = highest_bit(r) - 9;
            r          = (r << cnt) | ((1LL << cnt) - 1);
            rr         = mr >> (40 - cnt);
            a.mantissa = BIT41 | (~r & BITS40) | rr;
            mr <<= cnt;
            a.exponent -= cnt;
            goto chk_zero;
        }
        r = ~mr & BITS40;
        if (r) {
            int cnt    = highest_bit(r) - 9;
            rr         = mr;
            r          = (r << cnt) | ((1LL << cnt) - 1);
            a.mantissa = BIT41 | (~r & BITS40);
            mr         = 0;
            a.exponent -= 40 + cnt;
            goto chk_zero;
        }
        rr         = 1;
        a.mantissa = BIT41;
        mr         = 0;
        a.exponent -= 80;
        goto chk_zero;
    }
chk_zero:
    if (rr)
        round_flag = false;

chk_rnd:
    if (a.exponent & 0x8000)
        goto zero;

    if (!(rau & ROUND_DISABLE) && round_flag)
        a.mantissa |= 1;

    if (!a.mantissa && !(rau & NORM_DISABLE)) {
    zero:
        acc = 0;
        rmr &= ~BITS40;
        return;
    }

    acc = (Word)(a.exponent & 0177) << 41 | (a.mantissa & BITS41);
    rmr = mr & BITS40;
    if ((a.exponent & 0x80) && !(rau & OVF_DISABLE))
        throw ArithOverflow();
}

void Arith::add_exponent(int n)
{
    ME a(acc);
    a.exponent += n;
    rmr = 0;
    normalize_and_round({ a.mantissa, a.exponent }, 0, false);
}

void Arith::change_sign(bool negate)
{
    ME a(acc);
    if (negate) {
        a.negate();
        if (a.is_denormal())
            a.normalize_to_the_right();
    }
    rmr = 0;
    normalize_and_round({ a.mantissa, a.exponent }, 0, false);
}

void Arith::mul(Word val)
{
    if (!acc || !val) {
        acc = 0;
        rmr &= ~BITS40;
        return;
    }
    ME a(acc);
    ME word(val);
    Word mr = a.multiply(word.mantissa);
    a.exponent += word.exponent - 64;
    if (a.is_denormal())
        a.normalize_to_the_right();
    normalize_and_round({ a.mantissa, a.exponent }, mr, mr != 0);
}

void Arith::div(Word val)
{
    if (((val ^ (val << 1)) & BIT41) == 0)
        throw std::domain_error("division by zero");
    ME dividend(acc);
    ME divisor(val);
    ME a;
    a.mantissa = au_divide(dividend.mantissa & BITS42, divisor.mantissa & BITS42);
    a.exponent = dividend.exponent - divisor.exponent + 64;
    a.mantissa <<= 64 - 42;
    a.mantissa >>= 64 - 42;
    if (a.is_denormal())
        a.normalize_to_the_right();
    normalize_and_round({ a.mantissa, a.exponent }, 0, false);
}

constexpr Word PACK_BIT48 = 0'4000'0000'0000'0000;

Word pack_bits(Word val, Word mask)
{
    Word result = 0;
    for (; mask; mask >>= 1, val >>= 1)
        if (mask & 1) {
            result >>= 1;
            if (val & 1)
                result |= PACK_BIT48;
        }
    return result;
}

Word unpack_bits(Word val, Word mask)
{
    Word result = 0;
    for (unsigned i = 0; i < 48; ++i) {
        result <<= 1;
        if (mask & PACK_BIT48) {
            if (val & PACK_BIT48)
                result |= 1;
            val <<= 1;
        }
        mask <<= 1;
    }
    return result & WORD_MASK;
}
