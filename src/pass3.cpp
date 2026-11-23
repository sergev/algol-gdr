#include "pass3.h"

#include "translated.h"

unsigned pass3_code(Machine &m, unsigned pc);

namespace {

constexpr unsigned FREE  = 02003; // the free constant slot; it moves down
constexpr unsigned TABLE = 02144; // slot i at TABLE + i
constexpr unsigned LAST  = 01634; // the search ends here (04000)
constexpr unsigned ITEM  = 02032; // the result
constexpr Word NEGATIVE  = 0'0020'0000'0000'0000;
constexpr unsigned SIGN  = 0200; // in the item: a negative constant
constexpr unsigned CONST = 023;  // item code

//
// G22716: intern the constant in A. The slot of an equal word among those
// above the free slot, else the free slot. A and ITEM get the item
// slot<<12 | 023, with 0200 for a negative value. Returns through M12.
//
unsigned intern_constant(Machine &m)
{
    Word value              = m.acc;
    unsigned n              = m.mem[FREE] & 077777;
    m.mem[TABLE + n]        = value;
    m.mem[m.M[Machine::SP]] = value; // pushed and popped
    unsigned slot           = n;
    m.set_m(13, 0);
    m.set_m(14, TABLE + LAST);
    for (unsigned i = n + 1; i <= LAST; i++)
        if (m.mem[(TABLE + i) & 077777] == value) {
            slot = i;
            m.set_m(13, i - LAST);
            m.set_m(14, i);
            break;
        }
    if (slot == n)
        m.mem[FREE] = Word(n - 1);
    Word item = Word(slot) << 12;
    if (value & NEGATIVE)
        item += SIGN;
    m.mem[ITEM] = item;
    m.acc       = item + CONST;
    m.rmr       = 0;
    m.rau       = 003;
    m.set_multiplicative();
    return m.M[12];
}

} // namespace

Stream pass3(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx, pass3_code);
    pass.m.natives[022716] = intern_constant;
    return pass.run(022162, 01632);
}
