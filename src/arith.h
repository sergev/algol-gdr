#ifndef ALGOL_ARITH_H
#define ALGOL_ARITH_H

#include <stdexcept>

#include "stdarray.h"

//
// BESM-6 arithmetic unit: floating point as the original compiler computes
// its constants. Ported from dubna/arithmetic.cpp (Leonid Broukhis,
// Serge Vakulenko, MIT license).
//
// A word: exponent in bits 48-42 (offset 64), mantissa in bits 41-1, two's
// complement. The mode bits are those of the R register (NTR).
//
struct ArithOverflow : std::runtime_error {
    ArithOverflow() : std::runtime_error("arithmetic overflow") {}
};

class Arith {
public:
    enum : unsigned {
        NORM_DISABLE  = 001,
        ROUND_DISABLE = 002,
        OVF_DISABLE   = 040,
    };

    Word acc{};     // A
    Word rmr{};     // Y, the younger bits
    unsigned rau{}; // mode bits (NTR)

    Arith() = default;
    explicit Arith(Word a, unsigned mode = 0) : acc(a), rau(mode) {}

    void add(Word x) { add(x, false, false); }   // A+X
    void sub(Word x) { add(x, false, true); }    // A-X
    void rsub(Word x) { add(x, true, false); }   // X-A
    void sub_abs(Word x) { add(x, true, true); } // AMX: |A|-|X|
    void mul(Word x);                            // A*X
    void div(Word x);                            // A/X
    void add_exponent(int n);                    // E+N, E-N
    void change_sign(bool negate);               // AVX

private:
    struct Value {
        int64_t mantissa{};  // signed
        unsigned exponent{}; // offset by 64
    };
    void add(Word x, bool negate_acc, bool negate_x);
    void normalize_and_round(Value v, Word mr, bool round_flag);
};

// APX: the bits of val under mask, gathered to the low end.
Word pack_bits(Word val, Word mask);

// AUX: the low bits of val spread to the positions of mask.
Word unpack_bits(Word val, Word mask);

#endif
