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

//
// Handlers. An item's handler leaves the descriptor of its value on the
// stack (in place of the return address it pops): an address (with
// 074000000: on the stack, bit 48: through a register…), or 0 for a
// value in A, M4 then its stack cell. Operands come first in the stream
// after the operator; each is dispatched through READ_DISPATCH, which
// returns to the continuation in M13.
//

// Load the operand on the stack (23725), through M12 = link.
void load(Machine &m, unsigned link)
{
    m.set_m(12, link);
    operand_done(m);
}

// The template t, then the result in A.
unsigned apply_template(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.vtm(12, 024015);
    return TEMPLATE;
}

// Dispatch the next item, returning to link.
unsigned operand(Machine &m, unsigned link)
{
    m.set_m(13, link);
    return READ_DISPATCH;
}

// 23776: return (the address on the stack) with the descriptor in A in its place.
unsigned give(Machine &m)
{
    m.wtc(Machine::SP, 0);
    m.vtm(14, 0);
    m.atx(Machine::SP, 0);
    return m.M[14];
}

// 24016: no value.
unsigned give_none(Machine &m)
{
    m.xta(0, 0);
    return give(m);
}

// 24015: the value is in A.
unsigned result_in_acc(Machine &m)
{
    m.utc(Machine::SP, 077777);
    m.vtm(4, 0);
    return give_none(m);
}

// 23764: a string (070): its address loaded into A (template 23771, or 23773
// with A pushed).
unsigned string_item(Machine &m)
{
    m.ita(0, 3);
    m.arx(0, 02010);
    m.atx(Machine::SP, 0);
    m.vtm(11, 023771);
    m.vtm(12, 024015);
    if (!m.M[4])
        return TEMPLATE;
    m.vtm(11, 023773);
    m.xta(7, 02227);
    m.atx(4, 0);
    return TEMPLATE;
}

// 23775: a constant (023): its address in the constants.
unsigned constant_item(Machine &m)
{
    m.ita(0, 3);
    m.arx(0, 02010);
    return give(m);
}

// 24000: a variable (017): from its descriptor.
unsigned variable_item(Machine &m)
{
    m.xta(3, 0);
    m.ntr(0, 013);
    if (m.uza()) {
        m.asn(0, 073);
        m.aux(7, 02465);
    } else {
        m.aax(7, 02465);
    }
    return give(m);
}

// 24010: a call without parameters (0107), or a formal by name: A saved,
// the call (template 25747, or 25751 for one on the stack).
unsigned call_item(Machine &m)
{
    m.set_m(12, 024011);
    save_acc(m);
    m.xta(3, 0);
    m.aax(7, 02465);
    m.atx(Machine::SP, 0);
    m.aax(7, 02472);
    if (m.uza())
        return apply_template(m, 025751);
    return apply_template(m, 025747);
}

// 24005: a formal by name (022): a value parameter as a variable.
unsigned formal_item(Machine &m)
{
    m.xta(3, 0);
    m.arx(0, 0);
    if (m.u1a())
        return call_item(m);
    m.aax(7, 02513);
    return give(m);
}

// 24041: a unary operator, template in M11: the operand loaded, then the
// template.
unsigned unary(Machine &m)
{
    m.ita(0, 013);
    m.atx(Machine::SP, 0);
    return operand(m, 024043);
}

unsigned unary_2(Machine &m)
{
    load(m, 024044);
    m.wtc(Machine::SP, 0);
    m.vtm(11, 0);
    m.vtm(12, 024015);
    return TEMPLATE;
}

unsigned unary_with(Machine &m, unsigned t)
{
    m.vtm(11, t);
    return unary(m);
}

// 24035: call the routine on the stack (template 25751).
unsigned routine_call(Machine &m)
{
    return apply_template(m, 025751);
}

// 24030: a standard function n (071): 6 inline (template 25761), else the
// routine 4030+n of its argument.
unsigned std_function(Machine &m)
{
    m.ita(0, 3);
    m.aex(7, 02501);
    if (m.uza())
        return unary_with(m, 025761);
    m.utm(3, 04030);
    m.ita(0, 3);
    m.atx(Machine::SP, 0);
    return operand(m, 024034);
}

unsigned std_function_2(Machine &m)
{
    load(m, 024035);
    return routine_call(m);
}

// 24037: a standard procedure n without arguments (072).
unsigned std_procedure(Machine &m)
{
    m.utm(3, 04030);
    m.ita(0, 3);
    m.atx(Machine::SP, 0);
    return routine_call(m);
}

// 24071: the template at M11, the operand on the stack.
unsigned apply(Machine &m)
{
    m.set_m(12, 024072);
    run_template(m);
    m.utm(Machine::SP, 077777);
    return result_in_acc(m);
}

// The left operand on the stack loaded when neither is in A.
void left_to_acc(Machine &m, unsigned link)
{
    m.xta(Machine::SP, 0);
    if (m.u1a()) {
        m.utm(Machine::SP, 2);
        m.set_m(12, link);
        load_operand(m);
        m.utm(Machine::SP, 077776);
    }
    m.xta(Machine::SP, 1);
    m.atx(Machine::SP, 0);
}

// 24062: a binary operator, template in M11: both operands; the template
// works on A and the operand on the stack.
unsigned binary(Machine &m)
{
    m.ita(0, 013);
    m.atx(Machine::SP, 0);
    return operand(m, 024064);
}

unsigned binary_2(Machine &m)
{
    return operand(m, 024065);
}

unsigned binary_3(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (!m.uza())
        left_to_acc(m, 024074);
    m.wtc(Machine::SP, 077776);
    m.vtm(11, 0);
    return apply(m);
}

unsigned binary_with(Machine &m, unsigned t)
{
    m.vtm(11, t);
    return binary(m);
}

// 24122: an operator not commutative, M11 = a word of two templates: the
// left half for A op stack, the right one when the right operand is in A.
unsigned paired(Machine &m)
{
    m.ita(0, 013);
    m.atx(Machine::SP, 0);
    return operand(m, 024124);
}

unsigned paired_2(Machine &m)
{
    return operand(m, 024125);
}

unsigned paired_3(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.uza()) {
        m.wtc(Machine::SP, 077776);
        m.xta(0, 0);
        m.ati(0, 013);
        return apply(m);
    }
    left_to_acc(m, 024134);
    m.wtc(Machine::SP, 077776);
    m.xta(0, 0);
    m.asn(0, 0130);
    m.ati(0, 013);
    return apply(m);
}

unsigned paired_with(Machine &m, unsigned pair)
{
    m.vtm(11, pair);
    return paired(m);
}

// 24076: real division (016): both operands on the stack: two halves of
// its own (M7+2321, M7+2323).
unsigned divide(Machine &m)
{
    m.vtm(11, 024114);
    m.ita(0, 013);
    m.atx(Machine::SP, 0);
    return operand(m, 024100);
}

unsigned divide_2(Machine &m)
{
    return operand(m, 024101);
}

unsigned divide_3(Machine &m)
{
    m.xta(Machine::SP, 077776);
    m.aex(7, 02472);
    if (m.u1a())
        return paired_3(m);
    m.xta(7, 02321);
    emit_half(m, 024104);
    m.xta(7, 02323);
    emit_half(m, 024105);
    m.utm(Machine::SP, 077775);
    return result_in_acc(m);
}

// 24222: assignment, the target on the stack: the value, then the store
// (template 25760).
unsigned assign(Machine &m)
{
    m.atx(Machine::SP, 0);
    return operand(m, 024223);
}

unsigned assign_2(Machine &m)
{
    load(m, 024224);
    return apply_template(m, 025760);
}

// 24221: to a variable (0100).
unsigned assign_variable(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    return assign(m);
}

// 24240
unsigned assign_pushed(Machine &m)
{
    m.vtm(11, 024242);
    m.set_m(12, 024241);
    run_template(m);
    m.xta(7, 02514);
    return assign(m);
}

//
// 24226: to a formal by name (0101): its address computed (template 25747)
// and kept in a register counted by 02021 from 5 down, or pushed (template
// 24242) when none is left.
//
unsigned assign_formal(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02513);
    m.ntr(0, 013);
    if (m.uza())
        return assign(m);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025747);
    m.set_m(12, 024232);
    run_template(m);
    m.xta(0, 02021);
    if (m.uza())
        return assign_pushed(m);
    m.aox(7, 0520);
    emit_half(m, 024234);
    m.xta(0, 02021);
    m.asn(0, 054);
    m.xts(0, 02021);
    m.ntr(0, 3);
    m.arith(7, 02453, 005);
    m.stx(0, 02021);
    return assign(m);
}

// 25301: x ** c (c on the stack, then a flag: 1 real, 0 integer): c − 1
// multiplications (template 25774), then for an integer 25321.
unsigned power(Machine &m)
{
    m.its(0, 3);
    m.xts(0, 0);
    m.atx(Machine::SP, 0);
    return operand(m, 025303);
}

unsigned power_loop(Machine &m)
{
    for (;;) {
        m.xta(Machine::SP, 077776);
        m.arx(7, 02522);
        if (m.uza())
            break;
        m.atx(Machine::SP, 077776);
        m.xta(Machine::SP, 077777);
        m.atx(Machine::SP, 0);
        m.vtm(11, 025774);
        m.set_m(12, 025306);
        run_template(m);
    }
    m.utm(Machine::SP, 077776);
    m.xta(Machine::SP, 0);
    if (m.u1a())
        return result_in_acc(m);
    return apply_template(m, 025321);
}

unsigned power_2(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.uza()) {
        m.xta(7, 01572);
        m.atx(Machine::SP, 077777);
        emit_half(m, 025306);
        return power_loop(m);
    }
    m.atx(Machine::SP, 077777);
    m.utm(Machine::SP, 1);
    load(m, 025306);
    return power_loop(m);
}

// 25330: an octal base ** c (0352): ASN by c.
unsigned octal_shift(Machine &m)
{
    m.ita(0, 3);
    m.atx(Machine::SP, 0);
    return operand(m, 025332);
}

unsigned octal_shift_2(Machine &m)
{
    load(m, 025333);
    m.xta(Machine::SP, 0);
    m.aox(7, 02245);
    emit_half(m, 024015);
    return result_in_acc(m);
}

// 25276: a real ** c (0152).
unsigned power_real(Machine &m)
{
    m.xta(7, 02453);
    if (m.M[2])
        return octal_shift(m);
    return power(m);
}

// 25300: an integer ** c (0153).
unsigned power_integer(Machine &m)
{
    m.xta(0, 0);
    return power(m);
}

// 25337: a routine of two operands, its address in M14 (pushed): the
// first loaded, M4 = 02017, the second, then the call.
unsigned library_call(Machine &m)
{
    m.ita(0, 016);
    m.atx(Machine::SP, 0);
    return operand(m, 025341);
}

unsigned library_call_2(Machine &m)
{
    load(m, 025342);
    m.vtm(4, 02017);
    return operand(m, 025343);
}

unsigned library_call_3(Machine &m)
{
    load(m, 025344);
    return routine_call(m);
}

// 0150, 0151, 0147 (** by types), 001 (IMPL), 0157 (string relation n).
unsigned library_with(Machine &m, unsigned r, unsigned octal)
{
    m.vtm(14, r);
    if (octal && m.M[2])
        m.vtm(14, octal);
    return library_call(m);
}

unsigned string_relation(Machine &m)
{
    m.utc(3, 077777);
    m.vtm(14, 04070);
    return library_call(m);
}

// 25346: a string slice (050): the string, then the bounds; routine 4100
// (template 25361). M2 ≠ 0: the string pushed first.
unsigned slice(Machine &m)
{
    m.ita(0, 2);
    m.atx(Machine::SP, 0);
    return operand(m, 025350);
}

unsigned slice_3(Machine &m)
{
    m.vtm(4, 02017);
    return operand(m, 025354);
}

unsigned slice_2(Machine &m)
{
    load(m, 025351);
    m.xta(Machine::SP, 0);
    if (m.uza()) {
        m.vtm(4, 02017);
        return operand(m, 025360);
    }
    m.xta(7, 02227);
    emit_half(m, 025353);
    return slice_3(m);
}

unsigned slice_4(Machine &m)
{
    load(m, 025355);
    return apply_template(m, 025361);
}

unsigned slice_5(Machine &m)
{
    load(m, 025353);
    return slice_3(m);
}

void register_operators(Machine::Natives &n)
{
    n[023764] = string_item;
    n[023775] = constant_item;
    n[023776] = give;
    n[024000] = variable_item;
    n[024005] = formal_item;
    n[024010] = call_item;
    n[024015] = result_in_acc;
    n[024016] = give_none;
    n[024021] = [](Machine &m) { return unary_with(m, 025753); };
    n[024022] = [](Machine &m) { return unary_with(m, 025754); };
    n[024023] = [](Machine &m) { return unary_with(m, m.M[2] ? 025767 : 025756); };
    n[024026] = [](Machine &m) { return unary_with(m, 025762); };
    n[024027] = [](Machine &m) { return unary_with(m, 025763); };
    n[024030] = std_function;
    n[024034] = std_function_2;
    n[024037] = std_procedure;
    n[024043] = unary_2;
    n[024046] = [](Machine &m) { return binary_with(m, 025773); };
    n[024047] = [](Machine &m) { return binary_with(m, 025774); };
    n[024050] = [](Machine &m) { return binary_with(m, 025775); };
    n[024051] = [](Machine &m) { return binary_with(m, 025777); };
    n[024052] = [](Machine &m) { return binary_with(m, 026000); };
    n[024053] = [](Machine &m) { return binary_with(m, m.M[2] ? 026016 : 026001); };
    n[024055] = [](Machine &m) { return binary_with(m, 026003); };
    n[024056] = [](Machine &m) { return binary_with(m, 026005); };
    n[024057] = [](Machine &m) { return binary_with(m, 026007); };
    n[024060] = [](Machine &m) { return binary_with(m, 026012); };
    n[024061] = [](Machine &m) { return binary_with(m, 026014); };
    n[024064] = binary_2;
    n[024065] = binary_3;
    n[024075] = [](Machine &m) { return paired_with(m, 024113); };
    n[024076] = divide;
    n[024100] = divide_2;
    n[024101] = divide_3;
    n[024106] = [](Machine &m) { return paired_with(m, 024115); };
    n[024107] = [](Machine &m) { return paired_with(m, 024116); };
    n[024110] = [](Machine &m) { return paired_with(m, 024117); };
    n[024111] = [](Machine &m) { return paired_with(m, 024120); };
    n[024112] = [](Machine &m) { return paired_with(m, 024121); };
    n[024124] = paired_2;
    n[024125] = paired_3;
    n[024221] = assign_variable;
    n[024222] = assign;
    n[024223] = assign_2;
    n[024226] = assign_formal;
    n[024240] = assign_pushed;
    n[025276] = power_real;
    n[025300] = power_integer;
    n[025303] = power_2;
    n[025306] = power_loop;
    n[025323] = [](Machine &m) { return library_with(m, 04023, 04122); };
    n[025325] = [](Machine &m) { return library_with(m, 04024, 0); };
    n[025326] = [](Machine &m) { return library_with(m, 04025, 04121); };
    n[025330] = octal_shift;
    n[025332] = octal_shift_2;
    n[025335] = [](Machine &m) { return library_with(m, 04026, 0); };
    n[025336] = string_relation;
    n[025341] = library_call_2;
    n[025343] = library_call_3;
    n[025346] = slice;
    n[025350] = slice_2;
    n[025353] = slice_3;
    n[025354] = slice_4;
    n[025360] = slice_5;
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
    register_operators(n);
    return pass.run(023523, 01633);
}
