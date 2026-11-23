#include "pass3.h"

#include "translated.h"

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

//
// The rewrite of a FOR loop (22327…23426). The loop is parsed again from
// the FOR, as a tree in prefix order: a handler per item class (table at
// 22365) parses its operands and leaves a descriptor of the result in OPND:
// bit 48 not linear, bit 47 not constant; bits 31–45 the coefficient c of the
// control variable, bits 16–30 and 1–15 the first and last item of the
// operand on the value stack (M4, from 02144). Return addresses and operands
// are on the machine stack as in the original: parse() pushes M12, a handler
// returns by popping it (ret()). Items are written when an operand is
// finished (flush_stack); an array access linear in the control variable gets
// an index register (163/164 items), its load goes to the preamble list, its
// step to the increments list; both are written after the loop.
//

// Cells, AD02000+n of the source.
constexpr unsigned D13    = 02015;  // 02000 of the main loop, restored at the end
constexpr unsigned D14    = 02016;  // end of the preamble list (from 02454)
constexpr unsigned D15    = 02017;  // end of the body list (from 02702)
constexpr unsigned D16    = 02020;  // end of the increments list (from 02120)
constexpr unsigned D17    = 02021;  // 0: no register for the step
constexpr unsigned D18    = 02022;  // the step's value when a constant
constexpr unsigned D19    = 02023;  // 0: the step is not a constant
constexpr unsigned D20    = 02024;  // nonzero: a subscript needs the step
constexpr unsigned D23    = 02027;  // the item checked by check_operand
constexpr unsigned D24    = 02030;  // the step item
constexpr unsigned D25    = 02031;  // the next 0160 item: register load, 010000 apart
constexpr unsigned D27    = 02033;  // registers taken in the body
constexpr unsigned D28    = 02034;  // registers taken in the preamble, from the top
constexpr unsigned D29    = 02035;  // c<<12 of a subscript for the increment
constexpr unsigned D30    = 02036;  // the initial value item
constexpr unsigned D32    = 02040;  // the initial value as a 045 item, or 0
constexpr unsigned D33    = 02041;  // depth of IF; 077777 after a label of the loop
constexpr unsigned D35    = 02043;  // the until item
constexpr unsigned OPND   = 02037;  // the descriptor of the operand parsed last
constexpr unsigned VALUES = 02143;  // M4: the value stack, from VALUES+1
constexpr unsigned REGS   = 023153; // M7+0771: index registers, from 1

constexpr Word NONLINEAR = 0'4000'0000'0000'0000;
constexpr Word VARIABLE  = 0'2000'0000'0000'0000;

// Entries, jumped to through M12 or the stack.
constexpr unsigned MAIN_LOOP   = 022167;
constexpr unsigned READ_PARSE  = 022347;
constexpr unsigned PARSE       = 022351;
constexpr unsigned DISPATCH    = 022352;
constexpr unsigned STATEMENT   = 022422;
constexpr unsigned GIVE_UP     = 022662; // no register left: D17 = 0
constexpr unsigned NO_REG      = 022653;
constexpr unsigned BINARY_2    = 022523;
constexpr unsigned RESULT      = 022532;
constexpr unsigned UNARY_2     = 022542;
constexpr unsigned UNARY_3     = 022544;
constexpr unsigned AFTER_LABEL = 022516;
constexpr unsigned IF_2        = 023076;
constexpr unsigned PRE_REG     = 023164;

void write_item(Machine &m, unsigned ret)
{
    m.call(WRITE, 13, ret);
}

void read_item(Machine &m, unsigned ret)
{
    m.call(READ, 13, ret);
}

// 15 WTC; UJ: return to the address on the stack.
unsigned ret(Machine &m)
{
    m.wtc(Machine::SP, 0);
    return m.ea(0, m.field(0));
}

//
// 22757: move the nonzero words of [M8, M9] (A = M8<<15 | M9) to M5…,
// zeroing them; M6 = the last one moved. Through M13.
//
void move_items(Machine &m, unsigned link)
{
    m.set_m(13, link);
    m.ati(0, 011);
    m.asn(0, 0117);
    m.ati(0, 010);
    m.mtj(5, 6);
    m.mtj(8, 012);
    m.utm(6, 077777);
    for (;;) {
        m.xta(10, 0);
        if (!m.uza()) {
            m.utm(6, 1);
            m.atx(6, 0);
            m.xta(0, 0);
        }
        m.atx(10, 0);
        m.ita(0, 012);
        m.its(0, 011);
        m.aex(Machine::SP, 0);
        if (m.uza())
            return;
        m.utm(10, 1);
    }
}

//
// 22747: write the value stack and empty it. Through M12.
//
void flush_stack(Machine &m, unsigned link)
{
    m.set_m(12, link);
    m.vtm(5, TABLE);
    for (;;) {
        m.ntr(0, 3);
        m.ita(0, 5);
        m.its(0, 4);
        m.arith(Machine::SP, 0, 005);
        if (m.u1a())
            break;
        m.xta(5, 0);
        if (!m.uza())
            write_item(m, 022754);
        m.utm(5, 1);
    }
    m.vtm(4, VALUES);
}

//
// 22771: the operand in A is done. A single item (the last = the first) is
// kept in D23 (0 if it is the control variable); a nonlinear operand or one
// in an IF is left on the stack; else its items move to the body list after
// a register load (0160 item), and D23 = the load. Through M12.
//
void check_operand(Machine &m, unsigned link)
{
    m.set_m(12, link);
    m.atx(Machine::SP, 0);
    m.aax(7, 01316);
    m.aex(7, 01310);
    if (!m.u1a())
        m.atx(0, D17);
    m.xta(Machine::SP, 077777);
    m.asn(0, 0117);
    m.aex(Machine::SP, 077777);
    m.aax(7, 01332);
    if (!m.u1a()) {
        m.wtc(Machine::SP, 0);
        m.xta(0, 0);
        m.atx(0, D23);
        m.asn(0, 0114);
        m.aex(0, CONTROL);
        if (m.u1a())
            return;
        m.utm(Machine::SP, 1);
    }
    m.xta(0, 0);
    m.atx(0, D23);
    m.xta(Machine::SP, 0);
    m.arx(0, 0);
    if (m.uza()) // nonlinear
        return;
    m.xta(0, D33);
    if (m.u1a()) // in an IF
        return;
    m.wtc(0, D15);
    m.vtm(5, 1);
    m.utm(Machine::SP, 1);
    m.xta(0, D25);
    m.stx(5, 077777);
    move_items(m, 023011);
    m.ita(0, 6);
    m.arx(7, 01260);
    m.atx(0, D15);
    m.xta(0, D25);
    m.arx(7, 01260);
    m.atx(8, 0);
    m.atx(0, D23);
    m.xta(0, D25);
    m.arx(7, 01265);
    m.atx(0, D25);
}

//
// 23017: find the items of OPND (M8…M9) in the list from M5 (up to ITEM):
// M5 = its 0160 or 0162 head, or 0 if not there. Through M13.
//
void find_items(Machine &m, unsigned link)
{
    m.set_m(13, link);
    m.xta(0, OPND);
    m.ati(0, 011);
    m.asn(0, 0117);
    m.ati(0, 010);
    for (;;) {
        m.ita(0, 5);
        m.aex(0, ITEM);
        if (m.uza()) {
            m.vtm(5, 0);
            return;
        }
        m.xta(5, 0);
        m.utm(5, 1);
        m.arx(0, 0);
        if (m.u1a())
            continue;
        m.utm(5, 077777);
        m.mtj(8, 016);
        do {
            m.xta(14, 1);
            if (!m.uza()) {
                m.aex(5, 1);
                m.utm(5, 1);
            }
            m.utm(14, 1);
        } while (m.uza());
        m.aax(7, 01273);
        if (m.u1a())
            continue;
        m.xta(7, 01267);
        m.aex(5, 0);
        m.aax(7, 01336);
        if (m.u1a())
            continue;
        return;
    }
}

// 23051…23072 with M5 = the head found: the element read (0163) or stored
// (0164) through the register. M14 = the code; ends through M12.
unsigned access(Machine &m);

// 23130: a new register for the body.
unsigned body_register(Machine &m);

// 23062: the access through the register of the load at M5.
unsigned through_register(Machine &m)
{
    m.xts(5, 1);
    m.aax(7, 01263);
    m.aox(Machine::SP, 0);
    m.atx(8, 0);
    m.ita(0, 010);
    m.atx(Machine::SP, 0);
    m.asn(0, 061);
    m.aox(Machine::SP, 0);
    m.atx(0, OPND);
    do {
        m.utm(8, 1);
        m.xta(0, 0);
        m.atx(8, 0);
        m.ita(0, 010);
        m.its(0, 011);
        m.aex(Machine::SP, 0);
    } while (m.u1a());
    return m.M[12];
}

// The code of the access: 0163 for an element read (044), else 0164.
void access_code(Machine &m)
{
    m.xta(9, 0);
    m.aax(7, 01336);
    m.aex(7, 01275);
    m.vtm(14, 0163);
    if (!m.uza())
        m.vtm(14, 0164);
    m.xta(8, 0);
    m.aax(7, 01264);
    m.its(0, 016);
    m.aox(Machine::SP, 0);
}

unsigned access(Machine &m)
{
    m.vtm(5, 02702);
    m.xta(0, D15);
    m.atx(0, ITEM);
    find_items(m, 023054);
    access_code(m);
    if (m.M[5] == 0)
        return body_register(m);
    return through_register(m);
}

// 23051: an element not stepped by the loop.
unsigned fixed_access(Machine &m)
{
    m.xta(0, 0);
    m.atx(0, D29);
    return access(m);
}

//
// 23036: an element c·I+d: the increment c·step·stride must fit 12 bits,
// else 23164. Through M12.
//
unsigned stepped_access(Machine &m)
{
    m.xta(14, 1);
    m.asn(0, 0136);
    m.aax(7, 01332);
    m.aox(7, 01321);
    m.ntr(0, 2);
    m.arith(0, D18, 017);
    m.ntr(0, 3);
    m.arith(7, 01321, 004);
    m.aax(7, 01326);
    m.atx(Machine::SP, 0);
    m.arith(7, 01320, 005);
    if (!m.u1a()) {
        m.xta(Machine::SP, 077777);
        m.arith(7, 01314, 005);
        if (m.u1a())
            return PRE_REG;
    }
    m.xta(Machine::SP, 0);
    m.aax(7, 01257);
    m.asn(0, 064);
    m.atx(0, D29);
    return access(m);
}

// The load of the operand OPND (items moved from the value stack to the
// list ending at [list]): its 0160 head becomes 0162 and gets register
// number k of the table at REGS.
void load_register(Machine &m, unsigned list, unsigned k, unsigned link)
{
    m.xta(0, OPND);
    m.wtc(0, list);
    m.vtm(5, 0);
    move_items(m, link);
    m.xta(5, 0);
    m.aax(7, 01257);
    m.aox(7, 01316);
    m.atx(5, 0);
    m.mtj(6, 5);
    m.utm(6, 2);
    m.ita(0, 6);
    m.atx(0, list);
    m.xta(5, 0);
    m.aax(7, 01273);
    m.aex(7, 01267);
    m.atx(5, 0);
    m.wtc(0, k);
    m.xta(7, 0771);
    m.atx(5, 1);
}

// A to the increments list.
void add_increment(Machine &m)
{
    m.wtc(0, D16);
    m.atx(0, 0);
    m.xta(0, D16);
    m.arx(7, 01260);
}

// 23130: a register for the body (D27+1, unless it meets the preamble's),
// stepped by c<<12 | 0165 at the end when D29 ≠ 0.
unsigned body_register(Machine &m)
{
    m.xts(0, D27);
    m.arx(7, 01260);
    m.aex(0, D28);
    m.utm(Machine::SP, 077777);
    if (m.uza())
        return GIVE_UP;
    m.aex(0, D28);
    m.atx(0, D27);
    m.utm(Machine::SP, 1);
    load_register(m, D15, D27, 023136);
    m.xta(0, D29);
    if (!m.uza()) {
        m.aex(5, 1);
        m.aex(7, 01301);
        add_increment(m);
        m.atx(0, D16);
    }
    m.xta(Machine::SP, 0);
    return through_register(m);
}

//
// 23164: an element whose register is loaded in the preamble: D20 set when
// M9 ≠ 0. A new register (D28−1, not in an IF) is stepped at the end.
//
unsigned preamble_access(Machine &m)
{
    if (m.M[9]) {
        m.xta(7, 01260);
        m.atx(0, D20);
    }
    m.vtm(5, 02454);
    m.xta(0, D14);
    m.atx(0, ITEM);
    find_items(m, 023170);
    access_code(m);
    if (m.M[5] == 0) {
        m.xts(0, D33);
        m.utm(Machine::SP, 077777);
        if (m.u1a())
            return GIVE_UP;
        m.xta(0, D28);
        m.arx(7, 01323);
        m.aex(0, D27);
        if (m.uza())
            return GIVE_UP;
        m.aex(0, D27);
        m.atx(0, D28);
        m.utm(Machine::SP, 1);
        load_register(m, D14, D28, 023216);
        m.aex(7, 01301);
        add_increment(m);
        m.stx(0, D16);
    }
    through_register(m);
    return NO_REG;
}

// 22327: set up the rewrite; page 10000 to drum 26 zone 3.
unsigned begin_rewrite(Machine &m)
{
    m.xta(0, START);
    m.arx(7, 01246);
    m.atx(0, 02000);
    m.xta(7, 01265);
    m.atx(0, POS);
    m.vtm(14, 02454);
    m.ita(0, 016);
    m.atx(0, D14);
    m.vtm(14, 02702);
    m.ita(0, 016);
    m.atx(0, D15);
    m.vtm(14, 02120);
    m.ita(0, 016);
    m.atx(0, D16);
    m.atx(0, D17);
    m.xta(7, 01251);
    m.atx(0, D25);
    m.xta(0, 0);
    for (unsigned a : { D18, D33, D32, D30, D20 })
        m.atx(0, a);
    m.ita(0, 1);
    m.aex(7, 01331);
    m.arx(7, 01277);
    m.atx(0, D27);
    m.xta(7, 01307);
    m.atx(0, D28);
    m.extracode(070, 7, 01337);
    m.vtm(4, VALUES);
    m.vtm(12, STATEMENT);
    return READ_PARSE;
}

// 22347: read an item, then parse.
unsigned read_parse(Machine &m)
{
    read_item(m, 022350);
    m.atx(0, CUR);
    return PARSE;
}

// 22351: parse the item in CUR, returning to M12.
unsigned parse(Machine &m)
{
    m.ita(0, 014);
    m.atx(Machine::SP, 0);
    return DISPATCH;
}

//
// 22352: by the class of the item (6 bits per code at 22365): 0 is
// written and the statement goes on; else the item goes on the value stack
// and to its handler, through the table at 22407: per word, the handler of
// an even class and of the odd one after it.
//
unsigned classify(Machine &m)
{
    static const unsigned handlers[][2] = {
        { 0, 023231 },      { 022441, 022522 }, { 022541, 022551 }, { 022570, 022572 },
        { 022600, 022604 }, { 022607, 022607 }, { 022664, 022666 }, { 022665, 023417 },
        { 022425, 022436 }, { 022507, 023073 }, { 023115, 023123 },
    };
    m.xta(0, CUR);
    m.asn(0, 0103);
    m.aax(7, 01333);
    m.ati(0, 015);
    m.xta(0, CUR);
    m.aax(7, 01304);
    m.atx(Machine::SP, 0);
    m.asn(0, 077);
    m.arx(Machine::SP, 0);
    m.asn(0, 077);
    m.ati(0, 016);
    m.utc(13, 022365);
    m.xta(0, 0);
    m.asn(14, 0100);
    m.aax(7, 01335);
    m.ati(0, 016);
    if (m.M[14] == 0) {
        m.sti(0, 0);
        m.xta(0, CUR);
        write_item(m, 022364);
        m.vtm(12, STATEMENT);
        return READ_PARSE;
    }
    m.xta(0, CUR);
    m.utm(4, 1);
    m.atx(4, 0);
    m.ita(0, 016);
    m.asn(0, 0101);
    m.ati(0, 016);
    m.yta(0, 0);
    if (m.M[14] > 10)
        throw std::runtime_error("pass 3: item " + std::to_string(m.mem[CUR] & 0177));
    return handlers[m.M[14]][m.uza() ? 0 : 1];
}

// 22422: a statement is done.
unsigned statement(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, 022423);
    flush_stack(m, 022424);
    m.vtm(12, STATEMENT);
    return PARSE;
}

// 22425: an expression element (0136, 0165, 0177): its operand.
unsigned expression_element(Machine &m)
{
    m.set_m(12, 022426);
    return READ_PARSE;
}

unsigned expression_element_2(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, 022427);
    m.xta(0, D23);
    if (!m.uza()) {
        m.xta(0, OPND);
        m.asn(0, 0117);
        m.ati(0, 010);
        m.asn(0, 061);
        m.its(0, 010);
        m.aox(Machine::SP, 0);
        m.atx(0, OPND);
        m.xta(0, 0);
        m.atx(8, 077777);
    }
    return ret(m);
}

// 22436: a constant step (0137): the control variable is marked, then as 034.
unsigned constant_step(Machine &m)
{
    m.wtc(0, LVAR);
    m.vtm(14, 0);
    m.xta(14, 0);
    m.aox(7, 01252);
    m.atx(14, 0);
    m.set_m(12, 022442);
    return READ_PARSE;
}

// 22441: step-until (034): the initial value, the step, the until value.
unsigned step_until(Machine &m)
{
    m.set_m(12, 022442);
    return READ_PARSE;
}

// The initial value: as a 045 item in D32 when a constant of 12 bits ≠ 0;
// its item in D30.
unsigned initial_parsed(Machine &m)
{
    m.xta(0, OPND);
    m.aax(7, 01316);
    if (!m.u1a()) {
        m.xta(0, OPND);
        m.aax(7, 01262);
        if (!m.uza()) {
            m.asn(0, 0136);
            m.ntr(0, 3);
            m.arith(7, 01261, 005);
            if (!m.uza()) {
                m.arith(7, 01261, 004);
                m.asn(0, 064);
                m.aox(7, 01300);
                m.atx(0, D32);
            }
        }
    }
    m.xta(0, OPND);
    m.aax(7, 01270);
    m.aox(7, 01274);
    m.atx(0, OPND);
    check_operand(m, 022454);
    m.xta(0, D23);
    m.atx(0, D30);
    flush_stack(m, 022456);
    m.set_m(12, 022457);
    return PARSE;
}

// Then the next operand.
unsigned parse_until(Machine &m)
{
    flush_stack(m, 022470);
    m.set_m(12, 022471);
    return PARSE;
}

// The step: its item in D24, its value in D18 when a constant; a step
// not linear clears D17 and D19.
unsigned step_parsed(Machine &m)
{
    m.xta(0, OPND);
    m.aax(7, 01316);
    if (m.u1a()) {
        m.xta(0, OPND);
        m.arx(0, 0);
        if (!m.u1a()) {
            m.xta(0, 0);
            m.atx(0, D19);
            m.atx(0, D17);
            return parse_until(m);
        }
    } else {
        m.xta(0, OPND);
        m.ati(0, 016);
        m.xta(14, 0);
        m.atx(0, D24);
        m.asn(0, 0114);
        m.ati(0, 016);
        m.xta(14, TABLE);
        m.asn(0, 071);
        if (!m.uza()) {
            m.xta(14, TABLE);
            m.aox(7, 01321);
            m.atx(0, D18);
            return parse_until(m);
        }
    }
    m.xta(0, OPND);
    check_operand(m, 022505);
    m.xta(0, D23);
    m.atx(0, D24);
    return parse_until(m);
}

// The until value: its item in D35; then the body (from DO). Not linear:
// D17 = 0, D33 = 1.
unsigned until_parsed(Machine &m)
{
    m.xta(0, OPND);
    m.arx(0, 0);
    if (!m.u1a()) {
        m.xta(0, 0);
        m.atx(0, D17);
        m.xta(7, 01260);
        m.atx(0, D33);
    }
    m.xta(0, OPND);
    check_operand(m, 022476);
    m.xta(0, D23);
    m.atx(0, D35);
    flush_stack(m, DISPATCH);
    return DISPATCH;
}

// 22507: a label (024, 0163). One of the loop (below the control variable's
// block) stops register steps: D33 = 077777; else D17 = 0.
unsigned label(Machine &m)
{
    m.xta(0, CUR);
    m.asn(0, 0114);
    m.atx(0, ITEM);
    m.ntr(0, 3);
    m.arith(0, LVAR, 005);
    bool inside = false;
    if (!m.u1a()) {
        m.wtc(0, LVAR);
        m.xta(0, 0);
        m.aax(7, 01257);
        m.arith(0, ITEM, 006);
        inside = !m.uza();
    }
    if (inside) {
        m.xta(7, 01332);
        m.atx(0, D33);
    } else {
        m.xta(0, 0);
        m.atx(0, D17);
    }
    return AFTER_LABEL;
}

unsigned after_label(Machine &m)
{
    m.sti(0, 0);
    flush_stack(m, 022517);
    m.vtm(12, STATEMENT);
    return READ_PARSE;
}

// 22522: a binary operator: the left operand.
unsigned binary(Machine &m)
{
    m.set_m(12, BINARY_2);
    return READ_PARSE;
}

// The right operand.
unsigned binary_2(Machine &m)
{
    m.xta(0, OPND);
    m.atx(Machine::SP, 0);
    m.set_m(12, 022525);
    return PARSE;
}

// 22531 with A: OPND = A | the last item of OPND; 22532 with A and a word
// on the stack: OPND = A | the word; return.
unsigned result_last(Machine &m)
{
    m.xts(0, OPND);
    m.aax(7, 01332);
    return RESULT;
}

unsigned result(Machine &m)
{
    m.aox(Machine::SP, 0);
    m.atx(0, OPND);
    return ret(m);
}

// 22527: a variable (not linear) result from the left operand's first item.
unsigned variable_result(Machine &m)
{
    m.xta(Machine::SP, 0);
    m.arx(7, 01330);
    m.aax(7, 01254);
    m.aox(7, 01274);
    return result_last(m);
}

// 22534: a nonlinear result: both operands done.
unsigned nonlinear_result(Machine &m)
{
    m.xta(Machine::SP, 077777);
    check_operand(m, 022535);
    m.xta(0, OPND);
    check_operand(m, 022536);
    m.xta(Machine::SP, 0);
    m.arx(7, 01330);
    m.aax(7, 01254);
    m.aox(7, 01316);
    return result_last(m);
}

unsigned binary_3(Machine &m)
{
    m.xta(Machine::SP, 077777);
    m.aox(0, OPND);
    m.arx(0, 0);
    if (m.uza())
        return nonlinear_result(m);
    return variable_result(m);
}

// 22541: a unary operator (NOT, 047, 063, 064, 071, 0152, 0153).
unsigned unary(Machine &m)
{
    m.set_m(12, UNARY_2);
    return READ_PARSE;
}

unsigned unary_3(Machine &m)
{
    m.arx(7, 01330);
    m.atx(0, OPND);
    return ret(m);
}

unsigned unary_2(Machine &m)
{
    m.xta(0, OPND);
    m.arx(0, 0);
    if (m.uza()) {
        m.xta(0, OPND);
        check_operand(m, 022547);
        m.xta(0, OPND);
        m.aox(7, 01316);
    } else {
        m.aox(7, 01274);
    }
    return unary_3(m);
}

// 22560 with the flags in A: OPND = flags | the item at M4; next item.
unsigned leaf(Machine &m)
{
    m.its(0, 4);
    m.atx(Machine::SP, 0);
    m.asn(0, 061);
    m.aox(Machine::SP, 0);
    m.aox(Machine::SP, 0);
    m.atx(0, OPND);
    read_item(m, 022564);
    m.atx(0, CUR);
    return ret(m);
}

// 22551: a variable (017). The control variable is linear (an initial
// value item is marked 0200 at M4); one assigned in the loop is not.
unsigned variable(Machine &m)
{
    m.xta(0, CUR);
    m.asn(0, 0114);
    m.ati(0, 2);
    m.aex(0, CONTROL);
    if (m.u1a()) {
        m.xta(2, 0);
        m.aax(7, 01256);
        m.xta(7, m.u1a() ? 01316 : 01274);
        return leaf(m);
    }
    m.xta(0, D30);
    if (!m.u1a()) {
        m.xta(4, 0);
        m.aox(7, 01253);
        m.atx(4, 0);
    }
    m.xta(0, D19);
    if (m.uza())
        m.xta(7, 01316);
    else
        m.xta(7, 01312);
    return leaf(m);
}

// 22570: a string (070).
unsigned string_item(Machine &m)
{
    m.xta(7, 01274);
    return leaf(m);
}

// 22572: a constant (023): linear with c = the value if an integer.
unsigned constant(Machine &m)
{
    m.xta(0, CUR);
    m.asn(0, 0114);
    m.ati(0, 016);
    m.xta(14, TABLE);
    m.aex(7, 01321);
    m.asn(0, 0153);
    if (!m.u1a()) {
        m.xta(14, TABLE);
        m.asn(0, 042);
    }
    m.aax(7, 01262);
    return leaf(m);
}

// 22600: a constant subscript (045, 0166): (M4+1)<<15 on the stack, then
// the next item.
unsigned constant_subscript(Machine &m)
{
    read_item(m, 022601);
    m.atx(0, CUR);
    m.ita(0, 4);
    m.arx(7, 01260);
    m.asn(0, 061);
    m.atx(Machine::SP, 0);
    return DISPATCH;
}

// 22604: subscript + c (030): the subscript, then its OPND on the stack.
unsigned subscript_plus(Machine &m)
{
    m.set_m(12, 022605);
    return READ_PARSE;
}

unsigned subscript_plus_2(Machine &m)
{
    m.xta(0, OPND);
    m.atx(Machine::SP, 0);
    return DISPATCH;
}

//
// 22654 with A: OPND = A; the subscripts dropped, next item, return.
unsigned element_done(Machine &m)
{
    m.atx(0, OPND);
    read_item(m, 022655);
    m.atx(0, CUR);
    m.ita(0, 017);
    m.its(0, 3);
    m.ntr(0, 3);
    m.arith(Machine::SP, 0, 006);
    m.ati(0, 017);
    return ret(m);
}

// 22653: the element as it is (not constant).
unsigned no_register(Machine &m)
{
    m.xta(0, OPND);
    m.aox(7, 01316);
    return element_done(m);
}

// 22662: no register for the step.
unsigned give_up(Machine &m)
{
    m.xta(0, 0);
    m.atx(0, D17);
    return NO_REG;
}

// 22652: after a stepped access: the step is needed.
unsigned after_stepped(Machine &m)
{
    m.xta(7, 01260);
    m.atx(0, D20);
    return NO_REG;
}

//
// 22607: an array element (044 read, 0164, 046 store), its n subscripts on
// the stack. Not in a loop's preamble (D30 = 0) or with a nonlinear
// subscript: as it is. Else one stepped subscript (M10), the rest constant
// (M9 of them with c ≠ 0): through a register.
//
unsigned element(Machine &m)
{
    m.xta(0, CUR);
    m.asn(0, 0114);
    m.ati(0, 2);
    m.xta(0, CUR);
    m.asn(0, 0107);
    m.aax(7, 01334);
    m.ati(0, 3);
    m.aex(7, 01332);
    m.ati(0, 016);
    m.utc(Machine::SP, 1);
    m.xta(14, 0);
    m.arx(7, 01330);
    m.aax(7, 01254);
    m.its(0, 4);
    m.aox(Machine::SP, 0);
    m.atx(0, OPND);
    m.xta(0, D30);
    if (m.uza())
        return NO_REG;
    m.utc(Machine::SP, 077777);
    m.vtm(14, 0);
    m.mtj(3, 015);
    m.vtm(10, 0);
    m.vtm(11, 0);
    m.vtm(9, 0);
    do {
        m.xta(14, 0);
        m.arx(0, 0);
        if (!m.u1a()) {
            m.aax(7, 01274);
            if (!m.uza()) {
                m.utm(11, 1);
            } else {
                m.utm(10, 1);
                m.xta(14, 0);
                m.aax(7, 01262);
                if (!m.uza())
                    m.utm(9, 1);
            }
        }
        m.utm(13, 077777);
        m.utm(14, 077777);
    } while (m.M[13]);
    if (m.M[11])
        return m.M[10] ? GIVE_UP : NO_REG;
    if (m.M[10]) {
        m.xta(0, D19);
        if (m.uza())
            return GIVE_UP;
        m.utm(10, 077777);
        if (m.M[10])
            return PRE_REG;
        m.xta(14, 1);
        m.arx(0, 0);
        if (m.u1a())
            return PRE_REG;
        m.aax(7, 01262);
        if (m.uza())
            return PRE_REG;
        m.xta(0, D18);
        if (m.uza())
            return PRE_REG;
        m.set_m(12, 022652);
        return stepped_access(m);
    }
    m.xta(0, CUR);
    m.aax(7, 01336);
    m.aex(7, 01275);
    if (!m.u1a()) {
        m.xta(2, 0);
        m.aax(7, 01256);
        if (!m.u1a()) {
            m.xta(2, 0);
            m.aax(7, 01266);
            if (!m.u1a()) {
                m.xta(0, OPND);
                m.aox(7, 01274);
                return element_done(m);
            }
        }
    }
    m.vtm(12, NO_REG);
    return fixed_access(m);
}

// 22664…22666: integer + (067), − (076, 0170), × (077): M3 = 0, 1, 2 on
// the stack; the left operand.
unsigned int_op(Machine &m, unsigned op)
{
    m.vtm(3, op);
    m.ita(0, 3);
    m.atx(Machine::SP, 0);
    m.set_m(12, 022671);
    return READ_PARSE;
}

// Not a constant on the left: as any binary operator.
unsigned int_op_2(Machine &m)
{
    m.xta(0, OPND);
    m.aax(7, 01316);
    m.aex(7, 01316);
    if (!m.u1a()) {
        m.sti(0, 0);
        return BINARY_2;
    }
    m.xta(0, OPND);
    m.atx(Machine::SP, 0);
    m.set_m(12, 022676);
    return PARSE;
}

// A linear operand: c, the first item and the last item of the result.
unsigned linear_result(Machine &m);

// The constant of an operand (descriptor in A): its value.
void constant_value(Machine &m)
{
    m.asn(0, 0117);
    m.ati(0, 016);
    m.xta(14, 0);
    m.asn(0, 0114);
    m.ati(0, 016);
    m.xta(14, TABLE);
}

// Both operands: two constants are folded into one, interned.
unsigned int_op_3(Machine &m)
{
    m.xta(Machine::SP, 077776);
    m.ati(0, 3);
    m.xta(Machine::SP, 077777);
    m.aox(0, OPND);
    m.aax(7, 01316);
    if (m.u1a())
        return linear_result(m);
    m.xta(Machine::SP, 077777);
    constant_value(m);
    m.xts(0, OPND);
    constant_value(m);
    m.ntr(0, 3);
    switch (m.M[3]) {
    case 0:
        m.arith(Machine::SP, 0, 004);
        break;
    case 1:
        m.arith(Machine::SP, 0, 006);
        break;
    default:
        m.ntr(0, 2);
        m.arith(Machine::SP, 0, 017);
        m.ntr(0, 3);
        m.arith(7, 01321, 004);
        break;
    }
    m.set_m(12, 022734);
    intern_constant(m);
    // The left operand's items replaced by the new constant.
    m.xts(Machine::SP, 077776);
    m.asn(0, 0117);
    m.sti(0, 016);
    m.atx(14, 077777);
    do {
        m.xta(0, 0);
        m.atx(14, 0);
        m.ita(0, 016);
        m.aex(0, OPND);
        m.aax(7, 01332);
        m.utm(14, 1);
    } while (m.u1a());
    m.xta(Machine::SP, 0);
    m.arx(7, 01330);
    m.aax(7, 01254);
    m.atx(Machine::SP, 077777);
    m.asn(0, 0117);
    return RESULT;
}

//
// 23351: + − × with an operand not constant (A = the flags of both): linear
// when both are (c of the result: c1 ± c2, or c1·d2 or d1·c2 for × with a
// constant), else as any binary operator.
//
unsigned linear_result(Machine &m)
{
    m.arx(0, 0);
    if (!m.uza()) {
        m.xta(Machine::SP, 0);
        m.atx(Machine::SP, 077777);
        return variable_result(m);
    }
    m.xta(0, OPND);
    m.aax(7, 01316);
    m.aex(7, 01316);
    bool fold_c = false;
    if (!m.u1a()) {
        m.xta(Machine::SP, 0);
        m.atx(Machine::SP, 077777);
        return nonlinear_result(m);
    }
    m.utm(3, 077776);
    m.xta(Machine::SP, 077777);
    if (m.M[3] == 0) { // ×
        m.arx(0, 0);
        if (m.u1a()) {
            fold_c = true;
        } else {
            m.xta(0, OPND);
            m.arx(0, 0);
            if (!m.u1a()) {
                m.xta(Machine::SP, 0);
                m.atx(Machine::SP, 077777);
                return nonlinear_result(m);
            }
            fold_c = true;
        }
    } else {
        m.arx(0, 0);
        if (m.uza()) {
            m.aax(7, 01262);
            if (!m.uza()) {
                m.xta(0, OPND);
                m.arx(0, 0);
                if (m.uza()) {
                    m.aax(7, 01262);
                    fold_c = !m.uza();
                } else {
                    m.xta(Machine::SP, 077777);
                }
            }
        } else {
            m.xta(0, OPND);
            m.utm(3, 2);
            if (m.M[3]) {
                m.aex(7, 01262);
                m.arx(7, 01255);
            }
        }
    }
    if (fold_c) {
        m.xta(Machine::SP, 077777);
        m.aax(7, 01262);
        m.asn(0, 0136);
        m.aox(7, 01321);
        m.xts(0, OPND);
        m.aax(7, 01262);
        m.asn(0, 0136);
        m.aox(7, 01321);
        m.ntr(0, 3);
        switch (m.M[3]) {
        case 077776:
            m.arith(Machine::SP, 0, 004);
            break;
        case 077777:
            m.arith(Machine::SP, 0, 006);
            break;
        default:
            m.ntr(0, 2);
            m.arith(Machine::SP, 0, 017);
            m.ntr(0, 3);
            m.arith(7, 01321, 004);
            break;
        }
        m.asn(0, 042);
    }
    m.aax(7, 01262);
    m.xts(Machine::SP, 077776);
    m.arx(7, 01330);
    m.aax(7, 01254);
    m.xts(0, OPND);
    m.aax(7, 01332);
    m.aox(Machine::SP, 0);
    m.aox(Machine::SP, 0);
    m.aox(7, 01310);
    m.stx(0, OPND);
    m.sti(0, 0);
    return ret(m);
}

// 23417: integer negation (026): c negated.
unsigned int_neg(Machine &m)
{
    m.set_m(12, 023420);
    return READ_PARSE;
}

unsigned int_neg_2(Machine &m)
{
    m.xta(0, OPND);
    m.aax(7, 01316);
    m.aex(7, 01310);
    if (m.u1a())
        return UNARY_2;
    m.xta(0, OPND);
    m.aax(7, 01262);
    m.aex(7, 01262);
    m.arx(7, 01255);
    m.aax(7, 01262);
    m.xts(0, OPND);
    m.aax(7, 01322);
    m.aox(Machine::SP, 0);
    return UNARY_3;
}

// 23073: IF (027): M4 on the stack; the condition.
unsigned if_item(Machine &m)
{
    m.ita(0, 4);
    m.atx(Machine::SP, 0);
    m.set_m(12, 023075);
    return READ_PARSE;
}

unsigned if_2(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, IF_2);
    return IF_2;
}

//
// 23076: the branches, D33 counting the depth. An IF statement (the parse
// returns to STATEMENT) ends here; a conditional expression parses both
// branches, then gives a nonlinear operand.
//
unsigned if_branches(Machine &m)
{
    m.xta(0, D33);
    m.arx(7, 01260);
    m.atx(0, D33);
    m.wtc(Machine::SP, 077776);
    m.vtm(14, 055356);
    if (m.M[14] == 0) {
        m.sti(0, 0);
        flush_stack(m, 023127);
        m.vtm(12, STATEMENT);
        return DISPATCH;
    }
    m.set_m(12, 023102);
    return PARSE;
}

unsigned if_then(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, 023103);
    m.xta(0, CUR);
    m.utm(4, 1);
    m.atx(4, 0);
    m.set_m(12, 023105);
    return READ_PARSE;
}

unsigned if_else(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, 023106);
    m.xta(0, CUR);
    m.utm(4, 1);
    m.atx(4, 0);
    read_item(m, 023110);
    m.atx(0, CUR);
    m.xta(0, D33);
    m.ntr(0, 3);
    m.arith(7, 01260, 005);
    m.stx(0, D33);
    m.asn(0, 061);
    m.aox(7, 01316);
    m.its(0, 4);
    return RESULT;
}

// 23115: compare-and-jump (0130–0134): both sides, then as IF.
unsigned compare(Machine &m)
{
    m.ita(0, 4);
    m.atx(Machine::SP, 0);
    m.set_m(12, 023117);
    return READ_PARSE;
}

unsigned compare_2(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, 023120);
    m.set_m(12, 023121);
    return PARSE;
}

unsigned compare_3(Machine &m)
{
    m.xta(0, OPND);
    check_operand(m, IF_2);
    return IF_2;
}

// 23123: the end of an IF (053, 054).
unsigned if_end(Machine &m)
{
    m.xta(0, D33);
    m.ntr(0, 3);
    m.arith(7, 01260, 005);
    m.atx(0, D33);
    return AFTER_LABEL;
}

//
// 23231: the end of the loop (052, 0160). The marks of the assigned
// variables are cleared; the loop is read again from START and written: the
// 0160 heads of the body list without the item of the control variable
// (replaced by D30, or both by D32 for a 030 item), the preamble (0167 head,
// the initial value, the until and end items, the loads, then
// (0167−D28)<<7 | 0166), then the items up to the 052, the increments and
// the 052. The control variable's descriptor gets flags 0100000 or 0200000
// (step 1 or not) when all is in registers, and 0400000 with the count of
// registers. Page 10000 is restored from drum 26.
//
unsigned end_loop(Machine &m)
{
    m.utm(Machine::SP, 077777);
    flush_stack(m, 023232);
    m.vtm(3, SET - 046);
    for (;;) {
        m.xta(3, 0);
        m.ati(0, 2);
        if (m.M[2] == 0)
            break;
        m.xta(2, 0);
        m.aax(7, 01325);
        m.atx(2, 0);
        m.utm(3, 1);
    }
    m.xta(0, START);
    m.atx(0, POS);
    m.xta(0, SPAN);
    if (!m.uza()) {
        m.xta(0, START);
        m.aox(7, 01306);
        m.atx(0, 02000);
        read_item(m, 023243);
    }
    m.xta(0, CONTROL);
    m.asn(0, 064);
    m.aex(7, 01333);
    m.atx(0, ITEM);
    m.vtm(5, 02702);
    m.xta(0, POS);
    m.arx(0, 0);
    if (!m.u1a()) {
        m.ati(0, 016);
        m.xta(14, 0);
        m.aax(7, 01315);
        m.atx(14, 0);
    }
    // The body list.
    for (;;) {
        m.ita(0, 5);
        m.aex(0, D15);
        if (m.uza())
            break;
        m.xta(5, 0);
        m.aax(7, 01271);
        m.atx(5, 0);
        m.aex(0, ITEM);
        if (m.uza()) {
            m.xta(0, D30);
        } else {
            m.xta(5, 0);
            m.aex(7, 01311);
            bool both = false;
            if (m.uza()) {
                m.xta(5, 1);
                m.aex(0, ITEM);
                if (!m.u1a()) {
                    m.xta(0, D32);
                    both = !m.uza();
                }
            }
            if (both) {
                m.utm(5, 1);
            } else {
                m.xta(5, 0);
                m.xta(5, 0);
            }
        }
        m.utm(5, 1);
        write_item(m, 023251);
    }
    // The preamble.
    m.vtm(5, 02454);
    m.ita(0, 5);
    m.aex(0, D14);
    if (!m.uza()) {
        m.xta(0, CONTROL);
        m.asn(0, 064);
        m.xts(7, 01305);
        m.aex(Machine::SP, 0);
        write_item(m, 023274);
        m.xta(0, D30);
        write_item(m, 023275);
        m.xta(0, D24);
        write_item(m, 023276);
        m.xta(0, D35);
        write_item(m, 023277);
        for (;;) {
            m.ita(0, 5);
            m.aex(0, D14);
            if (m.uza())
                break;
            m.xta(5, 0);
            m.aax(7, 01271);
            m.utm(5, 1);
            write_item(m, 023277);
        }
        m.xta(7, 01307);
        m.ntr(0, 3);
        m.arith(0, D28, 005);
        m.asn(0, 071);
        m.xts(7, 01303);
        m.aex(Machine::SP, 0);
        write_item(m, 023307);
    }
    m.xta(0, D18);
    if (!m.uza()) {
        m.xta(0, D20);
        if (!m.uza()) {
            m.xta(0, D17);
            if (!m.uza()) {
                m.wtc(0, LVAR);
                m.vtm(14, 0);
                m.xta(0, D18);
                m.aex(7, 01324);
                m.xta(7, m.u1a() ? 01246 : 01245);
                m.aox(14, 0);
                m.atx(14, 0);
            }
        }
    }
    m.xta(0, D25);
    m.aax(7, 01264);
    m.asn(0, 064);
    m.wtc(0, LVAR);
    m.aex(0, 0);
    m.aox(7, 01247);
    m.wtc(0, LVAR);
    m.atx(0, 0);
    m.xta(7, 01265);
    m.atx(0, 02000);
    // The loop again, up to its 052.
    for (;;) {
        read_item(m, 023326);
        m.atx(Machine::SP, 0);
        m.aax(7, 01336);
        m.aex(7, 01313);
        if (m.uza())
            break;
        m.xta(Machine::SP, 0);
        write_item(m, 023325);
    }
    m.vtm(5, 02120);
    for (;;) {
        m.ita(0, 5);
        m.aex(0, D16);
        if (m.uza())
            break;
        m.xta(5, 0);
        m.utm(5, 1);
        write_item(m, 023333);
    }
    m.xta(Machine::SP, 0);
    write_item(m, 023340);
    m.extracode(070, 7, 01340);
    m.xta(0, D13);
    m.atx(0, 02000);
    return MAIN_LOOP;
}

void register_rewrite(Machine &m)
{
    m.natives[022327]      = begin_rewrite;
    m.natives[READ_PARSE]  = read_parse;
    m.natives[PARSE]       = parse;
    m.natives[DISPATCH]    = classify;
    m.natives[STATEMENT]   = statement;
    m.natives[022425]      = expression_element;
    m.natives[022426]      = expression_element_2;
    m.natives[022436]      = constant_step;
    m.natives[022441]      = step_until;
    m.natives[022442]      = initial_parsed;
    m.natives[022457]      = step_parsed;
    m.natives[022471]      = until_parsed;
    m.natives[022507]      = label;
    m.natives[AFTER_LABEL] = after_label;
    m.natives[022522]      = binary;
    m.natives[BINARY_2]    = binary_2;
    m.natives[022525]      = binary_3;
    m.natives[RESULT]      = result;
    m.natives[022541]      = unary;
    m.natives[UNARY_2]     = unary_2;
    m.natives[UNARY_3]     = unary_3;
    m.natives[022551]      = variable;
    m.natives[022570]      = string_item;
    m.natives[022572]      = constant;
    m.natives[022600]      = constant_subscript;
    m.natives[022604]      = subscript_plus;
    m.natives[022605]      = subscript_plus_2;
    m.natives[022607]      = element;
    m.natives[022652]      = after_stepped;
    m.natives[NO_REG]      = no_register;
    m.natives[GIVE_UP]     = give_up;
    m.natives[022664]      = [](Machine &mm) { return int_op(mm, 0); };
    m.natives[022665]      = [](Machine &mm) { return int_op(mm, 1); };
    m.natives[022666]      = [](Machine &mm) { return int_op(mm, 2); };
    m.natives[022671]      = int_op_2;
    m.natives[022676]      = int_op_3;
    m.natives[PRE_REG]     = preamble_access;
    m.natives[023073]      = if_item;
    m.natives[023075]      = if_2;
    m.natives[IF_2]        = if_branches;
    m.natives[023102]      = if_then;
    m.natives[023105]      = if_else;
    m.natives[023115]      = compare;
    m.natives[023117]      = compare_2;
    m.natives[023121]      = compare_3;
    m.natives[023123]      = if_end;
    m.natives[023231]      = end_loop;
    m.natives[023417]      = int_neg;
    m.natives[023420]      = int_neg_2;
}

} // namespace

Stream pass3(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx);
    register_rewrite(pass.m);
    pass.m.natives[022162] = start;
    pass.m.natives[022167] = next_item;
    pass.m.natives[022201] = copy_item;
    pass.m.natives[022214] = fold;
    pass.m.natives[022232] = for_scan;
    pass.m.natives[022716] = intern_constant;
    return pass.run(022162, 01632);
}
