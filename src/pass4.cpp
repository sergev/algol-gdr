#include "pass4.h"

#include "translated.h"

unsigned pass4_code(Machine &m, unsigned pc);

namespace {

constexpr unsigned M7          = 023523; // base of the constants: M7+2441… = 26164…
constexpr unsigned READ        = 027154; // the next item of the input stream in A
constexpr unsigned WRITE       = 027126; // A to the output stream
constexpr unsigned PAGE        = 02000;  // page and position of the streams
constexpr unsigned POS         = 02001;
constexpr unsigned EMIT_MODE   = 02022;      // 1 after 0200.0000: the next half starts a word
constexpr unsigned NAMES       = 06000;      // the names, from drum 26 zone 0
constexpr unsigned EXT_NAMES   = 027245;     // external table: 3 words per name
constexpr unsigned LINE        = M7 + 01107; // a line marker to write after the half
constexpr unsigned EMIT_LINK   = M7 + 01106; // return of emit while writing the externals
constexpr unsigned EXT_NEXT    = M7 + 0166;  // the external entry written next
constexpr unsigned HALF        = M7 + 036;   // the half being written
constexpr unsigned TEMPLATE_AT = 02007;      // the template half to emit next

constexpr unsigned MAIN_LOOP     = 023625;
constexpr unsigned READ_DISPATCH = 023752;
constexpr unsigned DISPATCH_NEXT = 023753;
constexpr unsigned TEMPLATE      = 023734; // the template at M11, returning to M12
constexpr unsigned OPERAND_DONE  = 023725;
constexpr unsigned BLOCK_ENTRY   = 025525;
constexpr unsigned ISO_TO_TEXT   = 030034;
constexpr unsigned DISPATCH      = 026056; // per code: UZA handler / UJ handler

// Write A; return to M13.
unsigned write_return(Machine &m)
{
    unsigned link = m.M[13];
    m.call(WRITE, 13, link);
    return link;
}

//
// 23655: emit the half-word in A (through M13). 7403.0000 is dropped. After
// 0200.0000 (EMIT_MODE = 1) a half that does not fit the word in progress
// first gets a 0200.0000 written before it (tables at M7+130, M7+131 by
// opcode). A line marker (LINE) is written after the half, preceded by the
// numbers of the external names in the table (written once each).
//
unsigned emit(Machine &m)
{
    m.atx(Machine::SP, 0);
    m.aex(7, 02510);
    if (!m.u1a()) {
        m.utm(Machine::SP, 077777);
        return m.M[13];
    }
    m.xta(0, EMIT_MODE);
    if (m.u1a()) {
        m.xta(Machine::SP, 077777);
        m.aax(7, 02446);
        if (m.u1a()) {
            m.xta(Machine::SP, 077777);
            m.asn(0, 0117);
            m.aax(7, 02530);
            m.ati(0, 016);
            m.xta(7, 0130);
        } else {
            m.xta(Machine::SP, 077777);
            m.aex(7, 02454);
            if (!m.u1a()) {
                m.stx(0, EMIT_MODE);
                return m.M[13];
            }
            m.xta(Machine::SP, 077777);
            m.asn(0, 0114);
            m.aax(7, 02531);
            m.ati(0, 016);
            m.xta(7, 0131);
        }
        m.asn(14, 021);
        m.ntr(0, 012);
        if (!m.uza()) {
            m.ita(0, 015);
            m.xts(7, 02475);
            m.call(WRITE, 13, 023646);
            m.xta(0, 0);
            m.stx(0, EMIT_MODE);
            m.sti(0, 015);
            return write_return(m);
        }
    } else {
        m.xta(Machine::SP, 077777);
        m.aex(7, 02441);
        if (m.uza()) {
            m.xta(7, 02453);
            m.stx(0, EMIT_MODE);
            return m.M[13];
        }
    }
    m.xta(Machine::SP, 0);
    m.atx(0, HALF);
    m.xta(0, LINE);
    m.aex(0, 0);
    if (m.u1a()) {
        m.xta(0, HALF);
        m.aax(7, 02451);
        m.aex(7, 02512);
        if (!m.u1a()) {
            m.ita(0, 015);
            m.atx(0, EMIT_LINK);
            m.xta(0, HALF);
            m.call(WRITE, 13, 023674);
            for (;;) {
                m.wtc(0, EXT_NEXT);
                m.utc(0, EXT_NAMES + 1);
                m.xta(0, 0);
                m.aax(7, 02506);
                if (!m.uza())
                    break;
                m.xta(0, EXT_NEXT);
                m.arx(7, 02467);
                m.atx(0, EXT_NEXT);
            }
            m.wtc(0, EXT_NEXT);
            m.utc(0, EXT_NAMES + 1);
            m.xta(0, 1);
            m.asn(0, 0117);
            m.aax(7, 02526);
            m.aox(7, 02446);
            m.call(WRITE, 13, 023702);
            m.xta(0, EXT_NEXT);
            m.arx(7, 02467);
            m.atx(0, EXT_NEXT);
            m.xta(7, 02471);
            m.aox(0, LINE);
            m.call(WRITE, 13, 023705);
            m.xta(0, 0);
            m.atx(0, LINE);
            m.wtc(0, EMIT_LINK);
            return m.ea(0, m.field(0));
        }
    }
    m.xta(0, HALF);
    return write_return(m);
}

void emit_half(Machine &m, unsigned link)
{
    m.set_m(13, link);
    emit(m);
}

// 23712: the next half of the template at 02007 (bit 48: the right half),
// through M13.
void template_half(Machine &m, unsigned link)
{
    m.set_m(13, link);
    m.xta(0, TEMPLATE_AT);
    m.ati(0, 016);
    m.arx(0, 0);
    if (m.u1a()) {
        m.xta(14, 0);
        m.asn(0, 0130);
    } else {
        m.xta(14, 0);
        m.aax(7, 02466);
    }
    m.xts(0, TEMPLATE_AT);
    m.arx(7, 02507);
    m.stx(0, TEMPLATE_AT);
}

//
// 23734: emit the template at M11, up to a zero half; through M12. A half
// with address 4444 takes the operand from the stack: its address replaces
// 4444, or, for an operand with bit 48, the operand + 4000000002277777
// is emitted first and the half keeps address 0.
//
unsigned run_template(Machine &m)
{
    m.ita(0, 013);
    m.atx(0, TEMPLATE_AT);
    for (;;) {
        template_half(m, 023736);
        if (m.uza())
            return m.M[12];
        m.atx(Machine::SP, 0);
        m.aax(7, 02451);
        m.aex(7, 02500);
        if (!m.u1a()) {
            m.xta(Machine::SP, 077776);
            m.arx(0, 0);
            if (m.uza()) {
                m.arx(7, 02505);
                emit_half(m, 023747);
                m.xta(Machine::SP, 0);
                m.aax(7, 02460);
                emit_half(m, 023750);
                m.utm(Machine::SP, 077777);
                continue;
            }
            m.xta(Machine::SP, 0);
            m.aax(7, 02460);
            m.aox(Machine::SP, 0);
            m.atx(Machine::SP, 0);
        }
        m.xta(Machine::SP, 0);
        emit_half(m, 023735);
    }
}

//
// The accumulator: M4 ≠ 0 is the stack cell of a value still in A.
// 23721: it is pushed (15 ATX emitted, the cell marked); through M12.
//
unsigned save_acc(Machine &m)
{
    m.utc(12, 0);
    unsigned link = m.field(0);
    if (!m.M[4])
        return link;
    m.xta(7, 02227);
    m.atx(4, 0);
    emit_half(m, 023724);
    m.vtm(4, 0);
    return m.M[12];
}

// 23727 with an operand in A: loaded into A (template 25772), or, with the
// accumulator in use, through the stack (template 25771).
unsigned load_operand(Machine &m)
{
    if (m.M[4]) {
        m.xts(7, 02227);
        m.atx(4, 0);
        m.vtm(4, 0);
        m.vtm(11, 025771);
        return run_template(m);
    }
    m.vtm(11, 025772);
    m.atx(Machine::SP, 0);
    return run_template(m);
}

// 23725: the operand on the stack done: loaded if any (nonzero); M4 = 0.
unsigned operand_done(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.u1a())
        return load_operand(m);
    m.vtm(4, 0);
    return m.M[12];
}

//
// 23752: read an item and go to its handler (return address on the stack);
// 23753 without the return. M1 = code, M2 = attribute (bits 8–12), M3 =
// value; the handler of code c is in the left half of word DISPATCH + c/2
// (UZA, c even) or the right one (UJ).
//
unsigned dispatch_next(Machine &m)
{
    m.call(READ, 13, 023754);
    m.atx(Machine::SP, 0);
    m.aax(7, 02532);
    m.ati(0, 1);
    m.xta(Machine::SP, 077777);
    m.asn(0, 0114);
    m.aax(7, 02451);
    m.sti(0, 3);
    m.asn(0, 0107);
    m.aax(7, 02530);
    m.ati(0, 2);
    m.ita(0, 1);
    m.asn(0, 0101);
    m.ati(0, 016);
    m.yta(0, 0);
    Word w = m.mem[DISPATCH + m.M[14]];
    return unsigned((m.uza() ? w >> 24 : w) & 077777);
}

unsigned read_dispatch(Machine &m)
{
    m.ita(0, 015);
    m.atx(Machine::SP, 0);
    return dispatch_next(m);
}

// 23625: the main loop: 02021 kept on the stack around each item.
unsigned main_loop(Machine &m)
{
    m.xta(0, 02021);
    m.atx(Machine::SP, 0);
    m.set_m(13, 023627);
    return read_dispatch(m);
}

unsigned main_loop_2(Machine &m)
{
    m.set_m(12, 023630);
    return OPERAND_DONE;
}

unsigned main_loop_3(Machine &m)
{
    m.xta(Machine::SP, 0);
    m.atx(0, 02021);
    return MAIN_LOOP;
}

// 23623: the location counter starts after the prologue.
unsigned prologue_done(Machine &m)
{
    m.ita(0, 1);
    m.atx(0, 02020);
    m.arx(7, 02453);
    m.atx(0, 02004);
    return MAIN_LOOP;
}

//
// 23523: init. The return address is pushed; external table entries get
// their numbers (M7+37 counts them), the names of drum 26 zone 0 (06000)
// their marks; then the outer block: the main program (the name at M7+66)
// gets the prologue (template 23612: 7 VTM, 13 VJM A/START, 15 UTM frame),
// a procedure ('CODE', 'ALGOL' module) its name as TEXT.
//
unsigned start(Machine &m)
{
    m.vtm(7, M7);
    m.ita(0, 015);
    m.xts(0, POS);
    m.arx(7, 02524);
    m.atx(0, PAGE);
    m.call(READ, 13, 023526);
    m.xta(7, 02520);
    m.atx(0, POS);
    m.xta(0, 0);
    m.atx(7, 037);
    m.vtm(13, 077775);
    for (;;) {
        m.utm(13, 3);
        m.utc(13, EXT_NAMES + 1);
        m.xta(0, 0);
        if (m.uza())
            break;
        m.aax(7, 02473);
        if (m.uza()) {
            m.xta(0, 0);
            m.atx(7, 037);
            continue;
        }
        m.xta(7, 037);
        m.asn(0, 061);
        m.utc(13, EXT_NAMES + 1);
        m.aox(0, 1);
        m.utc(13, EXT_NAMES + 1);
        m.atx(0, 1);
        m.aax(7, 02526);
        m.arx(7, 037);
        m.atx(7, 037);
    }
    m.vtm(8, NAMES);
    bool more = true;
    do {
        m.utm(8, 077777);
        m.xta(8, 0);
        if (m.uza()) {
            more = false;
            break;
        }
        m.aax(7, 02452);
    } while (m.u1a());
    if (more) {
        m.vtm(13, 3);
        for (;;) {
            m.utc(13, EXT_NAMES);
            m.xta(0, 0);
            if (m.uza())
                break;
            m.utc(13, EXT_NAMES + 1);
            m.xta(0, 0);
            m.asn(0, 0130);
            m.aax(7, 02451);
            m.atx(0, HALF);
            m.utm(13, 3);
            if (m.uza())
                continue;
            do {
                m.utm(8, 077777);
                m.xta(8, 1);
                m.aax(7, 02451);
                m.aex(0, HALF);
            } while (m.u1a());
            m.xta(8, 1);
            m.aax(7, 02502);
            m.atx(8, 1);
        }
    }
    m.xta(0, 0);
    m.atx(0, EXT_NEXT);
    m.xta(0, 02026);
    m.ati(0, 2);
    m.atx(0, 02015);
    m.vtm(14, 05760);
    m.ita(0, 016);
    m.atx(2, 0);
    m.vtm(1, 010000);
    m.vtm(3, 04133);
    m.extracode(070, 7, 02517);
    m.xta(0, 02003);
    m.aex(7, 02525);
    m.arx(7, 02450);
    m.atx(0, 02010);
    m.vtm(4, 0);
    m.xta(0, 0);
    m.atx(0, 02006);
    m.atx(0, EMIT_MODE);
    m.xta(7, 02477);
    m.atx(0, 02021);
    m.xta(2, 2);
    m.aax(7, 02503);
    m.aex(7, 02452);
    if (!m.uza()) {
        m.xta(7, 066);
        m.atx(0, 02005);
        m.xta(7, 02455);
        m.atx(0, 02014);
        m.call(BLOCK_ENTRY, 13, 023603);
        m.xta(0, 02014);
        m.apx(7, 02465);
        m.asn(0, 0105);
        m.aex(2, 0);
        m.atx(2, 0);
        m.xta(0, 02014);
        m.aax(7, 02526);
        m.its(0, 1);
        m.atx(Machine::SP, 0);
        m.vtm(11, 023612);
        m.vtm(12, 023623);
        return TEMPLATE;
    }
    m.xta(7, 02456);
    m.atx(0, 02014);
    m.wtc(2, 1);
    m.utc(0, 02000);
    m.xta(0, 04144);
    m.atx(0, 02006);
    m.call(ISO_TO_TEXT, 13, 023620);
    m.atx(0, 02005);
    m.call(BLOCK_ENTRY, 13, 023621);
    m.ita(0, 1);
    m.asn(0, 050);
    m.aex(2, 1);
    m.atx(2, 1);
    return prologue_done(m);
}

//
// 26152: EOP. The main program ends with UJ A/STOP (template 26160); return
// to the driver.
//
unsigned eop_end(Machine &m)
{
    m.utm(Machine::SP, 077776);
    m.wtc(Machine::SP, 0);
    return m.ea(0, m.field(0));
}

unsigned eop(Machine &m)
{
    m.xta(0, 02005);
    m.aex(7, 066);
    if (m.u1a())
        return eop_end(m);
    m.xta(0, 02020);
    m.atx(Machine::SP, 0);
    m.vtm(11, 026160);
    m.set_m(12, 026156);
    return TEMPLATE;
}

} // namespace

Stream pass4(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx, pass4_code);
    auto &n          = pass.m.natives;
    n[023523]        = start;
    n[023623]        = prologue_done;
    n[MAIN_LOOP]     = main_loop;
    n[023627]        = main_loop_2;
    n[023630]        = main_loop_3;
    n[023655]        = emit;
    n[023721]        = save_acc;
    n[OPERAND_DONE]  = operand_done;
    n[023727]        = load_operand;
    n[TEMPLATE]      = run_template;
    n[READ_DISPATCH] = read_dispatch;
    n[DISPATCH_NEXT] = dispatch_next;
    n[026152]        = eop;
    n[026156]        = eop_end;
    return pass.run(023523, 01633);
}
