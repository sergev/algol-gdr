#include "pass4.h"

#include "translated.h"

unsigned pass4_code(Machine &m, unsigned pc);

namespace {

constexpr unsigned M7          = 023523; // base of the constants: M7+2441… = 26164…
constexpr unsigned READ        = 027154; // the next item of the input stream in A
constexpr unsigned WRITE       = 027126; // A to the output stream
constexpr unsigned PAGE        = 02000;  // page and position of the streams
constexpr unsigned POS         = 02001;
constexpr unsigned LABELS      = 02004;      // the next label number
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

//
// Jumps. LABELS (02004) is the next label number; a forward jump pushes
// its label for the item that defines it.
//

// 24142: emit the jump in A, push its label, next label; on to the main loop.
unsigned jump_ahead(Machine &m)
{
    emit_half(m, 024143);
    m.xta(0, LABELS);
    m.atx(Machine::SP, 0);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    return MAIN_LOOP;
}

// 24137: IF (027): the condition loaded, U1A to a new label.
unsigned if_item(Machine &m)
{
    return operand(m, 024140);
}

unsigned if_item_2(Machine &m)
{
    load(m, 024141);
    m.xta(7, 02302);
    m.aox(0, LABELS);
    return jump_ahead(m);
}

// 24146: compare-and-jump of a string (0134): both operands, template
// 26040, then U1A ahead.
unsigned string_jump(Machine &m)
{
    return operand(m, 024147);
}

unsigned string_jump_2(Machine &m)
{
    return operand(m, 024150);
}

unsigned string_jump_3(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.u1a()) {
        m.xta(Machine::SP, 0);
        if (m.u1a()) {
            m.utm(Machine::SP, 2);
            load(m, 024160);
        } else {
            m.xta(Machine::SP, 1);
            m.atx(Machine::SP, 0);
        }
    }
    m.vtm(11, 026040);
    m.set_m(12, 024152);
    run_template(m);
    m.vtm(4, 0);
    m.xta(7, 02304);
    m.aox(0, LABELS);
    return jump_ahead(m);
}

//
// 24165: compare-and-jump (0130–0133), M11 = a word of two templates (as
// for 24122): the label pushed under the operands; the template jumps to it.
//
unsigned compare_jump(Machine &m)
{
    m.ita(0, 013);
    m.xts(0, LABELS);
    m.atx(Machine::SP, 0);
    return operand(m, 024167);
}

unsigned compare_jump_2(Machine &m)
{
    return operand(m, 024170);
}

unsigned compare_jump_3(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.u1a()) {
        m.xta(Machine::SP, 0);
        if (m.u1a()) {
            m.utm(Machine::SP, 2);
            m.set_m(12, 024205);
            load_operand(m);
            m.utm(Machine::SP, 077776);
        }
        m.xta(Machine::SP, 1);
        m.atx(Machine::SP, 0);
        m.wtc(Machine::SP, 077775);
        m.xta(0, 0);
        m.asn(0, 0130);
        m.ati(0, 013);
    } else {
        m.wtc(Machine::SP, 077775);
        m.xta(0, 0);
        m.ati(0, 013);
    }
    m.vtm(4, 0);
    m.set_m(12, 024174);
    run_template(m);
    m.xta(0, LABELS);
    m.atx(Machine::SP, 077777);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    return MAIN_LOOP;
}

unsigned compare_jump_with(Machine &m, unsigned pair)
{
    m.vtm(11, pair);
    return compare_jump(m);
}

// 24210: define the label in A (template 26051), the value in A.
unsigned define_label(Machine &m)
{
    m.utm(Machine::SP, 077776);
    m.atx(0, LABELS);
    return apply_template(m, 026051);
}

// 24206: the end of an IF statement (053): its label.
unsigned if_end(Machine &m)
{
    m.xta(Machine::SP, 077775);
    return define_label(m);
}

// 24207: the end of a conditional expression (054): the label before.
unsigned if_expression_end(Machine &m)
{
    m.xta(Machine::SP, 077775);
    m.arx(7, 02522);
    return define_label(m);
}

// 24213: ELSE (031): UJ to a new label, then the IF's label defined.
unsigned else_item(Machine &m)
{
    m.utm(Machine::SP, 077776);
    m.xta(7, 02317);
    m.aox(0, LABELS);
    emit_half(m, 024215);
    m.vtm(11, 026051);
    m.set_m(12, 024216);
    run_template(m);
    m.xta(0, LABELS);
    m.atx(Machine::SP, 0);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    return MAIN_LOOP;
}

// 24666: GOTO a label (024): UJ (template 24671).
unsigned goto_label(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    m.atx(Machine::SP, 0);
    m.vtm(11, 024671);
    m.vtm(12, 024016);
    return TEMPLATE;
}

// 24661: an element of a switch (025), the index on the stack: loaded
// (template 24360) unless in A, the half M7+1147 with it, then as 024.
unsigned switch_element(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (!m.uza()) {
        m.atx(Machine::SP, 0);
        m.vtm(11, 024360);
        m.set_m(12, 024664);
        run_template(m);
    }
    m.xta(7, 01147);
    m.aox(Machine::SP, 0);
    emit_half(m, 024666);
    return goto_label(m);
}

// 25010: switch elements start (0143): the label of the table pushed.
unsigned switch_start(Machine &m)
{
    m.xta(0, LABELS);
    m.atx(Machine::SP, 0);
    return DISPATCH_NEXT;
}

// 25012: an element expression (0144): its own label, the expression.
unsigned switch_expression(Machine &m)
{
    m.xta(0, LABELS);
    m.aox(7, 0746);
    emit_half(m, 025014);
    m.xta(0, LABELS);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    return operand(m, 025016);
}

unsigned switch_expression_2(Machine &m)
{
    m.xta(0, LABELS);
    m.arx(7, 02533);
    m.atx(Machine::SP, 077777);
    return DISPATCH_NEXT;
}

// 25020: an element label (0142).
unsigned switch_label(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    m.aox(7, 02317);
    m.atx(Machine::SP, 0);
    return DISPATCH_NEXT;
}

// 25023: the switch (041) with M2 elements: template 25040, then UJ to each
// element (M7+744 after each) from the stack.
unsigned switch_end(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02526);
    m.atx(Machine::SP, 0);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025040);
    m.set_m(12, 025026);
    run_template(m);
    m.ita(0, 2);
    m.aex(7, 02526);
    m.ati(0, 014);
    m.utm(12, 1);
    m.mtj(12, 013);
    do {
        m.utc(Machine::SP, 0);
        m.xta(12, 0);
        emit_half(m, 025033);
        m.xta(7, 0744);
        emit_half(m, 025034);
        m.utm(12, 1);
    } while (m.M[12]);
    m.utc(11, 0);
    m.utm(Machine::SP, 0);
    m.xta(Machine::SP, 0);
    m.atx(0, LABELS);
    return DISPATCH_NEXT;
}

void register_jumps(Machine::Natives &n)
{
    n[024137] = if_item;
    n[024140] = if_item_2;
    n[024146] = string_jump;
    n[024147] = string_jump_2;
    n[024150] = string_jump_3;
    n[024161] = [](Machine &m) { return compare_jump_with(m, 026052); };
    n[024162] = [](Machine &m) { return compare_jump_with(m, 026053); };
    n[024163] = [](Machine &m) { return compare_jump_with(m, 026054); };
    n[024164] = [](Machine &m) { return compare_jump_with(m, 026055); };
    n[024167] = compare_jump_2;
    n[024170] = compare_jump_3;
    n[024206] = if_end;
    n[024207] = if_expression_end;
    n[024213] = else_item;
    n[024661] = switch_element;
    n[024666] = goto_label;
    n[025010] = switch_start;
    n[025012] = switch_expression;
    n[025016] = switch_expression_2;
    n[025020] = switch_label;
    n[025023] = switch_end;
}

//
// 25525: block entry, the D-list at M2 (up to the word at its head): each
// descriptor gets its storage offset from 02014 (the frame size), counted
// up (or down from 02013 for some), or a number counted in M1, or for an
// external or 'CODE' name the number of its entry in the external table
// (its name as TEXT then in 05777-01400+M3, M3 counting). A word without
// bit 47 links to a nested list: it is followed and the place pushed; the inner blocks are
// entered after the list, each from the current size, the frame growing
// to the largest. Returns through M13 (saved on the stack meanwhile).
// The labels are those of the source.
//
unsigned block_entry(Machine &m)
{
    m.vtm(14, 04067);
    m.ita(0, 016);
    m.atx(0, 05777);
    m.vtm(14, 04047);
    m.ita(0, 016);
    m.atx(0, 05773);
    m.ita(0, 015);
    m.xts(0, 02015);
    m.its(0, 2);
    m.xts(0, 0);
    m.xts(2, 0);
    m.aax(7, 02526);
    m.atx(0, 02011);
    m.xta(0, 0);
    m.atx(0, 02012);
    m.xta(2, 0);
    m.asn(0, 0154);
    m.aax(7, 02467);
    m.ati(0, 016);
    m.xta(2, 0);
    m.ntr(0, 013);
    switch (m.M[14]) { // the head's kind
    case 0:
        if (m.u1a())
            goto L2060;
        goto L2035;
    case 1:
        m.atx(0, 02012);
        goto L2041;
    case 2:
        m.xta(2, 0);
        m.asn(0, 0130);
        [[fallthrough]];
    default:
        m.aax(7, 02451);
        m.xts(2, 0);
        m.aax(7, 02457);
        m.atx(2, 0);
        m.arx(0, 0);
        if (m.uza())
            goto L2031;
        m.xta(0, 02014);
        m.ati(0, 016);
        if (m.M[14])
            m.arx(7, 02522);
        goto L2025;
    }
L2025:
    m.xts(2, 0);
    m.aax(7, 02516);
    m.aex(Machine::SP, 077777);
    m.stx(2, 0);
    m.arx(7, 02461);
    m.arx(Machine::SP, 0);
    m.atx(0, 02014);
    goto L2060;
L2031:
    m.ita(0, 1);
    m.asn(0, 050);
    m.aex(2, 0);
    m.atx(2, 0);
    m.utm(1, 1);
    m.xta(0, 02014);
    goto L2025;
L2035:
    m.ita(0, 2);
    m.atx(0, 02015);
    m.xta(0, 02014);
    m.apx(7, 02465);
    m.asn(0, 0105);
    m.aex(2, 0);
    goto L2056;
L2041:
    m.xta(2, 077777);
    m.aax(7, 02447);
    m.asn(0, 0116);
    m.xts(0, 02014);
    m.aax(7, 02472);
    m.arx(7, 02523);
    m.atx(0, 02007);
    m.aex(Machine::SP, 0);
    m.atx(0, 02013);
    m.atx(0, 02014);
    m.ita(0, 2);
    m.atx(0, 02015);
    m.xta(2, 0);
    m.aax(7, 02516);
    m.aex(0, 02007);
    m.atx(2, 0);
    m.xta(2, 077777);
    m.aax(7, 02457);
    m.atx(2, 077777);
    m.xta(2, 0);
    m.aax(7, 02440);
    if (m.uza())
        goto L2060;
L2054: // offset: the next of the frame
    m.xta(2, 0);
    m.aax(7, 02516);
    m.aex(0, 02014);
L2056:
    m.atx(2, 0);
    m.xta(0, 02014);
    m.arx(7, 02453);
    m.atx(0, 02014);
L2060: // the next descriptor
    m.utm(2, 1);
L2061:
    m.ita(0, 2);
    m.aex(0, 02011);
    if (m.uza())
        goto L2150;
    m.xta(2, 0);
    m.aax(7, 02474);
    if (!m.u1a()) { // a link: follow it
        m.ita(0, 2);
        m.xts(2, 0);
        m.ati(0, 2);
        goto L2061;
    }
    m.xta(2, 0);
    m.aax(7, 02462);
    if (m.uza())
        goto L2074;
    m.xta(0, 02013);
L2070: // offset counted down from 02013
    m.arx(7, 02521);
    m.atx(0, 02013);
    m.xta(2, 0);
    m.aax(7, 02516);
    m.aex(0, 02013);
    m.atx(2, 0);
    goto L2060;
L2074: // by its kind (bits 36-38)
    m.xta(2, 0);
    m.asn(0, 0143);
    m.aax(7, 02504);
    m.ati(0, 016);
    m.xta(2, 0);
    m.aax(7, 02102);
    switch (m.M[14]) {
    case 1:
        if (m.uza())
            goto L2137;
        m.xta(0, 02012);
        if (m.uza())
            goto L2054;
        m.xta(0, 02013);
        goto L2070;
    case 2:
        if (m.uza())
            goto L2144;
        goto L2172;
    case 4:
    case 5:
        m.xta(2, 0);
        m.aax(7, 02452);
        [[fallthrough]];
    case 6:
        if (m.uza())
            goto L2144;
        m.wtc(2, 0);
        [[fallthrough]];
    case 7:
        break;
    default:
        throw std::runtime_error("pass 4: descriptor kind " + std::to_string(m.M[14]));
    }
    // An external name: its entry in the table, numbered from M3.
    m.utc(0, 02000);
    m.xta(0, 04144);
    m.atx(7, 036);
    m.vtm(14, 077775);
    for (;;) {
        m.utm(14, 3);
        m.utc(14, EXT_NAMES + 1);
        m.xta(0, 0);
        if (m.uza()) {
            m.xta(7, 036);
            m.call(ISO_TO_TEXT, 13, 025654);
            break;
        }
        m.aax(7, 02473);
        if (m.u1a())
            continue;
        m.utc(14, EXT_NAMES);
        m.xta(0, 0);
        m.aex(7, 036);
        if (m.u1a())
            continue;
        m.ita(0, 3);
        m.asn(0, 050);
        m.utc(14, EXT_NAMES + 1);
        m.aox(0, 0);
        m.utc(14, EXT_NAMES + 1);
        m.atx(0, 0);
        m.xta(7, 036);
        m.call(ISO_TO_TEXT, 13, 025644);
        m.asn(0, 0106);
        m.aox(7, 02464);
        m.atx(7, 036);
        m.vtm(13, 6);
        do {
            m.utm(13, 077772);
            m.xta(7, 02531);
            m.asn(13, 0100);
            m.aax(7, 036);
        } while (m.uza());
        m.xta(7, 02511);
        m.asn(13, 0106);
        m.aox(7, 036);
        break;
    }
    m.atx(3, 076400);
    m.xta(2, 0);
    m.aax(7, 02516);
    m.its(0, 3);
    m.aex(Machine::SP, 0);
    m.atx(2, 0);
    m.utm(3, 1);
    goto L2060;
L2137:
    m.xta(2, 0);
    m.ntr(0, 013);
    if (!m.u1a()) {
        m.xta(2, 0);
        m.aax(7, 02457);
        m.xts(0, 02015);
        m.asn(0, 050);
        m.aex(Machine::SP, 0);
        m.atx(2, 0);
    }
L2144: // numbered from M1
    m.xta(2, 0);
    m.aax(7, 02516);
    m.its(0, 1);
    m.aex(Machine::SP, 0);
    m.atx(2, 0);
    m.utm(1, 1);
    goto L2060;
L2172: // an array: its dope (bounds) in the frame
    m.xta(2, 0);
    m.ntr(0, 013);
    if (m.u1a())
        goto L2054;
    m.asn(0, 0124);
    m.aax(7, 02451);
    m.ati(0, 010);
    m.xta(0, 02014);
    m.apx(7, 02465);
    m.asn(0, 0105);
    m.xts(2, 0);
    m.aax(7, 02457);
    m.aox(Machine::SP, 0);
    m.atx(2, 0);
    m.xta(8, 0);
    m.asn(0, 0144);
    m.arith(0, 02014, 006);
    m.arith(7, 02453, 004);
    m.arx(7, 02463);
    m.aax(7, 02476);
    m.xts(2, 0);
    m.aax(7, 02516);
    m.aox(Machine::SP, 077777);
    m.stx(2, 0);
    m.arx(7, 02453);
    m.xts(8, 0);
    m.asn(0, 0130);
    m.aax(7, 02451);
    m.arx(Machine::SP, 0);
    m.aax(7, 02476);
    m.atx(0, 02014);
    m.aax(7, 02534);
    if (m.uza())
        goto L2060;
    m.xta(8, 0);
    m.asn(0, 0114);
    m.aax(7, 02451);
    m.ati(0, 2);
    m.ati(0, 016);
    m.xta(8, 0);
    m.asn(0, 0107);
    m.aax(7, 02530);
    m.ati(0, 014);
    m.xta(2, 0);
    m.asn(0, 073);
    m.aux(7, 02465);
    m.atx(0, 02014);
    do {
        m.xta(7, 02515);
        m.atx(14, 0);
        m.utm(14, 1);
        m.utm(12, 077777);
    } while (m.M[12]);
    goto L2054;
L2150: // the list done: the inner blocks
    m.xta(0, 02014);
    m.stx(0, 02013);
    for (;;) {
        m.ati(0, 2);
        if (!m.M[2])
            break;
        m.xta(0, 02013);
        m.xts(0, 02014);
        m.xts(0, 02013);
        m.atx(0, 02014);
        m.set_m(13, 025700);
        block_entry(m);
        m.xta(Machine::SP, 0);
        m.ntr(0, 3);
        m.arith(0, 02014, 005);
        if (!m.u1a()) {
            m.arith(0, 02014, 004);
            m.atx(0, 02014);
        }
        m.xta(Machine::SP, 0);
        m.stx(0, 02013);
    }
    m.xta(Machine::SP, 0);
    m.sti(0, 2);
    m.stx(0, 02015);
    m.ati(0, 015);
    m.xta(2, 0);
    m.aax(7, 02470);
    m.aex(7, 02452);
    if (m.u1a())
        return m.M[13];
    m.xta(0, 02014);
    m.apx(7, 02465);
    m.asn(0, 0105);
    m.aex(2, 0);
    m.atx(2, 0);
    return m.M[13];
}

// 24700: the frame reset to the block of the descriptor at M14 (template
// 24705, or 24707 for one by a register); then the next item.
unsigned frame_reset(Machine &m)
{
    m.xta(14, 0);
    m.asn(0, 073);
    m.aux(7, 02465);
    m.xts(14, 0);
    m.ntr(0, 013);
    m.vtm(11, 024705);
    if (!m.u1a())
        m.vtm(11, 024707);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24673: a label definition (d<<12 | 060): 0211.n; a label that may be
// jumped to from an inner block resets the frame.
unsigned label_item(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    m.aox(7, 0746);
    emit_half(m, 024675);
    m.xta(3, 0);
    m.arx(0, 0);
    if (m.u1a())
        return DISPATCH_NEXT;
    m.asn(0, 0130);
    m.ati(0, 016);
    return frame_reset(m);
}

// 24752: the end of a block (037): the frame of the block outside.
unsigned block_end(Machine &m)
{
    m.mtj(3, 016);
    return frame_reset(m);
}

// 24746: a line marker (051) or block begin (036): 0330.n when M2 = 1.
unsigned line_item(Machine &m)
{
    m.utm(2, 077777);
    if (m.M[2])
        return DISPATCH_NEXT;
    m.ita(0, 3);
    m.aex(7, 01226);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

//
// 24711: a procedure (042): its entry (template 24726 with the frame size
// and the number of parameters); 02021 = parameters − 1, 02023 its kind;
// a procedure with a label inside gets M7+1231 with it.
//
unsigned procedure_item(Machine &m)
{
    m.vtm(11, 024726);
    m.xta(3, 0);
    m.asn(0, 0130);
    m.aax(7, 02526);
    m.xts(3, 0);
    m.asn(0, 0124);
    m.aax(7, 02527);
    m.atx(Machine::SP, 0);
    m.arx(7, 02522);
    m.atx(0, 02021);
    m.xta(3, 077777);
    m.aax(7, 02526);
    m.atx(Machine::SP, 0);
    m.set_m(12, 024720);
    run_template(m);
    m.xta(3, 0);
    m.aax(7, 02437);
    m.aex(7, 02437);
    m.atx(0, 02023);
    m.xta(3, 077777);
    m.asn(0, 0130);
    m.aax(7, 02526);
    if (m.uza())
        return MAIN_LOOP;
    m.aox(7, 01231);
    emit_half(m, MAIN_LOOP);
    return MAIN_LOOP;
}

// 24730: the end of a procedure body (062): the return (M7+752, M7+750),
// then 0211 of the label after it (M7+746) or M7+744.
unsigned procedure_end(Machine &m)
{
    m.utm(Machine::SP, 077776);
    m.xta(3, 0);
    m.aax(7, 02440);
    if (!m.uza()) {
        m.xta(3, 0);
        m.aax(7, 02465);
        m.aox(7, 01165);
        emit_half(m, 024734);
    }
    m.xta(3, 0);
    m.aax(7, 02472);
    m.atx(Machine::SP, 0);
    m.asn(0, 0124);
    m.stx(0, 02021);
    m.aox(7, 0752);
    emit_half(m, 024740);
    m.xta(7, 0750);
    emit_half(m, 024741);
    m.xta(3, 077777);
    m.asn(0, 0130);
    m.aax(7, 02526);
    if (m.uza())
        m.xta(7, 0744);
    else
        m.aox(7, 0746);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

void register_blocks(Machine::Natives &n)
{
    n[024673]      = label_item;
    n[024711]      = procedure_item;
    n[024730]      = procedure_end;
    n[024746]      = line_item;
    n[024752]      = block_end;
    n[BLOCK_ENTRY] = block_entry;
}

//
// Arrays. Subscripts are pushed as operands, a constant one (045) as c and
// 0 above it.
//

// 24244: a constant subscript (045).
unsigned constant_subscript(Machine &m)
{
    m.ita(0, 3);
    m.xts(0, 0);
    m.atx(Machine::SP, 0);
    return DISPATCH_NEXT;
}

// 24246: subscript + c (030): c, then the expression (A pushed if in it).
unsigned subscript_plus(Machine &m)
{
    m.ita(0, 3);
    m.atx(Machine::SP, 0);
    return operand(m, 024250);
}

unsigned subscript_plus_2(Machine &m)
{
    m.xta(Machine::SP, 077777);
    if (m.u1a())
        return DISPATCH_NEXT;
    m.vtm(12, DISPATCH_NEXT);
    return save_acc(m);
}

//
// 24324 (24327 without a register for a formal: from 02021): the address
// of an element of the array at M3, M2 subscripts on the stack: 02011 =
// the half that indexes it. Returns M10 + k: 0 all constant (the address
// on the stack), 1 in a register, 2 in A, 3 in a register and on the stack.
//
unsigned subscript(Machine &m, unsigned base, bool counted)
{
    m.set_m(10, base);
    if (counted) {
        m.xta(0, 02021);
        if (m.uza()) {
            m.xta(7, 02300);
        } else {
            m.asn(0, 054);
            m.aox(7, 02266);
        }
    } else {
        m.xta(7, 02300);
    }
    m.atx(0, 02011);
    m.xta(3, 0);
    m.arx(0, 0);
    if (m.u1a()) {
        m.aax(7, 02465);
        m.aox(7, 02241);
        emit_half(m, 024343);
    } else {
        m.aax(7, 02476);
        m.arx(Machine::SP, 077776);
        m.aax(7, 02476);
        m.stx(Machine::SP, 077776);
        m.mtj(10, 013);
        if (!m.uza()) {
            m.atx(Machine::SP, 0);
            m.vtm(11, 024360);
            m.set_m(12, 024337);
            run_template(m);
            m.utc(10, 2);
            m.vtm(11, 0);
        }
        goto next;
    }
    for (;;) {
        // *0620B: the subscript in A or not
        m.xta(Machine::SP, 0);
        if (m.u1a()) {
            m.utm(Machine::SP, 1);
            m.xta(0, 02011);
            emit_half(m, 024350);
            m.vtm(11, 024360);
            m.set_m(12, 024351);
            run_template(m);
            m.xta(0, 02011);
            m.aax(7, 02472);
            m.aox(Machine::SP, 077777);
            m.atx(Machine::SP, 077777);
            m.utc(10, 3);
            m.vtm(11, 0);
        } else {
            m.utc(10, 1);
            m.vtm(11, 0);
        }
    next: // *0615B: the next subscript, if any (template 24360)
        m.utm(2, 077777);
        m.utc(11, 0);
        unsigned done = m.field(0);
        if (!m.M[2])
            return done;
        m.vtm(11, 024360);
        m.set_m(12, 024343);
        run_template(m);
    }
}

// The half for k = 0…3 (24262…24264): 02011, or M7+634 or M7+2243 with it.
unsigned element_half(Machine &m, unsigned k)
{
    switch (k) {
    case 1:
        m.xta(0, 02011);
        return 1;
    case 3:
        m.xta(7, 02243);
        return 1;
    default:
        m.xta(7, 0634);
        return 2;
    }
}

// 24252: an element read (044): the address, loaded unless constant.
unsigned element_read(Machine &m)
{
    unsigned k = subscript(m, 024253, true) - 024253;
    m.xta(Machine::SP, 0);
    if (k == 0)
        return give(m);
    m.vtm(12, 024015);
    return load_operand(m);
}

//
// 24260: an element stored (046): its address, A/SUBCH in check mode
// (template 24303), then the assignment through a register counted by
// 02021 (or pushed).
//
unsigned element_store(Machine &m)
{
    unsigned k = subscript(m, 024261, true) - 024261;
    if (k == 0) {
        m.xta(Machine::SP, 0);
        return assign(m);
    }
    if (element_half(m, k) == 2) {
        m.aox(Machine::SP, 0);
        emit_half(m, 024307);
        m.xta(0, 02011);
    } else {
        m.aox(Machine::SP, 0);
    }
    emit_half(m, 024266);
    m.xta(0, TranslatedPass::CHECKFL);
    if (!m.uza()) {
        m.xta(0, 02021);
        if (!m.uza()) {
            m.asn(0, 054);
            m.aox(7, 0562);
            emit_half(m, 024272);
        }
        m.xta(3, 0);
        m.arx(0, 0);
        if (!m.u1a()) {
            m.asn(0, 073);
            m.aux(7, 02465);
        }
        m.aax(7, 02465);
        m.atx(Machine::SP, 0);
        m.vtm(11, 024303);
        m.set_m(12, 024277);
        run_template(m);
    }
    m.xta(0, 02021);
    if (m.uza())
        return assign_pushed(m);
    m.ntr(0, 3);
    m.arith(7, 02453, 005);
    m.atx(0, 02021);
    m.xta(0, 02011);
    m.aax(7, 02472);
    return assign(m);
}

// 24311: the address of an element (0103), loaded (M7+1131).
unsigned element_address(Machine &m)
{
    unsigned k = subscript(m, 024312, false) - 024312;
    if (element_half(m, k) == 2) {
        m.aox(Machine::SP, 0);
        emit_half(m, 024322);
        m.xta(0, 02011);
    } else {
        m.aox(Machine::SP, 0);
    }
    emit_half(m, 024317);
    m.xta(7, 01131);
    m.atx(Machine::SP, 0);
    m.vtm(12, 024015);
    return OPERAND_DONE;
}

//
// 24616: an array segment (040) of the array at M3: its routine (4021,
// 4022 by kind; or for a formal 4131 with the number of its external entry
// in LINE), then each bound loaded and pushed (M4 = 02017) until 0104.
//
unsigned array_segment(Machine &m)
{
    m.xta(3, 0);
    m.ita(0, 3);
    m.atx(0, HALF);
    m.vtm(14, 0);
    bool found = false;
    for (;;) {
        m.utc(14, EXT_NAMES + 1);
        m.xta(0, 0);
        m.utm(14, 3);
        if (m.uza())
            break;
        m.asn(0, 0144);
        if (m.uza()) {
            m.utc(14, EXT_NAMES - 2);
            m.xta(0, 0);
            m.asn(0, 0130);
            m.atx(0, EMIT_LINK);
            continue;
        }
        m.aex(0, HALF);
        if (m.u1a())
            continue;
        m.vtm(14, 04131);
        m.xta(0, EMIT_LINK);
        m.atx(0, LINE);
        found = true;
        break;
    }
    if (!found) {
        m.xta(3, 0);
        m.vtm(14, 04021);
        m.ntr(0, 013);
        if (!m.u1a())
            m.vtm(14, 04022);
    }
    m.ita(0, 016);
    m.xts(3, 0);
    m.ntr(0, 013);
    if (!m.u1a()) {
        m.asn(0, 073);
        m.aux(7, 02465);
    }
    m.aax(7, 02465);
    m.its(0, 2);
    m.atx(Machine::SP, 0);
    return operand(m, 024644);
}

unsigned bound(Machine &m)
{
    load(m, 024645);
    m.vtm(4, 02017);
    return operand(m, 024644);
}

// 24646: the bounds end (0104): the routine call (template 24651).
unsigned bounds_end(Machine &m)
{
    m.ita(0, 2);
    m.atx(Machine::SP, 077777);
    m.vtm(11, 024651);
    m.vtm(4, 0);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24655: a block with arrays (0102): M7+674, then the frame of its
// descriptor.
unsigned array_block(Machine &m)
{
    m.xta(7, 0674);
    emit_half(m, 024656);
    m.xta(3, 0);
    m.asn(0, 073);
    m.aux(7, 02465);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

//
// Items made by pass 3: an element through an index register R (bits
// 8-12) at offset d (M3).
//

// 25363: 0161: d + 02027.
unsigned register_item(Machine &m)
{
    m.ita(0, 3);
    m.arx(0, 02027);
    return give(m);
}

// R<<20 + d.
void by_register(Machine &m)
{
    m.ita(0, 2);
    m.asn(0, 054);
    m.its(0, 3);
    m.arx(Machine::SP, 0);
}

// 25365: an element read through R (0163).
unsigned register_read(Machine &m)
{
    by_register(m);
    return give(m);
}

// 25375: an element stored through R (0164).
unsigned register_store(Machine &m)
{
    by_register(m);
    return assign(m);
}

// 25370: 0160: d + 02027 loaded, the half kept.
unsigned register_value(Machine &m)
{
    m.ita(0, 3);
    m.arx(0, 02027);
    m.atx(Machine::SP, 0);
    return operand(m, 025372);
}

unsigned register_value_2(Machine &m)
{
    load(m, 025373);
    m.xta(Machine::SP, 0);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

// 25400: step R by c (0165): UTM c, or for c = 0 template 25410.
unsigned register_step(Machine &m)
{
    if (m.M[3]) {
        by_register(m);
        m.aox(7, 02243);
        emit_half(m, DISPATCH_NEXT);
        return DISPATCH_NEXT;
    }
    m.ita(0, 2);
    m.asn(0, 054);
    m.its(0, 2);
    m.arx(7, 02444);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025410);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 25412: the preamble of an inner loop (0167): the control variable, the
// initial value, the step and the until value; template 25441 (or 25447
// when the step is not 1).
unsigned preamble(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    m.atx(Machine::SP, 0);
    return operand(m, 025414);
}

unsigned preamble_2(Machine &m)
{
    return operand(m, 025415);
}

unsigned preamble_3(Machine &m)
{
    return operand(m, 025416);
}

unsigned preamble_4(Machine &m)
{
    m.utc(Machine::SP, 077774);
    m.vtm(14, 0);
    m.vtm(11, 025441);
    m.xta(0, LABELS);
    m.arx(7, 02461);
    m.wtc(0, 02031);
    m.xts(0, 0);
    m.aax(7, 02445);
    if (!m.u1a()) {
        m.vtm(11, 025447);
        m.xta(14, 2);
        m.atx(Machine::SP, 0);
    }
    m.xta(14, 3);
    m.xts(14, 0);
    m.xts(0, LABELS);
    m.xts(14, 2);
    m.xts(14, 0);
    m.xts(0, LABELS);
    m.arx(7, 02453);
    m.xts(0, LABELS);
    m.xts(14, 1);
    m.atx(Machine::SP, 0);
    m.set_m(12, 025432);
    run_template(m);
    m.utm(Machine::SP, 077774);
    return DISPATCH_NEXT;
}

// 25433: the end of a preamble (0166): template 25455 with the count of
// registers in M2.
unsigned preamble_end(Machine &m)
{
    m.xta(0, LABELS);
    m.arx(7, 02453);
    m.its(0, 2);
    m.ntr(0, 3);
    m.arith(7, 02453, 006);
    m.aax(7, 02451);
    m.xts(0, LABELS);
    m.arx(7, 02461);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025455);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 25460: load a register with the address of an element (0162): the
// element item read here; then 02021 = 1.
unsigned register_load(Machine &m)
{
    m.call(READ, 13, 025461);
    m.asn(0, 0107);
    m.aax(7, 02530);
    m.atx(0, 02021);
    unsigned k = subscript(m, 025463, true) - 025463;
    if (element_half(m, k) == 2) {
        m.aox(Machine::SP, 0);
        emit_half(m, 025473);
        m.xta(0, 02011);
    } else {
        m.aox(Machine::SP, 0);
    }
    emit_half(m, 025470);
    m.xta(7, 02453);
    m.atx(0, 02021);
    return DISPATCH_NEXT;
}

// 25475: a FOR loop with registers (from 25134): its control variable,
// the next item, the elements; template 25522 (or 25516 with A bit
// 0400000000).
unsigned register_loop(Machine &m)
{
    m.aax(7, 02437);
    if (m.uza())
        return operand(m, 025507);
    return operand(m, 025477);
}

unsigned register_loop_2(Machine &m)
{
    m.call(READ, 13, 025500);
    return operand(m, 025501);
}

unsigned register_loop_3(Machine &m)
{
    m.xta(Machine::SP, 077764);
    m.xts(Machine::SP, 077764);
    m.xts(Machine::SP, 077775);
    m.xts(Machine::SP, 077773);
    m.vtm(11, 025522);
    m.atx(Machine::SP, 0);
    m.set_m(12, 025505);
    run_template(m);
    m.utm(Machine::SP, 077776);
    return DISPATCH_NEXT;
}

unsigned register_loop_4(Machine &m)
{
    return operand(m, 025510);
}

unsigned register_loop_5(Machine &m)
{
    return operand(m, 025511);
}

unsigned register_loop_6(Machine &m)
{
    m.xta(Machine::SP, 077763);
    m.xts(Machine::SP, 077763);
    m.xts(Machine::SP, 077774);
    m.xts(Machine::SP, 077774);
    m.xts(Machine::SP, 077771);
    m.vtm(11, 025516);
    m.atx(Machine::SP, 0);
    m.set_m(12, 025515);
    run_template(m);
    m.utm(Machine::SP, 077775);
    return DISPATCH_NEXT;
}

void register_arrays(Machine::Natives &n)
{
    n[024244] = constant_subscript;
    n[024246] = subscript_plus;
    n[024250] = subscript_plus_2;
    n[024252] = element_read;
    n[024260] = element_store;
    n[024311] = element_address;
    n[024616] = array_segment;
    n[024644] = bound;
    n[024646] = bounds_end;
    n[024655] = array_block;
    n[025363] = register_item;
    n[025365] = register_read;
    n[025370] = register_value;
    n[025372] = register_value_2;
    n[025375] = register_store;
    n[025400] = register_step;
    n[025412] = preamble;
    n[025414] = preamble_2;
    n[025415] = preamble_3;
    n[025416] = preamble_4;
    n[025433] = preamble_end;
    n[025460] = register_load;
    n[025475] = register_loop;
    n[025477] = register_loop_2;
    n[025501] = register_loop_3;
    n[025507] = register_loop_4;
    n[025510] = register_loop_5;
    n[025511] = register_loop_6;
}

//
// FOR loops. 055 reserves 10 stack words for the loop (S−10 … S−1 below):
// S−10 L, S−9 L+1, S−8 L+2 its labels; S−7 the control variable's address,
// S−6 the loop descriptor's word, S−5 the control variable (032/0141),
// S−2 the jump to the body after the elements. 02031 = the loop
// descriptor; with registers (bit 0400000 of it, from pass 3) the
// elements go to 25475.
//
constexpr unsigned REGISTER_LOOP = 025475;

unsigned step_in_acc(Machine &m);

// 25042: FOR (055).
unsigned for_item(Machine &m)
{
    m.utm(Machine::SP, 012);
    m.ita(0, 3);
    m.atx(0, 02031);
    m.xta(0, LABELS);
    m.atx(Machine::SP, 077766);
    m.arx(7, 02453);
    m.atx(Machine::SP, 077767);
    m.arx(7, 02453);
    m.atx(Machine::SP, 077770);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    m.xta(0, 0);
    m.atx(Machine::SP, 077776);
    m.xta(3, 0);
    m.asn(0, 0130);
    m.aax(7, 02526);
    m.atx(Machine::SP, 077772);
    m.xta(3, 0);
    m.aax(7, 02465);
    m.atx(Machine::SP, 077771);
    m.arx(7, 02461);
    m.atx(0, 02027);
    m.xta(3, 0);
    m.aax(7, 02442);
    if (!m.uza()) {
        m.xta(0, 02021);
        m.atx(0, 02030);
        m.xta(7, 02453);
        m.atx(0, 02021);
    }
    return DISPATCH_NEXT;
}

// 25062, 25064: the control variable, simple (032) or by name (0141).
unsigned control_variable(Machine &m, bool by_name)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    if (by_name)
        m.aox(7, 02507);
    m.atx(Machine::SP, 077773);
    return DISPATCH_NEXT;
}

// 25067: a by-name element (0135).
unsigned by_name_element(Machine &m)
{
    m.xta(3, 0);
    m.arx(0, 0);
    if (m.u1a())
        return give_none(m);
    return DISPATCH_NEXT;
}

// 25075: the half in A with label L+1, then (unless the descriptor word
// is 0) 0211 of L+2; on to the main loop.
unsigned for_jump(Machine &m)
{
    m.aox(Machine::SP, 077767);
    emit_half(m, 025076);
    m.xta(Machine::SP, 077772);
    if (m.uza())
        return MAIN_LOOP;
    m.aox(7, 0746);
    emit_half(m, MAIN_LOOP);
    return MAIN_LOOP;
}

// 25071: DO (033): the jump to the body (S−2, U1A or UJ).
unsigned do_item(Machine &m)
{
    m.wtc(0, 02031);
    m.xta(0, 0);
    m.aax(7, 02443);
    if (m.u1a())
        return MAIN_LOOP;
    m.xta(Machine::SP, 077776);
    if (!m.uza()) {
        m.aex(7, 02437);
        return for_jump(m);
    }
    m.xta(Machine::SP, 077772);
    if (m.uza())
        return MAIN_LOOP;
    m.xta(7, 02317);
    return for_jump(m);
}

// 25103: the end of a FOR (052): UJ back to L (or by the step), 0211 of
// L+1; the frame dropped, 02004 = L.
unsigned for_end(Machine &m)
{
    m.utm(Machine::SP, 077776);
    m.xta(3, 0);
    m.aax(7, 02443);
    if (m.u1a()) {
        m.xta(Machine::SP, 077766);
        m.aox(7, 01376);
        emit_half(m, 025110);
    } else {
        m.xta(Machine::SP, 077772);
        if (m.u1a()) {
            m.xta(Machine::SP, 077771);
            m.aox(7, 02241);
            emit_half(m, 025124);
            m.xta(0, 0);
        } else {
            m.xta(Machine::SP, 077766);
        }
        m.aox(7, 02317);
        emit_half(m, 025110);
    }
    m.xta(7, 0746);
    m.aox(Machine::SP, 077767);
    emit_half(m, 025112);
    m.xta(Machine::SP, 077766);
    m.atx(0, LABELS);
    m.utm(Machine::SP, 077766);
    m.xta(3, 0);
    m.aax(7, 02442);
    if (!m.uza()) {
        m.xta(0, 02030);
        m.atx(0, 02021);
    }
    return DISPATCH_NEXT;
}

//
// 25132: an element of kind M14: 0 STEP-UNTIL integer (034), 1 real (0140),
// 2 integer constant step (0137), 3 WHILE (035), 4 plain (056). The jump
// of the element before (S−2), the address (template 25174) and 0211 L
// for the first; then the value, and its continuation from the table at
// 25151.
//
unsigned element_item(Machine &m)
{
    m.ita(0, 016);
    m.atx(Machine::SP, 077777);
    m.wtc(0, 02031);
    m.xta(0, 0);
    m.aax(7, 02443);
    if (m.u1a())
        return REGISTER_LOOP;
    m.xta(Machine::SP, 077776);
    if (!m.uza()) {
        m.aox(Machine::SP, 077772);
        emit_half(m, 025137);
    }
    m.xta(Machine::SP, 077772);
    if (!m.uza()) {
        m.xta(Machine::SP, 077771);
        m.xts(Machine::SP, 077765);
        m.atx(Machine::SP, 0);
        m.vtm(11, 025174);
        m.set_m(12, 025143);
        run_template(m);
    }
    m.xta(Machine::SP, 077777);
    m.aex(7, 02467);
    if (!m.u1a()) {
        m.xta(Machine::SP, 077766);
        m.aox(7, 0746);
        emit_half(m, 025146);
    }
    return operand(m, 025147);
}

unsigned element_kind(Machine &m, unsigned kind)
{
    m.vtm(14, kind);
    return element_item(m);
}

unsigned element_value(Machine &m)
{
    m.wtc(Machine::SP, 077776);
    m.wtc(0, 025151);
    m.vtm(12, 0);
    return OPERAND_DONE;
}

// The value stored into the control variable (template 25760).
void store_control(Machine &m, unsigned link)
{
    m.xta(Machine::SP, 077773);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025760);
    m.set_m(12, link);
    run_template(m);
}

// 25156: WHILE: stored; the condition, U1A next.
unsigned while_value(Machine &m)
{
    store_control(m, 025160);
    m.xta(7, 02304);
    m.atx(Machine::SP, 077776);
    return operand(m, 025162);
}

unsigned while_condition(Machine &m)
{
    m.vtm(12, DISPATCH_NEXT);
    return OPERAND_DONE;
}

// 25163: a plain element: stored, UJ to the body, 0211 of L.
unsigned plain_value(Machine &m)
{
    store_control(m, 025165);
    m.xta(Machine::SP, 077772);
    if (m.uza()) {
        m.xta(Machine::SP, 077766);
        m.atx(Machine::SP, 077767);
        return DISPATCH_NEXT;
    }
    m.aox(7, 02317);
    emit_half(m, 025167);
    m.xta(0, 0);
    m.atx(Machine::SP, 077776);
    m.xta(Machine::SP, 077766);
    m.aox(7, 0746);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

// 25206: template M11 with M5 = the kind, returning to 25210 + kind.
unsigned by_kind(Machine &m)
{
    m.ati(0, 5);
    m.utc(5, 025210);
    m.vtm(12, 0);
    return TEMPLATE;
}

// 25176: STEP-UNTIL: the initial value stored (template 25235), then the
// step: a constant one (023, M2 = 0) is kept in S−3; in A: 25247.
unsigned step_value(Machine &m)
{
    m.xta(0, 0);
    m.atx(Machine::SP, 077775);
    return operand(m, 025200);
}

unsigned step_operand(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.uza())
        return step_in_acc(m);
    m.atx(Machine::SP, 077774);
    m.utm(1, 077755);
    if (!m.M[1] && !m.M[2])
        m.atx(Machine::SP, 077775);
    m.vtm(11, 025235);
    m.xts(Machine::SP, 077765);
    m.xts(Machine::SP, 077765);
    m.xts(Machine::SP, 077774);
    return by_kind(m);
}

// 25210…25212 by kind: the increment (templates 25243, 25237, 25241),
// then the until value: compared (template 26020, or 26017 when in A),
// and for a step not constant the sign test (template 25227); U1A to the
// body.
unsigned increment(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.xta(Machine::SP, 077773);
    m.xts(Machine::SP, 077766);
    m.xts(Machine::SP, 077771);
    m.atx(Machine::SP, 0);
    m.set_m(12, 025216);
    run_template(m);
    return operand(m, 025217);
}

unsigned until_test(Machine &m)
{
    m.xta(Machine::SP, 077775);
    if (!m.u1a()) {
        m.xta(Machine::SP, 077774);
        m.atx(Machine::SP, 0);
        m.vtm(11, 025227);
        m.set_m(12, 025225);
        run_template(m);
    }
    m.xta(7, 02302);
    m.atx(Machine::SP, 077776);
    return DISPATCH_NEXT;
}

unsigned until_value(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.uza())
        return operand(m, 025231);
    m.atx(Machine::SP, 0);
    m.vtm(11, 026020);
    m.set_m(12, 025222);
    run_template(m);
    return until_test(m);
}

unsigned until_in_acc(Machine &m)
{
    load(m, 025232);
    m.xta(Machine::SP, 077773);
    m.atx(Machine::SP, 0);
    m.vtm(11, 026017);
    m.vtm(12, 025222);
    return TEMPLATE;
}

// 25247: a step in A: kept in a temporary (template 25265, the until
// after it), then by kind with template 25270.
unsigned step_in_acc(Machine &m)
{
    m.xta(Machine::SP, 077766);
    m.xts(Machine::SP, 077770);
    m.aax(7, 02465);
    m.arx(7, 02453);
    m.xts(Machine::SP, 077765);
    m.xts(Machine::SP, 077770);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025265);
    m.set_m(12, 025254);
    run_template(m);
    return operand(m, 025255);
}

unsigned step_in_acc_2(Machine &m)
{
    load(m, 025256);
    m.xta(Machine::SP, 077770);
    m.xts(Machine::SP, 077766);
    m.xts(Machine::SP, 077771);
    m.xts(Machine::SP, 077766);
    m.aax(7, 02465);
    m.arx(7, 02453);
    m.xts(Machine::SP, 077764);
    m.xts(Machine::SP, 077762);
    m.xts(Machine::SP, 077775);
    m.xts(7, 02227);
    m.atx(Machine::SP, 077765);
    m.xta(Machine::SP, 077770);
    m.vtm(11, 025270);
    return by_kind(m);
}

void register_loops(Machine::Natives &n)
{
    n[025042] = for_item;
    n[025062] = [](Machine &m) { return control_variable(m, false); };
    n[025064] = [](Machine &m) { return control_variable(m, true); };
    n[025067] = by_name_element;
    n[025071] = do_item;
    n[025103] = for_end;
    n[025125] = [](Machine &m) { return element_kind(m, 1); };
    n[025126] = [](Machine &m) { return element_kind(m, 0); };
    n[025127] = [](Machine &m) { return element_kind(m, 2); };
    n[025130] = [](Machine &m) { return element_kind(m, 4); };
    n[025131] = [](Machine &m) { return element_kind(m, 3); };
    n[025147] = element_value;
    n[025156] = while_value;
    n[025162] = while_condition;
    n[025163] = plain_value;
    n[025176] = step_value;
    n[025200] = step_operand;
    n[025210] = [](Machine &m) { return increment(m, 025243); };
    n[025211] = [](Machine &m) { return increment(m, 025237); };
    n[025212] = [](Machine &m) { return increment(m, 025241); };
    n[025217] = until_value;
    n[025222] = until_test;
    n[025231] = until_in_acc;
    n[025255] = step_in_acc_2;
}

//
// Procedures. 02017 is a label still to be defined (0211) before the next
// code: after a thunk, the place its jump around goes.
//

// 24372: define the pending label, if any; through M12.
void pending_label(Machine &m, unsigned link)
{
    m.set_m(12, link);
    m.xta(0, 02017);
    if (m.uza())
        return;
    m.aox(7, 0746);
    emit_half(m, 024374);
    m.xta(0, 02017);
    m.atx(0, LABELS);
    m.xta(0, 0);
    m.atx(0, 02017);
}

// 24361: a call with parameters starts (0110): the frame of the arguments
// (template 24366, or 24370 with A pushed).
unsigned call_start(Machine &m)
{
    m.vtm(11, 024366);
    if (m.M[4]) {
        m.xta(7, 02227);
        m.atx(4, 0);
        m.vtm(4, 0);
        m.vtm(11, 024370);
    }
    m.xta(0, 0);
    m.atx(0, 02017);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24407: the call of the procedure in A: template M11, or 24420 for one
// on the stack; the result in A.
unsigned call_tail(Machine &m)
{
    m.atx(Machine::SP, 0);
    m.aax(7, 02472);
    m.vtm(12, 024015);
    if (!m.uza())
        m.vtm(11, 024420);
    return TEMPLATE;
}

// 24377: a standard function with an expression argument (n<<12 | 057):
// routine 2n (template 24402).
unsigned std_expression(Machine &m)
{
    m.vtm(11, 024402);
    pending_label(m, 024400);
    m.utc(3, 0);
    m.utm(3, 0);
    m.ita(0, 3);
    return call_tail(m);
}

// 24405: the call of a procedure with parameters (020), template 24416.
unsigned call_item_params(Machine &m)
{
    m.vtm(11, 024416);
    pending_label(m, 024406);
    m.xta(3, 0);
    m.aax(7, 02465);
    return call_tail(m);
}

// 24412: the call of a procedure without parameters (021), template 24414.
unsigned call_item_plain(Machine &m)
{
    m.set_m(12, 024413);
    save_acc(m);
    m.vtm(11, 024414);
    m.xta(3, 0);
    m.aax(7, 02465);
    return call_tail(m);
}

// 24426: an argument (A), its template M11.
unsigned argument(Machine &m)
{
    m.atx(Machine::SP, 0);
    pending_label(m, 024427);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24423, 24424: a string (0156) or a constant (0111) argument: its address.
unsigned constant_argument(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.ita(0, 3);
    m.arx(0, 02010);
    return argument(m);
}

// 24430, 24432: a variable (0112), a label (0114) argument.
unsigned variable_argument(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.xta(3, 0);
    m.aax(7, 02465);
    return argument(m);
}

// 24434: an array argument (0115), its address by its descriptor.
unsigned array_argument(Machine &m)
{
    m.vtm(11, 024472);
    m.xta(3, 0);
    m.arx(0, 0);
    if (!m.u1a()) {
        m.asn(0, 073);
        m.aux(7, 02465);
    }
    m.aax(7, 02465);
    return argument(m);
}

// 24440: a procedure argument (0113), with the register count 02021.
unsigned procedure_argument(Machine &m)
{
    m.vtm(11, 024474);
    m.xta(3, 0);
    m.aax(7, 02526);
    m.xts(0, 02021);
    m.asn(0, 054);
    m.aox(Machine::SP, 0);
    return argument(m);
}

// 24444: an expression argument (0120): its value (template 24476).
unsigned expression_argument(Machine &m)
{
    pending_label(m, 024445);
    m.vtm(4, 02017);
    return operand(m, 024446);
}

unsigned expression_argument_2(Machine &m)
{
    load(m, 024447);
    m.vtm(11, 024476);
    m.xta(0, 0);
    m.atx(0, 02017);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24452: a thunk starts (0117): the argument (template 24464) and a jump
// around the thunk's code to a new label, pushed.
unsigned thunk_start(Machine &m)
{
    pending_label(m, 024453);
    m.vtm(11, 024464);
    m.xta(0, 02021);
    m.asn(0, 054);
    m.aox(0, LABELS);
    m.atx(Machine::SP, 0);
    m.set_m(12, 024456);
    run_template(m);
    m.xta(0, LABELS);
    m.atx(Machine::SP, 0);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    return MAIN_LOOP;
}

// 24461: a thunk ends (0116): its return (M7+750); its label pending.
unsigned thunk_end(Machine &m)
{
    m.utm(Machine::SP, 077776);
    m.xta(Machine::SP, 0);
    m.atx(0, 02017);
    m.xta(7, 0750);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

// 24755: a jump around procedure bodies (0146): UJ to a new label, pushed
// twice.
unsigned bodies_start(Machine &m)
{
    m.xta(0, LABELS);
    m.atx(Machine::SP, 0);
    m.atx(Machine::SP, 0);
    m.arx(7, 02453);
    m.atx(0, LABELS);
    m.vtm(11, 024671);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24761: its target (0145): the label defined.
unsigned bodies_end(Machine &m)
{
    m.xta(Machine::SP, 077777);
    m.atx(0, LABELS);
    m.vtm(11, 026051);
    m.vtm(12, DISPATCH_NEXT);
    return TEMPLATE;
}

// 24764, 25005: VALUE ends (0155), starts (0127): for a procedure with
// value parameters (02023 = 0) the half M7+742 or M7+746 with 02004.
unsigned value_mark(Machine &m, unsigned half)
{
    m.xta(0, 02023);
    if (m.u1a())
        return DISPATCH_NEXT;
    m.xta(7, half);
    m.aox(0, LABELS);
    emit_half(m, DISPATCH_NEXT);
    return DISPATCH_NEXT;
}

// 24767, 24776: a value parameter (061), a real one (0154): its descriptor
// marked; the copy (template 25002, 24777) unless 02023 ≠ 0.
unsigned value_parameter(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.xta(3, 0);
    m.arx(7, 02453);
    m.atx(3, 0);
    m.aax(7, 02465);
    m.atx(Machine::SP, 0);
    m.arx(7, 02522);
    m.xts(0, 02023);
    m.vtm(12, DISPATCH_NEXT);
    if (m.uza())
        return TEMPLATE;
    m.utm(Machine::SP, 077776);
    return DISPATCH_NEXT;
}

void register_procedures(Machine::Natives &n)
{
    n[024361] = call_start;
    n[024377] = std_expression;
    n[024405] = call_item_params;
    n[024412] = call_item_plain;
    n[024423] = [](Machine &m) { return constant_argument(m, 024500); };
    n[024424] = [](Machine &m) { return constant_argument(m, 024466); };
    n[024430] = [](Machine &m) { return variable_argument(m, 024470); };
    n[024432] = [](Machine &m) { return variable_argument(m, 024472); };
    n[024434] = array_argument;
    n[024440] = procedure_argument;
    n[024444] = expression_argument;
    n[024446] = expression_argument_2;
    n[024452] = thunk_start;
    n[024461] = thunk_end;
    n[024755] = bodies_start;
    n[024761] = bodies_end;
    n[024764] = [](Machine &m) { return value_mark(m, 0742); };
    n[024767] = [](Machine &m) { return value_parameter(m, 025002); };
    n[024776] = [](Machine &m) { return value_parameter(m, 024777); };
    n[025005] = [](Machine &m) { return value_mark(m, 0746); };
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
    register_jumps(n);
    register_blocks(n);
    register_arrays(n);
    register_loops(n);
    register_procedures(n);
    return pass.run(023523, 01633);
}
