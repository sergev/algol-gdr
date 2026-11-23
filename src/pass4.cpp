#include "pass4.h"

#include "translated.h"

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

constexpr unsigned SP = Machine::SP;

// Constants of the pass (at M7+…).
constexpr Word BIT48      = 0'4000'0000'0000'0000;
constexpr Word BIT47      = 0'0400'0000'0000'0000;
constexpr Word ADDR       = 077777;
constexpr Word ON_STACK   = 074000000; // 15 ATX: the value saved on the stack (M7+2227)
constexpr Word DROPPED    = 074030000; // never emitted
constexpr Word ALIGN      = 0370002;   // the next half starts a word
constexpr Word ALIGN_END  = 0370003;
constexpr Word ALIGN_FILL = 0370006; // written before a half that does not fit
constexpr Word FITS_CMD   = 01014000000;          // by bits 20–16 of a command half (M7+130)
constexpr Word FITS_OP    = 0'0003'7400'0002'7413; // by bits 18–13 of others (M7+131)
constexpr Word EXTERNAL   = 04131;                 // address of a call of an external
constexpr Word EXT_HALF   = 02000000;              // the number of an external name
constexpr Word LINE_HALF  = 067100000;             // a line marker
constexpr Word OPERAND    = 04444;                 // template: the operand's address here
constexpr Word INDIRECT   = 0'4000'0000'0227'7777; // added to an operand with bit 48
constexpr Word ADDR_CLR   = 077770000;
constexpr Word EXT_USED   = 0'7777'0000'0000'0000;
constexpr Word EXT_COUNT  = 0'0000'7777'0000'0000;
constexpr Word MARK_CLR   = 0'3777'7500'0077'7777;
constexpr Word DESC_MASK  = 074077777;
constexpr Word KIND       = 0'3400'0000'0000'0000;
constexpr Word FRAME_MAIN = 030000000;
constexpr Word FRAME_PROC = 034000000;
constexpr Word CONST_BASE = 033777777;
constexpr Word READ_BACK  = 0'7777'7777'7757'7777; // a zone back (cyclic add)
constexpr Word OUT_CURSOR = 0'0000'0302'6020'6000; // 06000, drum 26 zone 2

constexpr unsigned EXT_COUNTER = M7 + 037; // externals numbered so far
constexpr unsigned MAIN_NAME   = M7 + 066; // the name of the main program as TEXT
constexpr unsigned NAMES_INFO  = M7 + 02517; // *70: drum 26 zone 0 to 06000

Word pop(Machine &m)
{
    m.M[SP] = (m.M[SP] - 1) & 077777;
    return m.mem[m.M[SP]];
}

void push(Machine &m, Word w)
{
    m.push(w);
}

void set_acc(Machine &m, Word w)
{
    m.acc = w;
    m.set_logical();
}

Word cyclic_add(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & WORD_MASK;
    return s;
}

// Write w to the output stream (27126), returning to link.
void write(Machine &m, Word w, unsigned link)
{
    m.acc = w;
    m.call(WRITE, 13, link);
}

//
// 23655: emit an instruction half; returns to link. 7403.0000 is dropped.
// 0370002 starts EMIT_MODE: until 0370003, a half that does not fit the
// word in progress (FITS_CMD, FITS_OP by its opcode) first gets a 0370006
// written before it. A line marker (LINE) is written after a call of an
// external (address 4131), preceded by the number of the next external
// name in the table not written yet.
//
void emit(Machine &m, Word half, unsigned link)
{
    if (half == DROPPED) {
        set_acc(m, 0);
        m.set_m(13, link);
        return;
    }
    if (m.mem[EMIT_MODE]) {
        unsigned k;
        Word table;
        if (half & 02000000) {
            k     = (half >> 15) & 037;
            table = FITS_CMD;
        } else if (half == ALIGN_END) {
            m.mem[EMIT_MODE] = 0;
            set_acc(m, half);
            m.set_m(13, link);
            return;
        } else {
            k     = (half >> 12) & 077;
            table = FITS_OP;
        }
        m.set_m(14, k);
        if (k > 47 || !((table >> k) & 1)) {
            write(m, ALIGN_FILL, 023646);
            m.mem[EMIT_MODE] = 0;
            write(m, half, link);
            return;
        }
    } else if (half == ALIGN) {
        m.mem[EMIT_MODE] = 1;
        set_acc(m, half);
        m.set_m(13, link);
        return;
    }
    m.mem[HALF] = half;
    if (m.mem[LINE] && (half & 07777) == EXTERNAL) {
        m.mem[EMIT_LINK] = link;
        write(m, half, 023674);
        unsigned e = m.mem[EXT_NEXT] & 077777;
        while (!(m.mem[(EXT_NAMES + 1 + e) & 077777] & EXT_USED)) {
            m.mem[EXT_NEXT] = cyclic_add(m.mem[EXT_NEXT], 3);
            e               = m.mem[EXT_NEXT] & 077777;
        }
        write(m, ((m.mem[(EXT_NAMES + 2 + e) & 077777] >> 15) & 077777) | EXT_HALF, 023702);
        m.mem[EXT_NEXT] = cyclic_add(m.mem[EXT_NEXT], 3);
        write(m, LINE_HALF | m.mem[LINE], 023705);
        m.mem[LINE] = 0;
        set_acc(m, 0);
        return;
    }
    write(m, half, link);
}

// Emit A, returning to link.
void emit_half(Machine &m, unsigned link)
{
    emit(m, m.acc, link);
}

// 23712: the next half of the template at TEMPLATE_AT (bit 48: the right half).
Word template_half(Machine &m)
{
    Word at = m.mem[TEMPLATE_AT];
    m.set_m(14, at & 077777);
    Word word = m.mem[at & 077777];
    Word half = (at & BIT48) ? word & 077777777 : word >> 24;
    m.mem[TEMPLATE_AT] = cyclic_add(at, BIT48);
    return half;
}

//
// 23734: emit the template at t, up to a zero half. A half with address
// 4444 takes the operand from the stack: its address replaces 4444, or, for
// an operand with bit 48, the operand + 4000000002277777 is emitted first
// and the half keeps address 0.
//
void emit_template(Machine &m, unsigned t)
{
    m.mem[TEMPLATE_AT] = t;
    for (;;) {
        Word half = template_half(m);
        if (!half)
            break;
        if ((half & 07777) == OPERAND) {
            Word op = pop(m);
            if (op & BIT48) {
                emit(m, cyclic_add(op, INDIRECT), 023747);
                emit(m, half & ADDR_CLR, 023750);
                continue;
            }
            half = (half & ADDR_CLR) | op;
        }
        emit(m, half, 023735);
    }
    set_acc(m, 0);
    m.set_m(13, 023736);
}

// 23734 as entered: the template at M11, returning to M12.
unsigned run_template(Machine &m)
{
    emit_template(m, m.M[11]);
    return m.M[12];
}

//
// The accumulator: M4 ≠ 0 is the stack cell of a value still in A.
// 23721: it is saved (15 ATX emitted, the cell marked); returns to M12.
//
unsigned save_acc(Machine &m)
{
    if (m.M[4]) {
        m.mem[m.M[4]] = ON_STACK;
        emit(m, ON_STACK, 023724);
        m.vtm(4, 0);
    }
    return m.M[12];
}

// 23727: the operand d loaded into A (template 25772), or, with the
// accumulator in use, through the stack (template 25771).
void load_value(Machine &m, Word d)
{
    push(m, d);
    unsigned t = 025772;
    if (m.M[4]) {
        m.mem[m.M[4]] = ON_STACK;
        m.vtm(4, 0);
        t = 025771;
    }
    m.vtm(11, t);
    emit_template(m, t);
}

// 23727 as entered: the operand in A, returning to M12.
unsigned load_operand(Machine &m)
{
    load_value(m, m.acc);
    return m.M[12];
}

// 23725: the operand on the stack done: loaded if any (nonzero); M4 = 0.
unsigned operand_done(Machine &m)
{
    Word d = pop(m);
    if (d) {
        load_value(m, d);
    } else {
        set_acc(m, 0);
        m.vtm(4, 0);
    }
    return m.M[12];
}

//
// 23752: read an item and go to its handler (return address on the stack);
// 23753 without the return. M1 = code, M2 = attribute (bits 12–8), M3 =
// value (bits 24–13); the handler of code c is in the left half of word
// DISPATCH + c/2 (c even) or the right one. A has bit 48 for c odd.
//
unsigned dispatch_next(Machine &m)
{
    m.call(READ, 13, 023754);
    Word item = m.acc;
    m.set_m(1, item & 0177);
    m.set_m(2, (item >> 7) & 037);
    m.set_m(3, (item >> 12) & 07777);
    m.set_m(14, m.M[1] >> 1);
    Word w = m.mem[DISPATCH + m.M[14]];
    set_acc(m, Word(m.M[1] & 1) << 47);
    m.rmr = m.acc;
    return unsigned(((m.M[1] & 1) ? w : w >> 24) & 077777);
}

unsigned read_dispatch(Machine &m)
{
    push(m, m.M[13]);
    return dispatch_next(m);
}

// 23625: the main loop: 02021 kept on the stack around each item.
unsigned main_loop(Machine &m)
{
    push(m, m.mem[02021]);
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
    m.mem[02021] = pop(m);
    return MAIN_LOOP;
}

// 23623: the location counter starts after the prologue.
unsigned prologue_done(Machine &m)
{
    m.mem[02020] = m.M[1];
    m.mem[02004] = m.M[1] + 1;
    return MAIN_LOOP;
}

//
// 23523: init. The return address is pushed; external table entries get
// their numbers (EXT_COUNTER counts them), the names of drum 26 zone 0
// (06000) used as externals lose their marks; then the outer block: the
// main program (named MAIN_NAME) gets the prologue (template 23612: 7 VTM,
// 13 VJM A/START, 15 UTM frame), a procedure ('CODE', 'ALGOL' module) its
// name as TEXT.
//
unsigned start(Machine &m)
{
    m.vtm(7, M7);
    push(m, m.M[13]);
    m.mem[PAGE] = cyclic_add(m.mem[POS], READ_BACK);
    m.call(READ, 13, 023526);
    m.mem[POS]         = OUT_CURSOR;
    m.mem[EXT_COUNTER] = 0;
    for (unsigned e = 0;; e += 3) {
        Word w = m.mem[EXT_NAMES + 1 + e];
        if (!w)
            break;
        if (!(w & EXT_COUNT)) {
            m.mem[EXT_COUNTER] = 0;
            continue;
        }
        Word v                   = (m.mem[EXT_COUNTER] << 15) | m.mem[EXT_NAMES + 2 + e];
        m.mem[EXT_NAMES + 2 + e] = v;
        m.mem[EXT_COUNTER]       = cyclic_add(v & ADDR, m.mem[EXT_COUNTER]);
    }
    unsigned p = NAMES;
    bool more  = true;
    do {
        p--;
        if (!m.mem[p]) {
            more = false;
            break;
        }
    } while (m.mem[p] & BIT47);
    if (more) {
        for (unsigned e = 3; m.mem[EXT_NAMES + e]; e += 3) {
            Word name   = (m.mem[EXT_NAMES + 1 + e] >> 24) & 07777;
            m.mem[HALF] = name;
            if (!name)
                continue;
            do
                p--;
            while ((m.mem[p + 1] & 07777) != name);
            m.mem[p + 1] &= MARK_CLR;
        }
    }
    m.vtm(8, p);
    m.mem[EXT_NEXT] = 0;
    Word head       = m.mem[02026];
    m.set_m(2, head);
    m.mem[02015] = head;
    m.vtm(14, 05760);
    m.store(m.M[2], 05760);
    m.vtm(1, 010000);
    m.vtm(3, 04133);
    m.extracode(070, 0, NAMES_INFO);
    m.mem[02010] = cyclic_add(m.mem[02003] ^ WORD_MASK, CONST_BASE);
    m.vtm(4, 0);
    m.mem[02006]     = 0;
    m.mem[EMIT_MODE] = 0;
    m.mem[02021]     = 5;
    if ((m.mem[m.M[2] + 2] & KIND) != BIT47) {
        m.mem[02005] = m.mem[MAIN_NAME];
        m.mem[02014] = FRAME_MAIN;
        m.call(BLOCK_ENTRY, 13, 023603);
        m.acc = m.mem[02014];
        m.apx(0, 026210); // DESC_MASK
        m.asn(0, 0105);
        m.store(m.M[2], m.acc ^ m.mem[m.M[2]]);
        push(m, m.mem[02014] & ADDR);
        push(m, m.M[1]);
        m.vtm(11, 023612);
        m.vtm(12, 023623);
        return TEMPLATE;
    }
    m.mem[02014] = FRAME_PROC;
    m.mem[02006] = m.mem[(06144 + m.mem[m.M[2] + 1]) & 077777];
    m.call(ISO_TO_TEXT, 13, 023620);
    m.mem[02005] = m.acc;
    m.call(BLOCK_ENTRY, 13, 023621);
    m.store(m.M[2] + 1, (Word(m.M[1]) << 24) ^ m.mem[m.M[2] + 1]);
    return prologue_done(m);
}

//
// 26152: EOP. The main program ends with UJ A/STOP (template 26160); return
// to the driver.
//
unsigned eop_end(Machine &m)
{
    pop(m);
    pop(m);
    return pop(m) & 077777;
}

unsigned eop(Machine &m)
{
    if (m.mem[02005] != m.mem[MAIN_NAME])
        return eop_end(m);
    push(m, m.mem[02020]);
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

constexpr Word LABEL_U1A  = 02600000;  // U1A to label n (M7+2302)
constexpr Word LABEL_U1A2 = 02700000;  // (M7+2304)
constexpr Word LABEL_UJ   = 03000000;  // UJ to label n (M7+2317)
constexpr Word LABEL_DEF  = 02100000;  // label n defined here (M7+746)
constexpr Word NEW_WORD   = 02000000;  // (M7+744)
constexpr Word SWITCH_UJ  = 042400000; // (M7+1147)
constexpr Word PREV_LABEL = 02777777;  // added to a label: the one after (M7+2533)
constexpr Word FORMAL     = 0'4000'0000'7407'7777; // a formal's descriptor (M7+2513)
constexpr Word BY_NAME    = 0'4000'0000'7400'0000; // a pushed address (M7+2514)
constexpr Word REG_STORE  = 060440000;             // its address to register n (M7+520)
constexpr Word STACK_ATX1 = 074000001;             // (M7+1572)
constexpr Word ASN        = 0360000;               // (M7+2245)
constexpr Word DIV_1      = 074010001;             // real division, both on the stack
constexpr Word DIV_2      = 074160002;
constexpr Word MINUS_ONE  = 0'7777'7777'7777'7776; // cyclic −1 (M7+2522)

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

// 23776: return (the address on the stack) with the descriptor d in its place.
unsigned give(Machine &m, Word d)
{
    unsigned ret = pop(m) & 077777;
    m.set_m(14, ret);
    push(m, d);
    m.acc = d;
    return ret;
}

// 24016: no value.
unsigned give_none(Machine &m)
{
    set_acc(m, 0);
    return give(m, 0);
}

// 24015: the value is in A, its cell the return address's.
unsigned result_in_acc(Machine &m)
{
    m.set_m(4, m.M[SP] - 1);
    return give_none(m);
}

// The value of item M3 in the constants (02010: their base).
Word constant_address(const Machine &m)
{
    return cyclic_add(m.M[3], m.mem[02010]);
}

// 23764: a string (070): its address loaded into A (template 23771, or 23773
// with A saved).
unsigned string_item(Machine &m)
{
    push(m, constant_address(m));
    m.vtm(11, 023771);
    m.vtm(12, 024015);
    if (m.M[4]) {
        m.vtm(11, 023773);
        m.mem[m.M[4]] = ON_STACK;
    }
    return TEMPLATE;
}

// 23775: a constant (023): its address in the constants.
unsigned constant_item(Machine &m)
{
    return give(m, constant_address(m));
}

// 24000: a variable (017): from its descriptor (bit 48: packed, unpacked).
unsigned variable_item(Machine &m)
{
    Word d = m.mem[m.M[3]];
    m.ntr(0, 013);
    if (d & BIT48)
        d = unpack_bits((d << 5) & WORD_MASK, DESC_MASK);
    else
        d &= DESC_MASK;
    m.set_logical();
    return give(m, d);
}

// 24010: a call without parameters (0107), or a formal by name: A saved,
// the call (template 25747, or 25751 for one on the stack).
unsigned call_item(Machine &m)
{
    m.set_m(12, 024011);
    save_acc(m);
    Word d = m.mem[m.M[3]] & DESC_MASK;
    push(m, d);
    return apply_template(m, (d & ON_STACK) ? 025747 : 025751);
}

// 24005: a formal by name (022): a value parameter as a variable.
unsigned formal_item(Machine &m)
{
    Word d = m.mem[m.M[3]];
    if (!(d & BIT48))
        return call_item(m);
    return give(m, d & FORMAL);
}

// 24041: a unary operator, template t: the operand loaded, then the template.
unsigned unary(Machine &m, unsigned t)
{
    m.vtm(11, t);
    push(m, t);
    return operand(m, 024043);
}

unsigned unary_2(Machine &m)
{
    load(m, 024044);
    m.vtm(11, pop(m));
    m.vtm(12, 024015);
    return TEMPLATE;
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
    if (m.M[3] == 6)
        return unary(m, 025761);
    m.set_m(3, m.M[3] + 04030);
    push(m, m.M[3]);
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
    m.set_m(3, m.M[3] + 04030);
    push(m, m.M[3]);
    return routine_call(m);
}

// 24071: the template t, the operand on the stack; the template's word dropped.
unsigned apply(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.set_m(12, 024072);
    emit_template(m, t);
    pop(m);
    return result_in_acc(m);
}

//
// The right operand d2 popped, the left one on the stack: unless in A, it
// is loaded, and the right one takes its place.
//
void left_to_acc(Machine &m, unsigned link)
{
    Word d1 = pop(m);
    Word d2 = m.mem[(m.M[SP] + 1) & 077777];
    if (d1) {
        m.set_m(SP, m.M[SP] + 2);
        m.set_m(12, link);
        load_value(m, d1);
        m.set_m(SP, m.M[SP] - 2);
    }
    push(m, d2);
}

// The word of the template (or two) under the operand.
Word template_word(const Machine &m)
{
    return m.mem[(m.M[SP] - 2) & 077777];
}

// 24062: a binary operator, template t: both operands; the template works
// on A and the operand on the stack.
unsigned binary(Machine &m, unsigned t)
{
    m.vtm(11, t);
    push(m, t);
    return operand(m, 024064);
}

unsigned binary_2(Machine &m)
{
    return operand(m, 024065);
}

unsigned binary_3(Machine &m)
{
    if (pop(m))
        left_to_acc(m, 024074);
    return apply(m, template_word(m) & 077777);
}

// 24122: an operator not commutative, pair = a word of two templates: the
// left half for A op stack, the right one when the right operand is in A.
unsigned paired(Machine &m, unsigned pair)
{
    m.vtm(11, pair);
    push(m, pair);
    return operand(m, 024124);
}

unsigned paired_2(Machine &m)
{
    return operand(m, 024125);
}

unsigned paired_3(Machine &m)
{
    if (!pop(m))
        return apply(m, m.mem[template_word(m) & 077777] & 077777);
    left_to_acc(m, 024134);
    return apply(m, (m.mem[template_word(m) & 077777] >> 24) & 077777);
}

// 24076: real division (016): both operands on the stack: two halves of
// its own (DIV_1, DIV_2).
unsigned divide(Machine &m)
{
    m.vtm(11, 024114);
    push(m, 024114);
    return operand(m, 024100);
}

unsigned divide_2(Machine &m)
{
    return operand(m, 024101);
}

unsigned divide_3(Machine &m)
{
    if (m.mem[(m.M[SP] - 2) & 077777] != ON_STACK) // the left operand
        return paired_3(m);
    emit(m, DIV_1, 024104);
    emit(m, DIV_2, 024105);
    m.set_m(SP, m.M[SP] - 3);
    return result_in_acc(m);
}

// 24222: assignment to target: the value, then the store (template 25760).
unsigned assign(Machine &m, Word target)
{
    push(m, target);
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
    return assign(m, m.mem[m.M[3]] & DESC_MASK);
}

// 24240: the address computed is pushed (template 24242).
unsigned assign_pushed(Machine &m)
{
    m.vtm(11, 024242);
    m.set_m(12, 024241);
    emit_template(m, 024242);
    return assign(m, BY_NAME);
}

//
// 24226: to a formal by name (0101): its address computed (template 25747)
// and kept in a register counted by 02021 from 5 down, or pushed (template
// 24242) when none is left.
//
unsigned assign_formal(Machine &m)
{
    Word d = m.mem[m.M[3]] & FORMAL;
    m.ntr(0, 013);
    if (d & BIT48)
        return assign(m, d);
    push(m, d);
    m.vtm(11, 025747);
    m.set_m(12, 024232);
    emit_template(m, 025747);
    Word r = m.mem[02021];
    if (!r)
        return assign_pushed(m);
    emit(m, r | REG_STORE, 024234);
    m.ntr(0, 3);
    m.mem[02021] = r - 1;
    m.set_logical();
    return assign(m, (r << 20) & WORD_MASK);
}

// 25301: x ** c with flag (1 real, 0 integer): c and the flag kept on the
// stack, then x: c − 1 multiplications (template 25774), then for an
// integer 25321.
unsigned power(Machine &m, Word flag)
{
    push(m, flag);
    push(m, m.M[3]);
    push(m, 0);
    return operand(m, 025303);
}

unsigned power_loop(Machine &m)
{
    for (;;) {
        Word c = cyclic_add(m.mem[(m.M[SP] - 2) & 077777], MINUS_ONE);
        if (c & BIT48)
            break;
        m.mem[(m.M[SP] - 2) & 077777] = c;
        push(m, m.mem[(m.M[SP] - 1) & 077777]);
        m.vtm(11, 025774);
        m.set_m(12, 025306);
        emit_template(m, 025774);
    }
    m.set_m(SP, m.M[SP] - 2);
    Word flag = pop(m);
    m.set_logical();
    if (flag)
        return result_in_acc(m);
    return apply_template(m, 025321);
}

unsigned power_2(Machine &m)
{
    Word d = pop(m);
    if (!d) {
        m.mem[(m.M[SP] - 1) & 077777] = STACK_ATX1;
        emit(m, STACK_ATX1, 025306);
        return power_loop(m);
    }
    m.mem[(m.M[SP] - 1) & 077777] = d;
    m.set_m(SP, m.M[SP] + 1);
    load(m, 025306);
    return power_loop(m);
}

// 25330: an octal base ** c (0352): ASN by c.
unsigned octal_shift(Machine &m)
{
    push(m, m.M[3]);
    return operand(m, 025332);
}

unsigned octal_shift_2(Machine &m)
{
    load(m, 025333);
    emit(m, pop(m) | ASN, 024015);
    return result_in_acc(m);
}

// 25276: a real ** c (0152).
unsigned power_real(Machine &m)
{
    if (m.M[2])
        return octal_shift(m);
    return power(m, 1);
}

// 25300: an integer ** c (0153).
unsigned power_integer(Machine &m)
{
    return power(m, 0);
}

// 25337: a routine r of two operands (pushed): the first loaded, M4 =
// 02017, the second, then the call.
unsigned library_call(Machine &m, unsigned r)
{
    m.set_m(14, r);
    push(m, r);
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

// 0150, 0151, 0147 (** by types), 001 (IMPL): routine r, or octal for an
// octal operand (M2).
unsigned library_with(Machine &m, unsigned r, unsigned octal)
{
    return library_call(m, octal && m.M[2] ? octal : r);
}

// 0157: string relation n (M3): routine 4067+n.
unsigned string_relation(Machine &m)
{
    return library_call(m, (m.M[3] + 04067) & 077777);
}

// 25346: a string slice (050): the string, then the bounds; routine 4100
// (template 25361). M2 ≠ 0: the string pushed first.
unsigned slice(Machine &m)
{
    push(m, m.M[2]);
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
    if (!pop(m)) {
        m.vtm(4, 02017);
        return operand(m, 025360);
    }
    emit(m, ON_STACK, 025353);
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
    n[023776] = [](Machine &m) { return give(m, m.acc); };
    n[024000] = variable_item;
    n[024005] = formal_item;
    n[024010] = call_item;
    n[024015] = result_in_acc;
    n[024016] = give_none;
    n[024021] = [](Machine &m) { return unary(m, 025753); };
    n[024022] = [](Machine &m) { return unary(m, 025754); };
    n[024023] = [](Machine &m) { return unary(m, m.M[2] ? 025767 : 025756); };
    n[024026] = [](Machine &m) { return unary(m, 025762); };
    n[024027] = [](Machine &m) { return unary(m, 025763); };
    n[024030] = std_function;
    n[024034] = std_function_2;
    n[024037] = std_procedure;
    n[024043] = unary_2;
    n[024046] = [](Machine &m) { return binary(m, 025773); };
    n[024047] = [](Machine &m) { return binary(m, 025774); };
    n[024050] = [](Machine &m) { return binary(m, 025775); };
    n[024051] = [](Machine &m) { return binary(m, 025777); };
    n[024052] = [](Machine &m) { return binary(m, 026000); };
    n[024053] = [](Machine &m) { return binary(m, m.M[2] ? 026016 : 026001); };
    n[024055] = [](Machine &m) { return binary(m, 026003); };
    n[024056] = [](Machine &m) { return binary(m, 026005); };
    n[024057] = [](Machine &m) { return binary(m, 026007); };
    n[024060] = [](Machine &m) { return binary(m, 026012); };
    n[024061] = [](Machine &m) { return binary(m, 026014); };
    n[024064] = binary_2;
    n[024065] = binary_3;
    n[024075] = [](Machine &m) { return paired(m, 024113); };
    n[024076] = divide;
    n[024100] = divide_2;
    n[024101] = divide_3;
    n[024106] = [](Machine &m) { return paired(m, 024115); };
    n[024107] = [](Machine &m) { return paired(m, 024116); };
    n[024110] = [](Machine &m) { return paired(m, 024117); };
    n[024111] = [](Machine &m) { return paired(m, 024120); };
    n[024112] = [](Machine &m) { return paired(m, 024121); };
    n[024124] = paired_2;
    n[024125] = paired_3;
    n[024221] = assign_variable;
    n[024222] = [](Machine &m) { return assign(m, m.acc); };
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

// The next label.
Word new_label(Machine &m)
{
    Word n         = m.mem[LABELS];
    m.mem[LABELS] = cyclic_add(n, 1);
    return n;
}

// 24142: emit the jump half, push its label, next label; on to the main loop.
unsigned jump_ahead(Machine &m, Word half)
{
    emit(m, half, 024143);
    push(m, new_label(m));
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
    return jump_ahead(m, LABEL_U1A | m.mem[LABELS]);
}

// Both operands of a comparison: unless the right one is in A, the left
// one is loaded (link) and the right one takes its place. False when the
// right one is in A.
bool comparison_operands(Machine &m, unsigned link)
{
    Word d2 = pop(m);
    if (!d2)
        return false;
    Word d1 = pop(m);
    if (d1) {
        m.set_m(SP, m.M[SP] + 2);
        m.set_m(12, link);
        load_value(m, d1);
        m.set_m(SP, m.M[SP] - 2);
    }
    push(m, d2);
    return true;
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
    Word d2 = pop(m);
    if (d2) {
        Word d1 = pop(m);
        if (d1) {
            m.set_m(SP, m.M[SP] + 2);
            load(m, 024160);
        } else {
            push(m, d2);
        }
    }
    m.vtm(11, 026040);
    m.set_m(12, 024152);
    emit_template(m, 026040);
    m.vtm(4, 0);
    return jump_ahead(m, LABEL_U1A2 | m.mem[LABELS]);
}

//
// 24165: compare-and-jump (0130–0133), pair = a word of two templates (as
// for 24122): the label pushed under the operands; the template jumps to it.
//
unsigned compare_jump(Machine &m, unsigned pair)
{
    m.vtm(11, pair);
    push(m, pair);
    push(m, m.mem[LABELS]);
    return operand(m, 024167);
}

unsigned compare_jump_2(Machine &m)
{
    return operand(m, 024170);
}

unsigned compare_jump_3(Machine &m)
{
    bool left    = comparison_operands(m, 024205);
    Word pair    = m.mem[m.mem[(m.M[SP] - 3) & 077777] & 077777];
    unsigned t   = (left ? pair >> 24 : pair) & 077777;
    m.vtm(11, t);
    m.vtm(4, 0);
    m.set_m(12, 024174);
    emit_template(m, t);
    m.mem[(m.M[SP] - 1) & 077777] = new_label(m);
    return MAIN_LOOP;
}

// 24210: define label n (template 26051), the value in A.
unsigned define_label(Machine &m, Word n)
{
    m.set_m(SP, m.M[SP] - 2);
    m.mem[LABELS] = n;
    return apply_template(m, 026051);
}

// 24206: the end of an IF statement (053): its label.
unsigned if_end(Machine &m)
{
    return define_label(m, m.mem[(m.M[SP] - 3) & 077777]);
}

// 24207: the end of a conditional expression (054): the label before.
unsigned if_expression_end(Machine &m)
{
    return define_label(m, cyclic_add(m.mem[(m.M[SP] - 3) & 077777], MINUS_ONE));
}

// 24213: ELSE (031): UJ to a new label, then the IF's label defined.
unsigned else_item(Machine &m)
{
    m.set_m(SP, m.M[SP] - 2);
    emit(m, LABEL_UJ | m.mem[LABELS], 024215);
    m.vtm(11, 026051);
    m.set_m(12, 024216);
    emit_template(m, 026051);
    push(m, new_label(m));
    return MAIN_LOOP;
}

// 24666: GOTO a label (024): UJ (template 24671).
unsigned goto_label(Machine &m)
{
    push(m, m.mem[m.M[3]] & DESC_MASK);
    m.vtm(11, 024671);
    m.vtm(12, 024016);
    return TEMPLATE;
}

// 24661: an element of a switch (025), the index on the stack: loaded
// (template 24360) unless in A, the half SWITCH_UJ with it, then as 024.
unsigned switch_element(Machine &m)
{
    Word d = pop(m);
    if (d) {
        push(m, d);
        m.vtm(11, 024360);
        m.set_m(12, 024664);
        emit_template(m, 024360);
    }
    emit(m, SWITCH_UJ | pop(m), 024666);
    return goto_label(m);
}

// 25010: switch elements start (0143): the label of the table pushed.
unsigned switch_start(Machine &m)
{
    push(m, m.mem[LABELS]);
    return DISPATCH_NEXT;
}

// 25012: an element expression (0144): its own label, the expression.
unsigned switch_expression(Machine &m)
{
    emit(m, m.mem[LABELS] | LABEL_DEF, 025014);
    new_label(m);
    return operand(m, 025016);
}

unsigned switch_expression_2(Machine &m)
{
    m.mem[(m.M[SP] - 1) & 077777] = cyclic_add(m.mem[LABELS], PREV_LABEL);
    return DISPATCH_NEXT;
}

// 25020: an element label (0142).
unsigned switch_label(Machine &m)
{
    push(m, (m.mem[m.M[3]] & DESC_MASK) | LABEL_UJ);
    return DISPATCH_NEXT;
}

// 25023: the switch (041) with M2 words on the stack: template 25040, then
// each word and NEW_WORD; the table's label back to LABELS.
unsigned switch_end(Machine &m)
{
    Word d = m.mem[m.M[3]] & ADDR;
    push(m, d);
    push(m, d);
    m.vtm(11, 025040);
    m.set_m(12, 025026);
    emit_template(m, 025040);
    unsigned n    = m.M[2];
    unsigned base = (m.M[SP] - n) & 077777;
    for (unsigned i = 0; i < n; i++) {
        emit(m, m.mem[(base + i) & 077777], 025033);
        emit(m, NEW_WORD, 025034);
    }
    m.set_m(11, -n);
    m.set_m(12, 0);
    m.set_m(SP, base);
    m.mem[LABELS] = pop(m);
    return DISPATCH_NEXT;
}

void register_jumps(Machine::Natives &n)
{
    n[024137] = if_item;
    n[024140] = if_item_2;
    n[024146] = string_jump;
    n[024147] = string_jump_2;
    n[024150] = string_jump_3;
    n[024161] = [](Machine &m) { return compare_jump(m, 026052); };
    n[024162] = [](Machine &m) { return compare_jump(m, 026053); };
    n[024163] = [](Machine &m) { return compare_jump(m, 026054); };
    n[024164] = [](Machine &m) { return compare_jump(m, 026055); };
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
        return give(m, m.acc);
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
        return assign(m, m.acc);
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
    return assign(m, m.acc);
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
    return give(m, m.acc);
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
    return give(m, m.acc);
}

// 25375: an element stored through R (0164).
unsigned register_store(Machine &m)
{
    by_register(m);
    return assign(m, m.acc);
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

//
// Non-ALGOL ('CODE') calls. Each argument pushes its descriptor and the
// address (M8) that passes it later, then the next item; the call
// (0126, 073, 065) passes them in order, then calls. M5 counts arguments
// that are expressions.
//

// 24505: push A and M8, then the next item.
unsigned push_argument(Machine &m)
{
    m.its(0, 010);
    m.atx(Machine::SP, 0);
    return DISPATCH_NEXT;
}

// 24502: no argument count (0121).
unsigned code_start(Machine &m)
{
    m.vtm(5, 0);
    return DISPATCH_NEXT;
}

// 24503: a variable (0122), passed by 24565.
unsigned code_variable(Machine &m)
{
    m.xta(3, 0);
    m.aax(7, 02465);
    m.vtm(8, 024565);
    return push_argument(m);
}

// 24531: a constant (066).
unsigned code_constant(Machine &m)
{
    m.ita(0, 3);
    m.arx(0, 02010);
    m.vtm(8, 024565);
    return push_argument(m);
}

// 24516: an array (0123), passed by 24602.
unsigned code_array(Machine &m)
{
    m.xta(3, 0);
    m.arx(0, 0);
    if (!m.u1a()) {
        m.asn(0, 073);
        m.aux(7, 02465);
    }
    m.aax(7, 02465);
    m.vtm(8, 024602);
    return push_argument(m);
}

// 24513: an expression's value kept in a stack word (M5 counts), passed by
// M8.
unsigned code_value(Machine &m)
{
    m.xta(Machine::SP, 0);
    m.ati(0, 5);
    m.utm(5, 077777);
    m.vtm(4, 02017);
    m.xta(0, 0);
    return push_argument(m);
}

// 24507: an expression (0125), passed by 24603.
unsigned code_expression(Machine &m)
{
    m.ita(0, 5);
    m.atx(Machine::SP, 0);
    return operand(m, 024511);
}

unsigned code_expression_2(Machine &m)
{
    load(m, 024512);
    m.vtm(8, 024603);
    return code_value(m);
}

// 24523: an expression of a non-ALGOL type (0124): in A, as 074's
// result; else its address, passed by 24612.
unsigned code_typed(Machine &m)
{
    m.ita(0, 5);
    m.atx(Machine::SP, 0);
    return operand(m, 024525);
}

unsigned code_result(Machine &m)
{
    m.xta(7, 01023);
    emit_half(m, 024542);
    m.vtm(8, 024607);
    return code_value(m);
}

unsigned code_typed_2(Machine &m)
{
    m.xta(Machine::SP, 0);
    if (m.uza())
        return code_result(m);
    m.aax(7, 02465);
    m.stx(Machine::SP, 1);
    m.ati(0, 5);
    m.xta(Machine::SP, 2);
    m.vtm(8, 024612);
    return push_argument(m);
}

// 24533: a call of an ALGOL procedure as argument (074): routine 4111
// (template 25751), the result passed by 24607.
unsigned code_call_argument(Machine &m)
{
    m.ita(0, 5);
    m.atx(Machine::SP, 0);
    return operand(m, 024535);
}

unsigned code_call_argument_2(Machine &m)
{
    load(m, 024536);
    m.vtm(14, 04111);
    m.ita(0, 016);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025751);
    m.set_m(12, 024542);
    run_template(m);
    m.vtm(8, 024607);
    return code_value(m);
}

//
// 24553: the call, M1 = the routine, A = the count of stack words: the
// arguments are passed in order (each by its M8 address, M6 at its pair,
// M2 counting), then the call (template 25751); the stack is reset and a
// result of M5 words in A.
//
unsigned code_call(Machine &m)
{
    m.aax(7, 02451);
    m.aox(7, 02227);
    m.atx(0, 02023);
    m.set_m(12, 024555);
    save_acc(m);
    m.ita(0, 2);
    m.aex(7, 02526);
    m.ati(0, 010);
    m.utc(8, 1);
    m.utc(8, 1);
    m.utc(Machine::SP, 0);
    m.vtm(6, 0);
    m.mtj(6, 010);
    m.wtc(6, 1);
    return m.ea(0, m.field(0));
}

// 24547: a call of the procedure at M3 (0126).
unsigned code_call_item(Machine &m)
{
    m.wtc(3, 0);
    m.vtm(1, 0);
    m.ita(0, 5);
    return code_call(m);
}

// 24551: a standard routine 4030+n (073).
unsigned code_std_item(Machine &m)
{
    m.utc(3, 04030);
    m.vtm(1, 0);
    m.ita(0, 5);
    return code_call(m);
}

// 24562: (065) the procedure at M3, one fewer.
unsigned code_call_item_2(Machine &m)
{
    m.wtc(3, 0);
    m.vtm(1, 0);
    m.ita(0, 5);
    m.utm(5, 077777);
    return code_call(m);
}

// 24567: pass the argument by template M11, then the next one or the call.
unsigned pass_argument(Machine &m)
{
    m.set_m(12, 024570);
    run_template(m);
    m.xta(0, 02023);
    m.arx(7, 02522);
    m.atx(0, 02023);
    m.utm(6, 2);
    m.utm(2, 077777);
    m.wtc(6, 1);
    unsigned next = m.field(0);
    if (m.M[2])
        return next;
    m.ita(0, 1);
    m.atx(Machine::SP, 0);
    m.vtm(11, 025751);
    m.set_m(12, 024576);
    run_template(m);
    m.mtj(8, 017);
    if (!m.M[5])
        return 024015;
    m.ita(0, 5);
    m.arx(7, 01056);
    emit_half(m, 024015);
    return 024015;
}

// 24566: the argument word at M6 pushed, template M11.
unsigned pass_word(Machine &m, unsigned t)
{
    m.vtm(11, t);
    m.xta(6, 0);
    m.atx(Machine::SP, 0);
    return pass_argument(m);
}

// 24603, 24607: pass a value from its stack word (02023 counting).
unsigned pass_value(Machine &m, bool result)
{
    if (result) {
        m.vtm(11, 025771);
        m.xta(0, 02023);
        m.arx(7, 02522);
        m.xts(0, 02023);
    } else {
        m.vtm(11, 024543);
        m.xta(0, 02023);
        m.atx(Machine::SP, 0);
    }
    m.arx(7, 02453);
    m.atx(0, 02023);
    return pass_argument(m);
}

//
// G30034: the 6 ISO characters in A as TEXT: each Latin letter (a 1 in
// the bit table at 30117/30116) its code − 040, left-justified in 8
// six-bit characters. Through M13; M1–M3 kept.
//
unsigned iso_to_text(Machine &m)
{
    m.xts(0, 0);
    m.its(0, 1);
    m.its(0, 2);
    m.its(0, 3);
    m.atx(Machine::SP, 0);
    m.vtm(14, 0);
    m.vtm(2, 030034);
    do {
        m.xta(Machine::SP, 077773);
        m.utc(0, 030060);
        m.asx(14, 0);
        m.aax(2, 063);
        if (!m.uza()) {
            m.ati(0, 1);
            m.asn(0, 0103);
            m.ati(0, 3);
            m.utc(0, 030076);
            m.xta(3, 0);
            m.its(0, 1);
            m.asn(0, 023);
            m.asn(0, 0155);
            m.sti(0, 3);
            m.utc(0, 030066);
            m.asx(3, 0);
            m.aax(2, 062);
            m.utc(0, 030076);
            m.asx(14, 0);
            m.aox(Machine::SP, 077774);
            m.atx(Machine::SP, 077774);
        }
        m.utm(14, 077773);
        if (!m.M[14])
            break;
        m.utm(14, 6);
    } while (true);
    m.xta(Machine::SP, 0);
    m.sti(0, 3);
    m.sti(0, 2);
    m.sti(0, 1);
    m.utm(Machine::SP, 077777);
    return m.M[13];
}

void register_code_calls(Machine::Natives &n)
{
    n[024502]      = code_start;
    n[024503]      = code_variable;
    n[024507]      = code_expression;
    n[024511]      = code_expression_2;
    n[024516]      = code_array;
    n[024523]      = code_typed;
    n[024525]      = code_typed_2;
    n[024531]      = code_constant;
    n[024533]      = code_call_argument;
    n[024535]      = code_call_argument_2;
    n[024547]      = code_call_item;
    n[024551]      = code_std_item;
    n[024562]      = code_call_item_2;
    n[024565]      = [](Machine &m) { return pass_word(m, 024543); };
    n[024602]      = [](Machine &m) { return pass_word(m, 024545); };
    n[024603]      = [](Machine &m) { return pass_value(m, false); };
    n[024607]      = [](Machine &m) { return pass_value(m, true); };
    n[024612]      = [](Machine &m) { return pass_word(m, 024613); };
    n[ISO_TO_TEXT] = iso_to_text;
}

} // namespace

Stream pass4(Context &ctx, const Stream &input)
{
    // The stream of the previous pass is in the workspace and on the drums,
    // where it left it.
    (void)input;
    TranslatedPass pass(ctx);
    auto &n          = pass.m.natives;
    n[023523]        = start;
    n[023623]        = prologue_done;
    n[MAIN_LOOP]     = main_loop;
    n[023627]        = main_loop_2;
    n[023630]        = main_loop_3;
    n[023655]        = [](Machine &mm) {
        unsigned link = mm.M[13];
        emit(mm, mm.acc, link);
        return link;
    };
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
    register_code_calls(n);
    return pass.run(023523, 01633);
}
