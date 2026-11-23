//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "pass3.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

#include "arith.h"

namespace {

// Cells of the workspace.
constexpr unsigned FREE    = 02003; // the free constant slot; it moves down
constexpr unsigned CUR     = 02012; // the current item
constexpr unsigned CONTROL = 02014; // d of the control variable (032) of the loop scanned
constexpr unsigned LVAR    = 02025; // d of the control variable of the FOR
constexpr unsigned ITEM    = 02032; // an item looked for; the result of intern
constexpr unsigned SET     = 02117; // variables assigned in the loop: from SET−046, 0 ends
constexpr unsigned TABLE   = 02144; // constant slot i at TABLE + i
constexpr unsigned LAST    = 01634; // the constant search ends here (04000)

// The rewrite of a FOR loop: AD02000+n of the source.
constexpr unsigned D14             = 02016; // end of the preamble list (from 02454)
constexpr unsigned D15             = 02017; // end of the body list (from 02702)
constexpr unsigned D16             = 02020; // end of the increments list (from 02120)
constexpr unsigned D17             = 02021; // 0: no register for the step
constexpr unsigned D18             = 02022; // the step's value when a constant
constexpr unsigned D19             = 02023; // 0: the step is not a constant
constexpr unsigned D20             = 02024; // nonzero: a subscript needs the step
constexpr unsigned D23             = 02027; // the item checked by check_operand
constexpr unsigned D24             = 02030; // the step item
constexpr unsigned D25             = 02031; // the next 0160 item: register load, 010000 apart
constexpr unsigned D27             = 02033; // registers taken in the body
constexpr unsigned D28             = 02034; // registers taken in the preamble, from the top
constexpr unsigned D29             = 02035; // c<<12 of a subscript for the increment
constexpr unsigned D30             = 02036; // the initial value item
constexpr unsigned OPND            = 02037; // the descriptor of the operand parsed last
constexpr unsigned D32             = 02040; // the initial value as a 045 item, or 0
constexpr unsigned D33             = 02041; // depth of IF; 077777 after a label of the loop
constexpr unsigned D35             = 02043; // the until item
constexpr unsigned VALUES          = 02143; // the value stack, from VALUES+1
constexpr unsigned PREAMBLE_LIST   = 02454;
constexpr unsigned BODY_LIST       = 02702;
constexpr unsigned INCREMENTS_LIST = 02120;

// Continuations, kept on the stack as return addresses.
constexpr unsigned DRIVER      = 01632;
constexpr unsigned MAIN_LOOP   = 022167;
constexpr unsigned REWRITE     = 022327;
constexpr unsigned STATEMENT   = 022422;
constexpr unsigned AFTER_LABEL = 022516;
constexpr unsigned BINARY_2    = 022523;
constexpr unsigned NO_REG      = 022653;
constexpr unsigned GIVE_UP     = 022662; // no register left: D17 = 0
constexpr unsigned IF_2        = 023076;
constexpr unsigned PRE_REG     = 023164;

constexpr unsigned STACK      = 053401;
constexpr unsigned STACK_SIZE = 010000;

constexpr unsigned RAW  = Arith::NORM_DISABLE | Arith::ROUND_DISABLE; // NTR 3
constexpr unsigned NORM = Arith::ROUND_DISABLE;                       // NTR 2

constexpr Word BIT48     = 0'4000'0000'0000'0000;
constexpr Word NEGATIVE  = 0'0020'0000'0000'0000; // bit 41: a negative number
constexpr Word ADDR      = 077777;
constexpr Word ITEM_MASK = 077777777;
constexpr unsigned SIGN  = 0200; // in the item: a negative constant
constexpr unsigned CONST = 023;  // item code

// An operand's descriptor: bit 48 and VARIABLE flags, the coefficient C
// of the control variable, the first and the last item on the value stack.
constexpr Word NONLINEAR = 0'4000'0000'0000'0000;
constexpr Word VARIABLE  = 0'2000'0000'0000'0000;
constexpr Word FLAGS     = 0'6000'0000'0000'0000;
constexpr Word COEF      = 0'0777'7700'0000'0000;
constexpr Word COEF_ONE  = 0'0000'0100'0000'0000;
constexpr Word FIRST     = 0'0000'0077'7770'0000;
constexpr Word FIRST_ONE = 0'0000'0000'0010'0000;
constexpr Word CONTROL_C = 0'4000'0100'0000'0000; // the control variable: c = 1
constexpr Word INTEGER   = 0'6400'0000'0000'0000; // exponent of an integer

constexpr Word ASSIGNED      = 0'0000'0400'0000'0000; // a descriptor: assigned in the loop
constexpr Word UNMARK        = 0'7777'7377'7777'7777;
constexpr Word STEP_ONE      = 01000000; // the control variable: a constant step
constexpr Word BLOCK_MARK    = 0'1000'0000'0000'0000;
constexpr Word STEP_FLAGS    = 0'7000'0077'7777'7777;
constexpr Word REG_BITS      = 07600;
constexpr Word C_FIELD12     = 077770000;
constexpr Word HEAD_MASK     = 077777600;
constexpr Word C_ONE         = 0'6400'0000'0000'0001; // the integer 1
constexpr Word C_LOW         = 0'6400'0000'0000'3777;
constexpr Word C_HIGH        = 0'6400'0000'0007'7677;
constexpr Word TRUNCATE      = 0'7740'0000'0007'7777;
constexpr Word REG_STEP      = 0100000; // loop flags: step 1, not 1, registers
constexpr Word REG_STEP_N    = 0200000;
constexpr Word REG_LOOP      = 0400000;
constexpr unsigned STD_FIRST = 6; // standard procedures allowed in a loop: 6–8
constexpr unsigned STD_LAST  = 8;

Word cyclic_add(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & WORD_MASK;
    return s;
}

// The arithmetic unit in mode: a + x, x − a, a − x, a · x.
Word unit_add(unsigned mode, Word a, Word x)
{
    Arith u(a, mode);
    u.add(x);
    return u.acc;
}

Word unit_rsub(unsigned mode, Word a, Word x)
{
    Arith u(a, mode);
    u.rsub(x);
    return u.acc;
}

Word unit_sub(unsigned mode, Word a, Word x)
{
    Arith u(a, mode);
    u.sub(x);
    return u.acc;
}

Word unit_mul(unsigned mode, Word a, Word x)
{
    Arith u(a, mode);
    u.mul(x);
    return u.acc;
}

// An integer c × x with the multiplication normalized, then denormalized.
Word integer_product(Word a, Word x)
{
    return unit_add(RAW, unit_mul(NORM, a, x), INTEGER);
}

// Class of each item code in the first scan of a FOR loop (22264, 3 bits
// per code): see scan_loop.
const uint8_t FOR_CLASS[0200] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 000
    3, 3, 3, 0, 0, 3, 7, 0, 0, 0, 1, 0, 0, 3, 3, 3, // 020
    3, 3, 3, 0, 0, 0, 2, 0, 3, 0, 4, 0, 0, 5, 3, 3, // 040
    0, 3, 3, 0, 7, 3, 3, 0, 0, 6, 3, 3, 3, 3, 0, 0, // 060
    2, 3, 3, 3, 3, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 100
    3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 3, 0, 0, // 120
    3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 3, 3, 3, 3, // 140
    4, 1, 1, 0, 6, 3, 0, 0, 2, 1, 0, 2, 0, 1, 0, 0, // 160
};

// Class of each item code in the rewrite (22365, 6 bits per code): the index
// of its handler in classify. Codes 0160… read the words after the table.
const uint8_t PARSE_CLASS[0200] = {
    0, 3, 3,  3,  4,  3,  3,  3,  3,  3,  3,  3,  3,  3, 3,  5,  // 000
    0, 0, 0,  7,  18, 0,  15, 19, 9,  0,  0,  0,  2,  0, 0,  0,  // 020
    0, 0, 0,  3,  10, 8,  11, 4,  0,  0,  1,  21, 21, 0, 0,  0,  // 040
    0, 0, 0,  4,  4,  0,  0,  12, 6,  4,  0,  0,  0,  0, 14, 13, // 060
    0, 0, 0,  0,  0,  3,  3,  0,  0,  0,  0,  0,  0,  0, 0,  0,  // 100
    0, 0, 0,  0,  0,  0,  0,  0,  20, 20, 20, 20, 20, 0, 16, 17, // 120
    0, 0, 0,  0,  0,  0,  0,  3,  3,  3,  4,  4,  0,  0, 0,  0,  // 140
    1, 0, 40, 18, 10, 16, 8,  0,  14, 0,  34, 0,  0,  0, 0,  16, // 160
};

// Index register codes for loop subscripts (23154), from 1: R5, R4, R3, R2,
// R8, R9, R10, R11.
const std::array<Word, 9> REGS = { 0, 01200, 01000, 0600, 0400, 02000, 02200, 02400, 02600 };

//
// Pass 3 (G22162): the optimizer. It copies the stream of pass 2, folding
// negated and converted constants; a FOR loop it can rewrite is copied,
// then parsed again from its start (the parser chained by continuation
// addresses of the original, kept on its stack) and written over the copy:
// the accesses to array elements linear in the control variable go through
// index registers loaded before the loop and stepped at its end.
//
class Pass3 {
public:
    Pass3(Context &ctx, const Stream &input);

    Stream run();

private:
    using Handler = std::function<unsigned()>;

    Context &ctx_;
    std::map<unsigned, Handler> handlers_;

    // Streams: the input, the output, and the scratch of a rewrite. Items
    // are read from one of them; written to the output or the scratch.
    std::vector<Word> in_, out_, scratch_;
    const std::vector<Word> *src_{ &in_ };
    size_t rd_{};
    std::vector<Word> *dst_{ &out_ };
    size_t wr_{};
    size_t start_{};  // the output after the FOR being scanned
    size_t resume_{}; // the input after the loop rewritten

    std::array<Word, STACK_SIZE> stack_{};
    unsigned sp_{ STACK };
    Word zero_{};

    unsigned depth_{}; // 5 − the depth of procedure bodies
    unsigned vs_{};    // the top of the value stack
    unsigned list_{};  // M5: a list word; the 0160 head found
    unsigned moved_{}; // M6: the last word moved
    unsigned first_{}; // M8: the first item of the operand
    unsigned last_{};  // M9: its last item; in element(), subscripts with c ≠ 0
    unsigned subs_{};  // the element's first subscript on the stack, − 1
    unsigned nsubs_{}; // the element's subscripts

    Word &mem(unsigned addr);
    void store(unsigned addr, Word w)
    {
        if (addr)
            mem(addr) = w;
    }
    void push(Word w)
    {
        mem(sp_) = w;
        sp_      = (sp_ + 1) & 077777;
    }
    Word pop()
    {
        sp_ = (sp_ - 1) & 077777;
        return mem(sp_);
    }
    Word &top(unsigned k = 1) { return mem((sp_ - k) & 077777); }

    Word get();
    void put(Word w);

    // Main loop.
    Word intern(Word value);
    unsigned dispatch(Word item);
    unsigned copy_item();
    unsigned procedure_end();
    void fold();
    unsigned eop();
    unsigned for_scan();
    unsigned start();

    // The rewrite.
    unsigned ret() { return pop() & 077777; }
    unsigned read_parse(unsigned link);
    unsigned parse(unsigned link);
    unsigned classify();
    void move_items(Word range);
    void flush_stack();
    void check_operand(Word d);
    void find_items();
    void through_register(Word code);
    Word access_code();
    unsigned access(unsigned link);
    unsigned stepped_access(unsigned link);
    Word load_register(unsigned list, unsigned k);
    Word add_increment(Word w);
    unsigned body_register(Word code, unsigned link);
    unsigned preamble_access();
    unsigned begin_rewrite();
    unsigned initial_parsed();
    unsigned parse_until();
    unsigned step_parsed();
    unsigned until_parsed();
    unsigned label();
    unsigned result(Word a);
    unsigned variable_result();
    unsigned nonlinear_result();
    unsigned unary_2();
    unsigned unary_3(Word a);
    unsigned leaf(Word flags);
    unsigned variable();
    unsigned constant();
    unsigned element_done(Word d);
    unsigned element();
    unsigned int_op_2();
    Word constant_value(Word d);
    unsigned int_op_3();
    unsigned linear_result(Word flags);
    unsigned int_neg_2();
    unsigned if_branches();
    unsigned if_else();
    unsigned end_loop();
    void register_rewrite();
};

Word &Pass3::mem(unsigned addr)
{
    if (Workspace::contains(addr))
        return ctx_.ws[addr];
    if (addr >= STACK && addr < STACK + STACK_SIZE)
        return stack_[addr - STACK];
    if (!addr) {
        zero_ = 0;
        return zero_;
    }
    throw std::runtime_error("pass 3: no data at " + std::to_string(addr));
}

Word Pass3::get()
{
    if (rd_ >= src_->size())
        throw std::runtime_error("pass 3: the stream ends without EOP");
    return (*src_)[rd_++];
}

void Pass3::put(Word w)
{
    w &= ITEM_MASK;
    if (wr_ < dst_->size())
        (*dst_)[wr_] = w;
    else
        dst_->push_back(w);
    wr_++;
}

//
// G22716: intern the constant value: the slot of an equal word among those
// above the free slot, else the free slot. Returns the item slot<<12 | 023
// (in ITEM without the code), with 0200 for a negative value.
//
Word Pass3::intern(Word value)
{
    unsigned n     = mem(FREE) & 077777;
    mem(TABLE + n) = value;
    unsigned slot  = n;
    for (unsigned i = n + 1; i <= LAST; i++)
        if (mem((TABLE + i) & 077777) == value) {
            slot = i;
            break;
        }
    if (slot == n)
        mem(FREE) = Word(n - 1);
    Word item = Word(slot) << 12;
    if (value & NEGATIVE)
        item += SIGN;
    mem(ITEM) = item;
    return item + CONST;
}

// 22201: copy the current item, then the next.
unsigned Pass3::copy_item()
{
    put(mem(CUR));
    return MAIN_LOOP;
}

//
// 22203: after a 146 (end of the procedure declarations, a jump over the
// bodies). Line markers (051) are dropped; a 145 (more declarations) drops
// the 146 too, else 146 and the item are written.
//
unsigned Pass3::procedure_end()
{
    for (;;) {
        Word item  = get();
        mem(CUR)   = item;
        unsigned c = item & 0177;
        if (c == 051)
            continue;
        if (c == 0145)
            return MAIN_LOOP;
        put(0146);
        return dispatch(mem(CUR));
    }
}

//
// 22214: an integer minus (026) or int→real (064) before a constant becomes
// a constant: negated, or converted. Else both items are written.
//
void Pass3::fold()
{
    Word op   = mem(CUR);
    Word item = get();
    mem(CUR)  = item;
    if ((item & 0177) != CONST) {
        put(op);
        return;
    }
    Word v = mem(TABLE + ((item >> 12) & 07777));
    if (op == 064) {
        v = unit_add(NORM, v, 0); // normalized, real
    } else {
        Arith u(v, RAW); // the sign changed
        u.change_sign(true);
        v = u.acc;
    }
    mem(CUR) = intern(v);
}

// 23343: EOP: written, the stream ends; return to the driver.
unsigned Pass3::eop()
{
    put(mem(CUR));
    out_.resize(wr_);
    return pop() & 077777;
}

// 22170: by the code of the item: 146, 075 (EOP), 055 (FOR), 042, 062
// (procedure body: depth), 026, 064 (minus, to real: fold a constant),
// else copy.
unsigned Pass3::dispatch(Word item)
{
    mem(CUR) = item;
    switch (item & 0177) {
    case 0146:
        return procedure_end();
    case 075:
        return eop();
    case 055:
        return for_scan();
    case 042:
        depth_ = (depth_ - 1) & 077777;
        break;
    case 062:
        depth_ = (depth_ + 1) & 077777;
        break;
    case 026:
    case 064:
        fold();
        break;
    }
    return copy_item();
}

//
// 22232: FOR (055 d). First scan: copies the loop, classifying each item;
// gives up (copies the rest through the main loop) on any item it cannot
// rewrite. At the end of the body (052, 0160), the variables assigned in it
// get ASSIGNED in their descriptors, and the rewrite (22327) starts over
// from the output after the FOR. A nested FOR restarts the scan from it.
//
unsigned Pass3::for_scan()
{
    unsigned set; // the next of SET, from SET−046
restart:
    mem(LVAR) = mem(CUR) >> 12;
    if (mem(mem(LVAR) & 077777) & BIT48)
        return copy_item();
    put(mem(CUR));
    start_ = wr_;
    set    = 077732;
    goto read;
copy:
    put(mem(CUR));
read:
    mem(CUR) = get();
classify: {
    unsigned code = unsigned(mem(CUR)) & 0177;
    switch (FOR_CLASS[code]) {
    case 0: // copied
        goto copy;
    case 1: // its control variable (032, …)
        mem(CONTROL) = mem(CUR) >> 12;
        goto copy;
    case 2: // left part (0100, 046, …)
        mem((SET + set) & 077777) = mem(CUR) >> 12;
        if (!set)
            return copy_item(); // too many
        set = (set + 1) & 077777;
        goto copy;
    case 3: // cannot rewrite
        return copy_item();
    case 4: // end of the loop
        break;
    case 5: // a FOR inside
        goto restart;
    case 6: { // standard procedure: only 6–8 (0700 >> n & 1, as ASN by n)
        unsigned n = (mem(CUR) >> 12) & 0177;
        if (n < STD_FIRST || n > STD_LAST)
            return copy_item();
        goto copy;
    }
    default: // 026, 064
        fold();
        goto classify;
    }
}
    // Its control variable must not be assigned in it.
    mem((SET + set) & 077777) = 0;
    for (unsigned i = 0; mem(SET - 046 + i); i++)
        if (mem(SET - 046 + i) == mem(CONTROL))
            return copy_item();
    put(mem(CUR));
    for (unsigned i = 0;; i++) {
        unsigned d = mem(SET - 046 + i) & 077777;
        if (!d)
            break;
        mem(d) |= ASSIGNED;
    }
    resume_ = rd_;
    return REWRITE;
}

// 22162: init: the return address pushed; depth 5.
unsigned Pass3::start()
{
    push(DRIVER);
    depth_ = 5;
    return MAIN_LOOP;
}

//
// The rewrite of a FOR loop (22327…23426). The loop is parsed again from
// its copy in the output, as a tree in prefix order: a handler per item
// class (table at 22365) parses its operands and leaves a descriptor of the
// result in OPND: NONLINEAR, VARIABLE; COEF the coefficient c of the
// control variable, FIRST and the low 15 bits the first and last item of
// the operand on the value stack (vs_, from 02144). parse() pushes the
// continuation, a handler returns by popping it (ret()). Items are written
// to the scratch when an operand is finished (flush_stack); an array access
// linear in the control variable gets an index register (163/164 items),
// its load goes to the preamble list, its step to the increments list;
// both are written after the loop.
//

// 22347: read an item, then parse it.
unsigned Pass3::read_parse(unsigned link)
{
    mem(CUR) = get();
    return parse(link);
}

// 22351: parse the item in CUR, returning to link.
unsigned Pass3::parse(unsigned link)
{
    push(link);
    return classify();
}

//
// 22352: by the class of the item (6 bits per code at 22365): 0 is
// written and the statement goes on; else the item goes on the value stack
// and to its handler.
//
unsigned Pass3::classify()
{
    static const unsigned handlers[] = {
        0,      023231, 022441, 022522, 022541, 022551, 022570, 022572, 022600, 022604, 022607,
        022607, 022664, 022666, 022665, 023417, 022425, 022436, 022507, 023073, 023115, 023123,
    };
    unsigned code = mem(CUR) & 0177;
    unsigned cls  = PARSE_CLASS[code];
    if (!cls) {
        pop();
        put(mem(CUR));
        return read_parse(STATEMENT);
    }
    vs_      = (vs_ + 1) & 077777;
    mem(vs_) = mem(CUR);
    if (cls >= std::size(handlers))
        throw std::runtime_error("pass 3: item " + std::to_string(code));
    return handlers[cls];
}

// 22757: move the nonzero words of [first, last] (range = first<<15 |
// last) to list_…, zeroing them; moved_ = the last one moved.
void Pass3::move_items(Word range)
{
    last_      = range & 077777;
    first_     = (range >> 15) & 077777;
    moved_     = (list_ - 1) & 077777;
    unsigned p = first_;
    for (;;) {
        if (Word w = mem(p)) {
            moved_      = (moved_ + 1) & 077777;
            mem(moved_) = w;
        }
        mem(p) = 0;
        if (p == last_)
            return;
        p = (p + 1) & 077777;
    }
}

// 22747: write the value stack and empty it.
void Pass3::flush_stack()
{
    for (unsigned p = TABLE; p <= vs_; p++)
        if (Word w = mem(p))
            put(w);
    vs_ = VALUES;
}

//
// 22771: the operand d is done. A single item (the last = the first) is
// kept in D23 (0 if it is the control variable); a nonlinear operand or one
// in an IF is left on the stack; else its items move to the body list after
// a register load (0160 item), and D23 = the load.
//
void Pass3::check_operand(Word d)
{
    if ((d & FLAGS) == NONLINEAR)
        mem(D17) = 0;
    if (!(((d >> 15) ^ d) & ADDR)) {
        Word item = mem(d & ADDR);
        mem(D23)  = item;
        if ((item >> 12) != mem(CONTROL))
            return;
    }
    mem(D23) = 0;
    if (d & BIT48) // nonlinear
        return;
    if (mem(D33)) // in an IF
        return;
    list_ = (mem(D15) + 1) & 077777;
    store((list_ - 1) & 077777, mem(D25));
    move_items(d);
    mem(D15)    = cyclic_add(moved_, 1);
    Word load   = cyclic_add(mem(D25), 1);
    mem(first_) = load;
    mem(D23)    = load;
    mem(D25)    = cyclic_add(mem(D25), 010000);
}

//
// 23017: find the items of OPND (first_…last_) in the list from list_ (up
// to ITEM): list_ = its 0160 or 0162 head, or 0 if not there.
//
void Pass3::find_items()
{
    last_  = mem(OPND) & 077777;
    first_ = (mem(OPND) >> 15) & 077777;
    for (;;) {
        if (list_ == mem(ITEM)) {
            list_ = 0;
            return;
        }
        Word w = mem(list_);
        list_  = (list_ + 1) & 077777;
        if (!(w & BIT48))
            continue;
        list_      = (list_ - 1) & 077777;
        unsigned p = first_;
        Word diff;
        do { // the items, the zeros of the stack skipped
            diff = mem((p + 1) & 077777);
            if (diff) {
                diff ^= mem((list_ + 1) & 077777);
                list_ = (list_ + 1) & 077777;
            }
            p = (p + 1) & 077777;
        } while (!diff);
        if (diff & HEAD_MASK)
            continue;
        if ((mem(list_) ^ 0162) & 0177)
            continue;
        return;
    }
}

// 23062: the access through the register of the load at list_: the
// operand's items become the one access item.
void Pass3::through_register(Word code)
{
    mem(first_) = (mem((list_ + 1) & 077777) & REG_BITS) | code;
    mem(OPND)   = (Word(first_) << 15) | first_;
    do {
        first_      = (first_ + 1) & 077777;
        mem(first_) = 0;
    } while (first_ != last_);
}

// The access item: 0163 for an element read (044), else 0164, with the
// array of the operand's first item.
Word Pass3::access_code()
{
    Word code = ((mem(last_) & 0177) == 044) ? 0163 : 0164;
    return code | (mem(first_) & C_FIELD12);
}

// 23051…23072: the element read (0163) or stored (0164) through a register
// of the body (D29: its step); then on to link.
unsigned Pass3::access(unsigned link)
{
    list_     = BODY_LIST;
    mem(ITEM) = mem(D15);
    find_items();
    Word code = access_code();
    if (!list_)
        return body_register(code, link);
    through_register(code);
    return link;
}

//
// 23036: an element c·I+d: the increment c·step must fit 12 bits, else
// 23164.
//
unsigned Pass3::stepped_access(unsigned link)
{
    Word c   = ((mem((subs_ + 1) & 077777) >> 30) & ADDR) | INTEGER;
    Word inc = unit_add(RAW, unit_mul(NORM, c, mem(D18)), INTEGER) & TRUNCATE;
    push(inc);
    if (!(unit_sub(RAW, inc, C_LOW) & NEGATIVE) && (unit_sub(RAW, inc, C_HIGH) & NEGATIVE))
        return PRE_REG;
    mem(D29) = (pop() & 07777) << 12;
    return access(link);
}

// The load of the operand OPND (items moved from the value stack to the
// list ending at [list]): its first item gets FLAGS, its last becomes the
// 0162 head, followed by register number [k] of the table REGS, which is
// returned.
Word Pass3::load_register(unsigned list, unsigned k)
{
    list_ = mem(list) & 077777;
    move_items(mem(OPND));
    mem(list_)     = (mem(list_) & 07777) | FLAGS;
    list_          = moved_; // the last item moved: the head
    moved_         = (moved_ + 2) & 077777;
    mem(list)      = moved_;
    mem(list_)     = (mem(list_) & HEAD_MASK) ^ 0162;
    Word reg       = REGS.at(mem(k) & 077777);
    mem(list_ + 1) = reg;
    return reg;
}

// A word to the increments list; returns the list's new end.
Word Pass3::add_increment(Word w)
{
    mem(mem(D16) & 077777) = w;
    return cyclic_add(mem(D16), 1);
}

// 23130: a register for the body (D27+1, unless it meets the preamble's),
// stepped by c<<12 | 0165 at the end when D29 ≠ 0.
unsigned Pass3::body_register(Word code, unsigned link)
{
    Word next = cyclic_add(mem(D27), 1);
    if (next == mem(D28))
        return GIVE_UP;
    mem(D27) = next;
    load_register(D15, D27);
    if (Word c = mem(D29))
        mem(D16) = add_increment(c ^ mem(list_ + 1) ^ 0165);
    through_register(code);
    return link;
}

//
// 23164: an element whose register is loaded in the preamble: D20 set when
// subscripts with c ≠ 0. A new register (D28−1, not in an IF) is stepped at
// the end.
//
unsigned Pass3::preamble_access()
{
    if (last_)
        mem(D20) = 1;
    list_     = PREAMBLE_LIST;
    mem(ITEM) = mem(D14);
    find_items();
    Word code = access_code();
    if (!list_) {
        if (mem(D33))
            return GIVE_UP;
        Word next = cyclic_add(mem(D28), 0'7777'7777'7777'7776);
        if (next == mem(D27))
            return GIVE_UP;
        mem(D28) = next;
        Word reg = load_register(D14, D28);
        mem(D16) = add_increment(reg ^ 0165);
    }
    through_register(code);
    return NO_REG;
}

// 22327: set up the rewrite: the copy read from the output after the FOR,
// written to the scratch.
unsigned Pass3::begin_rewrite()
{
    src_ = &out_;
    rd_  = start_;
    scratch_.clear();
    dst_     = &scratch_;
    wr_      = 0;
    mem(D14) = PREAMBLE_LIST;
    mem(D15) = BODY_LIST;
    mem(D16) = INCREMENTS_LIST;
    mem(D17) = INCREMENTS_LIST;
    mem(D25) = 0160;
    for (unsigned a : { D18, D33, D32, D30, D20 })
        mem(a) = 0;
    mem(D27) = cyclic_add(Word(depth_) ^ WORD_MASK, 5);
    mem(D28) = 010;
    vs_      = VALUES;
    return read_parse(STATEMENT);
}

// 22442: the initial value: as a 045 item in D32 when a constant of 12 bits
// ≠ 0; its item in D30.
unsigned Pass3::initial_parsed()
{
    Word d = mem(OPND);
    if (!(d & FLAGS) && (d & COEF)) {
        Word c = (d & COEF) >> 30;
        if (c < 04000)
            mem(D32) = (c << 12) | 045;
    }
    mem(OPND) = (d & ~BIT48 & WORD_MASK) | VARIABLE;
    check_operand(mem(OPND));
    mem(D30) = mem(D23);
    flush_stack();
    return parse(022457);
}

// Then the next operand.
unsigned Pass3::parse_until()
{
    flush_stack();
    return parse(022471);
}

// 22457: the step: its item in D24, its value in D18 when a constant; a
// step not linear clears D17 and D19.
unsigned Pass3::step_parsed()
{
    Word d = mem(OPND);
    if (d & FLAGS) {
        if (d & BIT48) {
            mem(D19) = 0;
            mem(D17) = 0;
            return parse_until();
        }
    } else {
        Word item = mem(d & 077777);
        mem(D24)  = item;
        Word v    = mem(TABLE + ((item >> 12) & 07777));
        if ((v << 7) & WORD_MASK) {
            mem(D18) = v | INTEGER;
            return parse_until();
        }
    }
    check_operand(mem(OPND));
    mem(D24) = mem(D23);
    return parse_until();
}

// 22471: the until value: its item in D35; then the body (from DO). Not
// linear: D17 = 0, D33 = 1.
unsigned Pass3::until_parsed()
{
    if (mem(OPND) & BIT48) {
        mem(D17) = 0;
        mem(D33) = 1;
    }
    check_operand(mem(OPND));
    mem(D35) = mem(D23);
    flush_stack();
    return classify();
}

// 22507: a label (024, 0163). One of the loop (below the control variable's
// block) stops register steps: D33 = 077777; else D17 = 0.
unsigned Pass3::label()
{
    Word n      = mem(CUR) >> 12;
    mem(ITEM)   = n;
    bool inside = !(unit_sub(RAW, n, mem(LVAR)) & NEGATIVE) &&
                  (unit_rsub(RAW, mem(mem(LVAR) & 077777) & 07777, n) & NEGATIVE);
    if (inside)
        mem(D33) = 077777;
    else
        mem(D17) = 0;
    return AFTER_LABEL;
}

// 22532: OPND = a | the word on the stack; return.
unsigned Pass3::result(Word a)
{
    mem(OPND) = a | pop();
    return ret();
}

// 22527: a variable (not linear) result from the left operand's first item.
unsigned Pass3::variable_result()
{
    Word left = pop();
    push((cyclic_add(left, 0'7777'7777'7767'7777) & FIRST) | VARIABLE);
    return result(mem(OPND) & ADDR);
}

// 22534: a nonlinear result: both operands done.
unsigned Pass3::nonlinear_result()
{
    check_operand(top(1));
    check_operand(mem(OPND));
    Word left = pop();
    push((cyclic_add(left, 0'7777'7777'7767'7777) & FIRST) | FLAGS);
    return result(mem(OPND) & ADDR);
}

// 22542: after a unary operator's operand.
unsigned Pass3::unary_2()
{
    Word d = mem(OPND);
    if (d & BIT48) {
        check_operand(d);
        return unary_3(mem(OPND) | FLAGS);
    }
    return unary_3(d | VARIABLE);
}

// 22544: OPND = a with the operator's item first.
unsigned Pass3::unary_3(Word a)
{
    mem(OPND) = cyclic_add(a, 0'7777'7777'7767'7777);
    return ret();
}

// 22560: OPND = flags | the item at vs_; the next item.
unsigned Pass3::leaf(Word flags)
{
    mem(OPND) = flags | (Word(vs_) << 15) | vs_;
    mem(CUR)  = get();
    return ret();
}

// 22551: a variable (017). The control variable is linear (an initial
// value item is marked 0200 at vs_); one assigned in the loop is not.
unsigned Pass3::variable()
{
    Word n = mem(CUR) >> 12;
    if (n != mem(CONTROL))
        return leaf((mem(n & 077777) & ASSIGNED) ? FLAGS : VARIABLE);
    if (!mem(D30))
        mem(vs_) |= 0200;
    return leaf(mem(D19) ? CONTROL_C : FLAGS);
}

// 22572: a constant (023): linear with c = the value if an integer.
unsigned Pass3::constant()
{
    Word v = mem(TABLE + ((mem(CUR) >> 12) & 07777));
    Word a = (v ^ INTEGER) >> 43;
    if (!a)
        a = (v << 30) & WORD_MASK;
    return leaf(a & COEF);
}

// 22654: OPND = d; the subscripts dropped, the next item, return.
unsigned Pass3::element_done(Word d)
{
    mem(OPND) = d;
    mem(CUR)  = get();
    sp_       = (sp_ - nsubs_) & 077777;
    return ret();
}

//
// 22607: an array element (044 read, 0164, 046 store), its subscripts on
// the stack. Not in a loop's preamble (D30 = 0) or with a nonlinear
// subscript: as it is. Else one stepped subscript, the rest constant
// (last_ of them with c ≠ 0): through a register.
//
unsigned Pass3::element()
{
    Word item    = mem(CUR);
    unsigned arr = (item >> 12) & 077777;
    nsubs_       = (item >> 7) & 037;
    Word first   = mem((sp_ - nsubs_) & 077777);
    mem(OPND)    = (cyclic_add(first, 0'7777'7777'7767'7777) & FIRST) | vs_;
    if (!mem(D30))
        return NO_REG;
    unsigned p = (sp_ - 1) & 077777, stepped = 0, varying = 0;
    last_ = 0;
    for (unsigned n = nsubs_; n; n--) {
        Word d = mem(p);
        if (d & BIT48) {
            if (d & VARIABLE) {
                varying++;
            } else {
                stepped++;
                if (d & COEF)
                    last_++;
            }
        }
        p = (p - 1) & 077777;
    }
    subs_ = p;
    if (varying)
        return stepped ? GIVE_UP : NO_REG;
    if (stepped) {
        if (!mem(D19))
            return GIVE_UP;
        if (stepped != 1)
            return PRE_REG;
        Word d = mem((subs_ + 1) & 077777);
        if (!(d & BIT48) || !(d & COEF) || !mem(D18))
            return PRE_REG;
        return stepped_access(022652);
    }
    if ((item & 0177) == 044 && !(mem(arr) & ASSIGNED) && !(mem(arr) & BLOCK_MARK))
        return element_done(mem(OPND) | VARIABLE);
    mem(D29) = 0;
    return access(NO_REG);
}

// 22671: integer + − ×, the left operand parsed: not a constant on the
// left, as any binary operator.
unsigned Pass3::int_op_2()
{
    if ((mem(OPND) & FLAGS) == FLAGS) {
        pop();
        return BINARY_2;
    }
    push(mem(OPND));
    return parse(022676);
}

// The value of a constant operand d.
Word Pass3::constant_value(Word d)
{
    Word item = mem((d >> 15) & 077777);
    return mem(TABLE + ((item >> 12) & 077777));
}

// 22676: both operands: two constants are folded into one, interned.
unsigned Pass3::int_op_3()
{
    unsigned op = top(2) & 077777;
    if (Word flags = (top(1) | mem(OPND)) & FLAGS)
        return linear_result(flags);
    Word l = constant_value(top(1)), r = constant_value(mem(OPND));
    Word v;
    switch (op) {
    case 0:
        v = unit_add(RAW, r, l);
        break;
    case 1:
        v = unit_rsub(RAW, r, l);
        break;
    default:
        v = integer_product(r, l);
        break;
    }
    Word item = intern(v);
    // The left operand's items replaced by the new constant.
    unsigned p            = (top(1) >> 15) & 077777;
    mem((p - 1) & 077777) = item;
    unsigned end          = mem(OPND) & 077777;
    for (;;) {
        mem(p)    = 0;
        bool done = p == end;
        p         = (p + 1) & 077777;
        if (done)
            break;
    }
    Word field = cyclic_add(pop(), 0'7777'7777'7767'7777) & FIRST;
    top(1)     = field;
    return result(field >> 15);
}

//
// 23351: + − × with an operand not constant (flags of both): linear when
// both are (c of the result: c1 ± c2, or c1·d2 or d1·c2 for × with a
// constant), else as any binary operator.
//
unsigned Pass3::linear_result(Word flags)
{
    if (!(flags & BIT48)) {
        Word l = pop();
        top(1) = l;
        return variable_result();
    }
    if ((mem(OPND) & FLAGS) == FLAGS) {
        Word l = pop();
        top(1) = l;
        return nonlinear_result();
    }
    unsigned op = top(2) & 077777;
    Word left   = top(1);
    Word right  = mem(OPND);
    Word a      = 0;
    bool fold_c = false;
    if (op == 2) { // ×
        if ((left & BIT48) && (right & BIT48)) {
            pop();
            top(1) = left;
            return nonlinear_result();
        }
        fold_c = true;
    } else if (left & BIT48) {
        a = left & COEF;
        if (a) {
            if (right & BIT48) {
                a      = right & COEF;
                fold_c = a != 0;
            } else {
                a = left;
            }
        }
    } else {
        a = right;
        if (op)
            a = cyclic_add(a ^ COEF, COEF_ONE);
    }
    if (fold_c) {
        Word l = ((top(1) & COEF) >> 30) | INTEGER;
        Word r = ((right & COEF) >> 30) | INTEGER;
        switch (op) {
        case 0:
            a = unit_add(RAW, r, l);
            break;
        case 1:
            a = unit_rsub(RAW, r, l);
            break;
        default:
            a = integer_product(r, l);
            break;
        }
        a = (a << 30) & WORD_MASK;
    }
    a          = a & COEF;
    Word field = cyclic_add(top(1), 0'7777'7777'7767'7777) & FIRST;
    mem(OPND)  = (right & ADDR) | field | a | NONLINEAR;
    pop();
    pop();
    return ret();
}

// 23420: integer negation (026): c negated.
unsigned Pass3::int_neg_2()
{
    Word d = mem(OPND);
    if ((d & FLAGS) != NONLINEAR)
        return unary_2();
    Word c = cyclic_add((d & COEF) ^ COEF, COEF_ONE) & COEF;
    return unary_3((d & STEP_FLAGS) | c);
}

//
// 23076: the branches, D33 counting the depth. An IF statement (the parse
// returns to STATEMENT) ends here; a conditional expression parses both
// branches, then gives a nonlinear operand.
//
unsigned Pass3::if_branches()
{
    mem(D33) = cyclic_add(mem(D33), 1);
    if ((top(2) & 077777) == STATEMENT) {
        pop();
        flush_stack();
        return classify();
    }
    return parse(023102);
}

unsigned Pass3::if_else()
{
    check_operand(mem(OPND));
    vs_      = (vs_ + 1) & 077777;
    mem(vs_) = mem(CUR);
    mem(CUR) = get();
    mem(D33) = unit_sub(RAW, mem(D33), 1);
    push(((pop() << 15) & WORD_MASK) | FLAGS);
    return result(vs_);
}

//
// 23231: the end of the loop (052, 0160). The marks of the assigned
// variables are cleared; the output goes back to after the FOR: the 0160
// heads of the body list without the item of the control variable
// (replaced by D30, or both by D32 for a 030 item), the preamble (0167
// head, the initial value, the until and end items, the loads, then
// (0167−D28)<<7 | 0166), then the scratch up to its 052, the increments
// and the 052. The control variable's descriptor gets flags REG_STEP or
// REG_STEP_N (step 1 or not) when all is in registers, and REG_LOOP with the
// count of registers.
//
unsigned Pass3::end_loop()
{
    pop();
    flush_stack();
    for (unsigned p = SET - 046;; p++) {
        unsigned d = mem(p) & 077777;
        if (!d)
            break;
        mem(d) &= UNMARK;
    }
    dst_      = &out_;
    wr_       = start_;
    mem(ITEM) = (mem(CONTROL) << 12) ^ 017;
    // The body list.
    for (unsigned p = BODY_LIST; p != mem(D15);) {
        Word w = mem(p) & ITEM_MASK;
        mem(p) = w;
        Word a = w;
        if (w == mem(ITEM)) {
            a = mem(D30);
        } else if (w == 030 && mem(p + 1) == mem(ITEM) && mem(D32)) {
            a = mem(D32);
            p++;
        }
        p++;
        put(a);
    }
    // The preamble.
    if (PREAMBLE_LIST != mem(D14)) {
        put((mem(CONTROL) << 12) ^ 0167);
        put(mem(D30));
        put(mem(D24));
        put(mem(D35));
        for (unsigned p = PREAMBLE_LIST; p != mem(D14); p++)
            put(mem(p) & ITEM_MASK);
        put(((unit_sub(RAW, 010, mem(D28)) << 7) & WORD_MASK) ^ 0166);
    }
    unsigned cv = mem(LVAR) & 077777;
    if (mem(D18) && mem(D20) && mem(D17))
        mem(cv) |= (mem(D18) ^ C_ONE) ? REG_STEP_N : REG_STEP;
    mem(cv) = (((mem(D25) & C_FIELD12) << 12) ^ mem(cv)) | REG_LOOP;
    // The loop again, up to its 052.
    src_ = &scratch_;
    rd_  = 0;
    for (;;) {
        Word w = get();
        push(w);
        if ((w & 0177) == 052)
            break;
        put(pop());
    }
    for (unsigned p = INCREMENTS_LIST; p != mem(D16); p++)
        put(mem(p));
    put(pop());
    src_ = &in_;
    rd_  = resume_;
    return MAIN_LOOP;
}

void Pass3::register_rewrite()
{
    auto &n      = handlers_;
    n[MAIN_LOOP] = [this] { return dispatch(get()); };
    n[REWRITE]   = [this] { return begin_rewrite(); };
    n[STATEMENT] = [this] { // a statement is done
        check_operand(mem(OPND));
        flush_stack();
        return parse(STATEMENT);
    };
    n[022425] = [this] { return read_parse(022426); }; // an expression element
    n[022426] = [this] {
        check_operand(mem(OPND));
        if (mem(D23)) {
            Word d                     = mem(OPND);
            first_                     = (d >> 15) & 077777;
            mem(OPND)                  = (((d >> 15) << 15) & WORD_MASK) | first_;
            mem((first_ - 1) & 077777) = 0;
        }
        return ret();
    };
    n[022436] = [this] { // a constant step (0137): the control variable marked
        mem(mem(LVAR) & 077777) |= STEP_ONE;
        return read_parse(022442);
    };
    n[022441]      = [this] { return read_parse(022442); }; // step-until (034)
    n[022442]      = [this] { return initial_parsed(); };
    n[022457]      = [this] { return step_parsed(); };
    n[022471]      = [this] { return until_parsed(); };
    n[022507]      = [this] { return label(); };
    n[AFTER_LABEL] = [this] {
        pop();
        flush_stack();
        return read_parse(STATEMENT);
    };
    n[022522]   = [this] { return read_parse(BINARY_2); }; // a binary operator
    n[BINARY_2] = [this] {
        push(mem(OPND));
        return parse(022525);
    };
    n[022525] = [this] {
        if ((top(1) | mem(OPND)) & BIT48)
            return nonlinear_result();
        return variable_result();
    };
    n[022541] = [this] { return read_parse(022542); }; // a unary operator
    n[022542] = [this] { return unary_2(); };
    n[022551] = [this] { return variable(); };
    n[022570] = [this] { return leaf(VARIABLE); }; // a string (070)
    n[022572] = [this] { return constant(); };
    n[022600] = [this] { // a constant subscript (045, 0166)
        mem(CUR) = get();
        push(Word((vs_ + 1) & 077777) << 15);
        return classify();
    };
    n[022604] = [this] { return read_parse(022605); }; // subscript + c (030)
    n[022605] = [this] {
        push(mem(OPND));
        return classify();
    };
    n[022607] = [this] { return element(); };
    n[022652] = [this] { // after a stepped access: the step is needed
        mem(D20) = 1;
        return NO_REG;
    };
    n[NO_REG]  = [this] { return element_done(mem(OPND) | FLAGS); };
    n[GIVE_UP] = [this] {
        mem(D17) = 0;
        return NO_REG;
    };
    // Integer + − × (067, 076 and 0170, 077).
    for (unsigned op = 0; op < 3; op++)
        n[022664 + op] = [this, op] {
            push(op);
            return read_parse(022671);
        };
    n[022671]  = [this] { return int_op_2(); };
    n[022676]  = [this] { return int_op_3(); };
    n[PRE_REG] = [this] { return preamble_access(); };
    n[023073]  = [this] { // IF (027): vs_ kept; the condition
        push(vs_);
        return read_parse(023075);
    };
    n[023075] = [this] {
        check_operand(mem(OPND));
        return if_branches();
    };
    n[IF_2]   = [this] { return if_branches(); };
    n[023102] = [this] {
        check_operand(mem(OPND));
        vs_      = (vs_ + 1) & 077777;
        mem(vs_) = mem(CUR);
        return read_parse(023105);
    };
    n[023105] = [this] { return if_else(); };
    n[023115] = [this] { // compare-and-jump (0130–0134): both sides, then as IF
        push(vs_);
        return read_parse(023117);
    };
    n[023117] = [this] {
        check_operand(mem(OPND));
        return parse(023121);
    };
    n[023121] = [this] {
        check_operand(mem(OPND));
        return if_branches();
    };
    n[023123] = [this] { // the end of an IF (053, 054)
        mem(D33) = unit_sub(RAW, mem(D33), 1);
        return AFTER_LABEL;
    };
    n[023231] = [this] { return end_loop(); };
    n[023417] = [this] { return read_parse(023420); }; // integer negation (026)
    n[023420] = [this] { return int_neg_2(); };
}

Pass3::Pass3(Context &ctx, const Stream &input) : ctx_(ctx)
{
    in_.reserve(input.size());
    std::transform(input.begin(), input.end(), std::back_inserter(in_),
                   [](const Item &item) { return Word(item.bits); });
    register_rewrite();
}

Stream Pass3::run()
{
    unsigned pc = start();
    while (pc != DRIVER) {
        auto it = handlers_.find(pc);
        if (it == handlers_.end())
            throw std::runtime_error("pass 3: no handler at " + std::to_string(pc));
        pc = it->second();
    }
    Stream out(out_.size());
    std::transform(out_.begin(), out_.end(), out.begin(),
                   [](Word w) { return Item{ uint32_t(w) }; });
    return out;
}

} // namespace

Stream pass3(Context &ctx, const Stream &input)
{
    return Pass3(ctx, input).run();
}
