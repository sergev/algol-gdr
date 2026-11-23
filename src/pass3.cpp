#include "pass3.h"

#include "translated.h"

unsigned pass3_code(Machine &m, unsigned pc);

namespace {

constexpr unsigned READ        = 027154; // the next item of the input stream in A
constexpr unsigned WRITE       = 027126; // A to the output stream
constexpr unsigned CUR         = 02012;  // the current item
constexpr unsigned M7          = 022162; // base of the constants: M7+1250… = 23432…
constexpr unsigned FREE        = 02003;  // the free constant slot; it moves down
constexpr unsigned POS         = 02001;  // position in the input stream
constexpr unsigned START       = 02013;  // POS at the FOR
constexpr unsigned CONTROL     = 02014;  // d of the control variable (032) of the loop scanned
constexpr unsigned LVAR        = 02025;  // d of the control variable of the FOR
constexpr unsigned SPAN        = 02042;  // nonzero: the loop is not on the page it ends on
constexpr unsigned SET         = 02117;  // variables assigned in the loop: from SET-046, 0 ends
constexpr unsigned FOR_CLASSES = 022264; // 3 bits per code: word code>>4, bits 3·(code&017)
constexpr unsigned TABLE       = 02144;  // slot i at TABLE + i
constexpr unsigned LAST        = 01634;  // the search ends here (04000)
constexpr unsigned ITEM        = 02032;  // the result
constexpr Word NEGATIVE        = 0'0020'0000'0000'0000;
constexpr unsigned SIGN        = 0200; // in the item: a negative constant
constexpr unsigned CONST       = 023;  // item code

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

// Next pass of the main loop after the item in A (22170): codes 146, 075
// (EOP), 055 (FOR), 042, 062 (procedure body: depth), 026, 064 (minus, to
// real: fold a constant), else copy. M14 counts down as in the original.
unsigned dispatch(Machine &m);
unsigned for_scan(Machine &m);

// 22201: copy the current item, then the next.
unsigned copy_item(Machine &m)
{
    m.xta(0, CUR);
    m.call(WRITE, 13, 022167);
    return 022167;
}

// 22167: read an item and dispatch it.
unsigned next_item(Machine &m)
{
    m.call(READ, 13, 022170);
    return dispatch(m);
}

//
// 22203: after a 146 (end of the procedure declarations, a jump over the
// bodies). Line markers (051) are dropped; a 145 (more declarations) drops the
// 146 too, else 146 and the item are written.
//
unsigned procedure_end(Machine &m)
{
    for (;;) {
        m.call(READ, 13, 022204);
        m.atx(0, CUR);
        m.aax(7, 01336);
        m.ati(0, 016);
        m.utm(14, 077727);
        if (m.M[14] == 0)
            continue;
        m.utm(14, 077704);
        if (m.M[14] == 0)
            return 022167;
        m.vtm(14, 0146);
        m.ita(0, 016);
        m.call(WRITE, 13, 022211);
        m.xta(0, CUR);
        return dispatch(m);
    }
}

//
// 22214: an integer minus (026) or int→real (064) before a constant becomes
// a constant: negated, or converted. Else both items are written. Returns
// through M11.
//
unsigned fold(Machine &m)
{
    m.xta(0, CUR);
    m.atx(Machine::SP, 0);
    m.call(READ, 13, 022216);
    m.atx(0, CUR);
    m.aax(7, 01336);
    m.aex(7, 01272);
    if (!m.uza()) {
        m.xta(Machine::SP, 0);
        m.call(WRITE, 13, m.M[11]);
        return m.M[11];
    }
    m.xta(0, CUR);
    m.asn(0, 0114);
    m.sti(0, 016);
    m.aex(7, 01276);
    if (m.uza()) {
        m.xta(14, TABLE);
        m.ntr(0, 2);
        m.arith(0, 0, 004); // + 0: normalized, real
    } else {
        m.xta(14, TABLE);
        m.ntr(0, 3);
        m.arith(7, 01250, 014); // the sign changed
    }
    m.set_m(12, 022227);
    intern_constant(m);
    m.atx(0, CUR);
    return m.M[11];
}

//
// 23343: EOP. Written, then a 0, with the stream marked ended in 02001;
// return to the driver.
//
unsigned eop(Machine &m)
{
    m.xta(0, CUR);
    m.call(WRITE, 13, 023344);
    m.xta(0, 02001);
    m.aox(7, 01306);
    m.atx(0, 02001);
    m.xta(0, 0);
    m.call(WRITE, 13, 023347);
    m.xta(7, 01302);
    m.atx(0, 02001);
    m.wtc(Machine::SP, 0);
    return m.ea(0, m.field(0));
}

unsigned dispatch(Machine &m)
{
    m.atx(0, CUR);
    m.aax(7, 01336);
    m.ati(0, 016);
    m.utm(14, 077632);
    if (m.M[14] == 0)
        return procedure_end(m);
    m.utm(14, 051);
    if (m.M[14] == 0)
        return eop(m);
    m.utm(14, 020);
    if (m.M[14] == 0)
        return for_scan(m);
    m.utm(14, 013);
    if (m.M[14] == 0) {
        m.utm(1, 077777);
        return copy_item(m);
    }
    m.utm(14, 077760);
    if (m.M[14] == 0) {
        m.utm(1, 1);
        return copy_item(m);
    }
    m.utm(14, 034);
    m.vtm(11, 022201);
    if (m.M[14] == 0)
        return fold(m);
    m.utm(14, 077742);
    if (m.M[14] == 0)
        return fold(m);
    return copy_item(m);
}

//
// 22232: FOR (055 d). First scan: copies the loop, classifying each item;
// gives up (copies the rest through the main loop) on any item it cannot
// rewrite. At the end of the body (052, 0160), the variables assigned in it
// get bit 0000040000000000 in their descriptors, and the rewrite (22327)
// starts over from START. A nested FOR restarts the scan from it.
//
unsigned for_scan(Machine &m)
{
restart:
    m.xta(0, CUR);
    m.asn(0, 0114);
    m.ati(0, 2);
    m.atx(0, LVAR);
    m.xta(2, 0);
    m.arx(0, 0);
    if (m.uza())
        return copy_item(m);
    m.xta(0, CUR);
    m.call(WRITE, 13, 022237);
    m.acc = m.mem[POS];
    m.atx(0, START);
    m.set_m(3, 077732); // −046: the first of SET
    goto read;
copy:
    m.xta(0, CUR);
    m.call(WRITE, 13, 022242);
read:
    m.call(READ, 13, 022243);
    m.atx(0, CUR);
classify: {
    unsigned code = unsigned(m.mem[CUR]) & 0177;
    m.store(m.M[Machine::SP], code & 017); // left above the stack
    m.set_m(13, code >> 4);
    unsigned cls = unsigned(m.mem[FOR_CLASSES + (code >> 4)] >> (3 * (code & 017))) & 7;
    m.acc        = cls;
    m.set_logical();
    m.set_m(14, cls);
    switch (cls) {
    case 0: // copied
        goto copy;
    case 1: // its control variable (032, …)
        m.xta(0, CUR);
        m.asn(0, 0114);
        m.atx(0, CONTROL);
        goto copy;
    case 2: // left part (0100, 046, …)
        m.xta(0, CUR);
        m.asn(0, 0114);
        m.atx(3, SET);
        if (m.vlm(3))
            goto copy;
        return copy_item(m); // too many
    case 3:                  // cannot rewrite
        return copy_item(m);
    case 4: // end of the loop
        break;
    case 5: // a FOR inside
        goto restart;
    case 6: // standard procedure: only 6–8
        m.xta(0, CUR);
        m.asn(0, 0114);
        m.ati(0, 016);
        m.xta(7, 0117);
        m.asn(14, 0100);
        m.aax(7, 01260);
        if (m.uza())
            return copy_item(m);
        goto copy;
    default: // 026, 064
        m.set_m(11, 022244);
        fold(m);
        goto classify;
    }
}
    // Its control variable must not be assigned in it.
    m.acc = 0;
    m.atx(3, SET);
    m.set_m(3, 0);
    for (;;) {
        m.xta(3, SET - 046);
        if (m.uza())
            break;
        m.aex(0, CONTROL);
        m.utm(3, 1);
        if (!m.u1a())
            return copy_item(m);
    }
    m.xta(0, CUR);
    m.call(WRITE, 13, 022312);
    m.set_m(3, 0);
    do {
        m.xta(3, SET - 046);
        m.ati(0, 2);
        m.xta(2, 0);
        m.aox(7, 01256);
        m.atx(2, 0);
        m.utm(3, 1);
    } while (m.M[2]);
    m.acc = m.mem[02000];
    m.atx(0, 02015);
    m.xta(0, START);
    m.aex(0, POS);
    m.aax(7, 01254);
    m.atx(0, SPAN);
    if (m.uza())
        return 022327;
    // Back to the page of the FOR.
    m.xta(0, POS);
    m.aox(7, 01306);
    m.atx(0, POS);
    m.xta(0, 0);
    m.call(WRITE, 13, 022325);
    m.xta(0, START);
    m.aox(7, 01306);
    m.atx(0, 02000);
    m.call(READ, 13, 022327);
    return 022327;
}

//
// 22162: init. The return address is pushed, 02000 set from 02001, the
// first item (a 251, the head of the stream) skipped; M1 = 5 − depth.
//
unsigned start(Machine &m)
{
    m.vtm(7, M7);
    m.ita(0, 015);
    m.xts(0, 02001);
    m.arx(7, 01327);
    m.atx(0, 02000);
    m.call(READ, 13, 022165);
    m.xta(7, 01317);
    m.atx(0, 02001);
    m.vtm(1, 5);
    return next_item(m);
}

} // namespace

Stream pass3(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx, pass3_code);
    pass.m.natives[022162] = start;
    pass.m.natives[022167] = next_item;
    pass.m.natives[022201] = copy_item;
    pass.m.natives[022214] = fold;
    pass.m.natives[022232] = for_scan;
    pass.m.natives[022716] = intern_constant;
    return pass.run(022162, 01632);
}
