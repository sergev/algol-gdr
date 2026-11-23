//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "pass4.h"

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "arith.h"
#include "besm6.h"

namespace {

// Cells of the workspace.
constexpr unsigned LABELS      = 02004; // the next label number
constexpr unsigned EMIT_MODE   = 02022; // set by 0037.0002: the next half starts a word
constexpr unsigned NAMES       = 06000; // the names saved by pass 2
constexpr unsigned EXT_NAMES   = 027245; // external table: 3 words per name

// Addresses of the original: continuations, kept on the stack as return
// addresses, and handlers.
constexpr unsigned DRIVER        = 01633; // the return into the driver
constexpr unsigned MAIN_LOOP     = 023625;
constexpr unsigned DISPATCH_NEXT = 023753;
constexpr unsigned RESULT        = 024015; // the value is in A

// The machine stack, as the driver leaves it.
constexpr unsigned STACK      = 053401;
constexpr unsigned STACK_SIZE = 010000;

// The arithmetic unit without normalization or rounding (NTR 3).
constexpr unsigned RAW = Arith::NORM_DISABLE | Arith::ROUND_DISABLE;

constexpr Word BIT48      = 0'4000'0000'0000'0000;
constexpr Word BIT47      = 0'2000'0000'0000'0000; // a descriptor; without it, a link
constexpr Word BIT46      = 0'1000'0000'0000'0000; // its offset counted down from 02013
constexpr Word BIT45      = 0'0400'0000'0000'0000;
constexpr Word ADDR       = 077777;
constexpr Word MINUS_ONE  = 0'7777'7777'7777'7776; // cyclic −1
constexpr Word MINUS_TWO  = 0'7777'7777'7777'7775;
constexpr Word ON_STACK   = 074000000; // 15 ATX: the value saved on the stack
constexpr Word DROPPED    = 074030000; // never emitted
constexpr Word ALIGN      = 0370002;   // the next half starts a word
constexpr Word ALIGN_END  = 0370003;
constexpr Word ALIGN_FILL = 0370006;              // written before a half that does not fit
constexpr Word FITS_CMD   = 01014000000;          // by bits 20–16 of a command half
constexpr Word FITS_OP    = 0'0003'7400'0002'7413; // by bits 18–13 of others
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

constexpr Word LABEL_U1A   = 02600000;  // U1A to label n
constexpr Word LABEL_U1A2  = 02700000;
constexpr Word LABEL_UJ    = 03000000;  // UJ to label n
constexpr Word LABEL_DEF   = 02100000;  // label n defined here
constexpr Word NEW_WORD    = 02000000;
constexpr Word SWITCH_UJ   = 042400000;
constexpr Word PREV_LABEL  = 02777777; // added to a label: the one after
constexpr Word FORMAL      = 0'4000'0000'7407'7777; // a formal's descriptor
constexpr Word BY_NAME     = 0'4000'0000'7400'0000; // a pushed address
constexpr Word REG_STORE   = 060440000;             // its address to register n
constexpr Word STACK_ATX1  = 074000001;
constexpr Word ASN         = 0360000;
constexpr Word DIV_1       = 074010001; // real division, both on the stack
constexpr Word DIV_2       = 074160002;

constexpr Word TYPE        = 0'0360'0000'0000'0000;
constexpr Word OFFSET_CLR  = 0'7777'7777'0370'0000; // a descriptor without its offset
constexpr Word HEAD_KEEP   = 0'7400'0000'7777'7777;
constexpr Word HEAD_KIND   = 0'1400'0000'0000'0000;
constexpr Word TEXT_HEAD   = 0'1200'0000'0000'0000;
constexpr Word DOPE_INIT   = 0'2021'0000'0000'0000; // a dope word not yet set
constexpr Word DOPE_MASK   = 074007777;
constexpr Word FRAME_STEP  = 010000;
constexpr Word FRAME_BASE  = 0'7777'7777'7377'7777; // added to the frame of a procedure
constexpr Word LINE_HEAD   = 03300000;              // 0330.n: line n
constexpr Word PROC_LABEL  = 036400000;
constexpr Word FRAME_REG   = 0100000;
constexpr Word RETURN_1    = 0440017;
constexpr Word RETURN_2    = 03004020;

constexpr Word INDEX_A     = 062400000; // the half that indexes by A
constexpr Word INDEX_REG   = 02400000;  // … by register r<<20
constexpr Word ARRAY_HALF  = 02300000;
constexpr Word ELEMENT_A   = 02200000;
constexpr Word ELEMENT_REG = 02500000;
constexpr Word CHECK_REG   = 0440014; // A/SUBCH's register
constexpr Word ADDRESS_OP  = 060000000;
constexpr Word DOPE_HALF   = 0420017;
constexpr Word STEP_ONE    = 01000000; // a loop descriptor: the step is 1

constexpr Word BY_REGISTERS = 0300000; // a loop descriptor: pass 3's registers
constexpr Word UJ_BACK      = 057700000;

constexpr Word VALUE_START = 056400000;
constexpr Word CODE_RESULT = 0420014;
constexpr Word RESULT_SIZE = 076410000;

Word cyclic_add(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & WORD_MASK;
    return s;
}

// The packed descriptor of a frame size (APX, then 5 right).
Word packed_frame(Word size)
{
    return pack_bits(size, DESC_MASK) >> 5;
}

// The unpacked address of a packed descriptor (5 left, AUX).
Word unpacked(Word d)
{
    return unpack_bits((d << 5) & WORD_MASK, DESC_MASK);
}

// A descriptor's address, unpacked when packed (bit 48).
Word address_of(Word d)
{
    return ((d & BIT48) ? unpacked(d) : d) & DESC_MASK;
}

// a < x on the arithmetic unit without normalization or rounding.
bool unit_less(Word a, Word x)
{
    Arith u(a, RAW);
    u.sub(x);
    return u.acc & 0'0020'0000'0000'0000;
}

//
// Instruction templates (in the code of G23523 and at 25747…): the halves
// an item emits; * is the operand (address 4444), taken from the stack.
//
enum Template : unsigned {
    PROGRAM_START,
    STRING_ADDRESS,
    STRING_ADDRESS_SAVED,
    PUSH_ADDRESS,
    CHECK_SUBSCRIPT,
    INDEX,
    CALL_FRAME,
    CALL_FRAME_SAVED,
    STD_FUNCTION_CALL,
    CALL_NO_PARAMETERS,
    CALL_PARAMETERS,
    CALL_ON_STACK,
    ARG_THUNK,
    ARG_CONSTANT,
    ARG_VARIABLE,
    ARG_ADDRESS,
    ARG_PROCEDURE,
    ARG_EXPRESSION,
    ARG_STRING,
    PASS_ADDRESS,
    PASS_ARRAY,
    PASS_INDIRECT,
    BLOCK_ARRAYS,
    JUMP,
    FRAME_RESET,
    FRAME_RESET_INDIRECT,
    PROCEDURE_ENTRY,
    VALUE_REAL,
    VALUE,
    SWITCH,
    SAVE_FRAME,
    SIGN_TEST,
    STEP_START,
    LOOP_STEP,
    LOOP_STEP_CYCLIC,
    LOOP_STEP_INTEGER,
    STEP_TEMPORARY,
    STEP_IN_TEMPORARY,
    INTEGER_ROUND,
    STRING_SLICE,
    STEP_REGISTER,
    PREAMBLE_CYCLIC,
    PREAMBLE,
    PREAMBLE_END,
    LOOP_CALL_3,
    LOOP_CALL_2,
    CALL_INDIRECT,
    CALL,
    NEGATE,
    NEGATE_INTEGER,
    NOT,
    STORE,
    ABS,
    TO_REAL,
    TO_INTEGER,
    NOT_OCTAL,
    PUSH,
    LOAD,
    ADD,
    MULTIPLY,
    EQUAL,
    NOT_EQUAL,
    OR,
    AND_CALL,
    EQUIV,
    ADD_INTEGER,
    MULTIPLY_INTEGER,
    EQUAL_MIXED,
    NOT_EQUAL_MIXED,
    AND,
    SUBTRACT,
    SUBTRACT_FROM,
    DIVIDE,
    DIVIDE_INTO,
    SIGN_FROM,
    SIGN,
    NOT_SIGN,
    NOT_SIGN_FROM,
    SUBTRACT_INTEGER,
    SUBTRACT_FROM_INTEGER,
    STRING_COMPARE,
    JUMP_ZERO_FROM,
    JUMP_ZERO,
    JUMP_NONZERO_FROM,
    JUMP_NONZERO,
    LABEL,
    PROGRAM_END,
    TEMPLATE_COUNT
};

const char *const TEMPLATE_TEXT[] = {
    "7 VTM *; 13 VJM 4014; 15 UTM *", // PROGRAM_START
    "UTC *; 14 VTM 0; ITA 16", // STRING_ADDRESS
    "UTC *; 14 VTM 0; ITS 16", // STRING_ADDRESS_SAVED
    "ITA 14; 15 ATX 0", // PUSH_ADDRESS
    "XTA *; 13 VJM 4114; *20 0", // CHECK_SUBSCRIPT
    "WTC *", // INDEX
    "ITA 17; 15 UTM 10; *20 0", // CALL_FRAME
    "ITS 17; 15 UTM 10; *20 0", // CALL_FRAME_SAVED
    "12 VTM *; 14 VTM 4066; 13 VJM 4010; *20 0", // STD_FUNCTION_CALL
    "14 VTM *; 13 VJM 4007; *20 0", // CALL_NO_PARAMETERS
    "14 VTM *; 13 VJM 4010", // CALL_PARAMETERS
    "ATI 17; UTC *; 13 VJM 0; *20 0", // CALL_ON_STACK
    "14 VJM 4017; UTC *", // ARG_THUNK
    "14 VJM 4011; UTC *", // ARG_CONSTANT
    "14 VJM 4012; UTC *", // ARG_VARIABLE
    "14 VJM 4013; UTC *", // ARG_ADDRESS
    "14 VJM 4015; UTC *", // ARG_PROCEDURE
    "14 VJM 4016; *20 0", // ARG_EXPRESSION
    "14 VJM 4027; UTC *", // ARG_STRING
    "UTC *; 14 VTM 0; ITS 16", // PASS_ADDRESS
    "XTS *; ASN 136", // PASS_ARRAY
    "WTC *; 14 VTM 0; ITS 16", // PASS_INDIRECT
    "11 VTM *; 12 VTM *; UTC *; 13 VTM 0; 14 VJM *; *20 0", // BLOCK_ARRAYS
    "UJ *; *20 0", // JUMP
    "UTC *; 15 VTM 0", // FRAME_RESET
    "WTC *; 15 VTM 0", // FRAME_RESET_INDIRECT
    "*21 *; 15 MTJ *; 15 UTM *", // PROCEDURE_ENTRY
    "UTC *; 13 VJM 0; *20 0; A+X 0; ATX *", // VALUE_REAL
    "UTC *; 13 VJM 0; *20 0; ATX *", // VALUE
    "*21 *; 8 UJ *; *20 0", // SWITCH
    "14 VTM *; ITA 16; ATX *", // SAVE_FRAME
    "AVX *", // SIGN_TEST
    "UJ *; *21 *; XTA *", // STEP_START
    "A+X *; *21 *; ATX *", // LOOP_STEP
    "ARX *; *21 *; ATX *", // LOOP_STEP_CYCLIC
    "NTR 3; A+X *; NTR 2; *21 *; ATX *", // LOOP_STEP_INTEGER
    "ATX *; 14 VTM *; ITA 16; ATX *; *21 *", // STEP_TEMPORARY
    "WTC *; UJ 0; *21 *; 14 VTM *; ITS 16; ATX *; XTA *; UJ *; *21 *; 15 ATX 0", // STEP_IN_TEMPORARY
    "NTR 3; A+X 4006; NTR 2", // INTEGER_ROUND
    "13 VJM 4100; *20 0", // STRING_SLICE
    "WTC *; UTM *", // STEP_REGISTER
    "XTA *; UJ *; *21 *; XTA *; ARX *; *21 *; ATX *; X-A *; U1A *; NTR 6", // PREAMBLE_CYCLIC
    "XTA *; UJ *; *21 *; XTA *; A+X *; *21 *; ATX *; X-A *; AVX *; U1A *; NTR 6", // PREAMBLE
    "*21 *; 12 VTM *; 14 VTM *; 13 VJM 4115; *20 0", // PREAMBLE_END
    "XTA *; XTS *; XTS *; 14 VTM *; 13 VJM 4117; *21 *", // LOOP_CALL_3
    "XTA *; XTS *; 14 VTM *; 13 VJM 4116; *21 *", // LOOP_CALL_2
    "UTC *; 13 VJM 0; *20 0", // CALL_INDIRECT
    "13 VJM *; *20 0", // CALL
    "AVX 4000", // NEGATE
    "NTR 3; AVX 4000; NTR 2", // NEGATE_INTEGER
    "14 VJM 4001; *20 0", // NOT
    "ATX *", // STORE
    "AMX 0", // ABS
    "A+X 0", // TO_REAL
    "14 VJM 4002; *20 0", // TO_INTEGER
    "14 VJM 4120; *20 0", // NOT_OCTAL
    "XTS *", // PUSH
    "XTA *", // LOAD
    "A+X *", // ADD
    "A*X *", // MULTIPLY
    "AEX *; 14 VJM 4001; *20 0", // EQUAL
    "AEX *", // NOT_EQUAL
    "AOX *", // OR
    "XTS *; 14 VJM 4004; *20 0", // AND_CALL
    "XTS *; 14 VJM 4005; *20 0", // EQUIV
    "NTR 3; A+X *; NTR 2", // ADD_INTEGER
    "A*X *; NTR 3; A+X 4006; NTR 2", // MULTIPLY_INTEGER
    "A-X *; 14 VJM 4001; *20 0", // EQUAL_MIXED
    "A-X *; AEX 0", // NOT_EQUAL_MIXED
    "AAX *", // AND
    "A-X *", // SUBTRACT
    "X-A *", // SUBTRACT_FROM
    "A/X *", // DIVIDE
    "XTS *; 15 A/X 0", // DIVIDE_INTO
    "X-A *; AAX 4000", // SIGN_FROM
    "A-X *; AAX 4000", // SIGN
    "A-X *; AAX 4000; AEX 4000", // NOT_SIGN
    "X-A *; AAX 4000; AEX 4000", // NOT_SIGN_FROM
    "NTR 3; A-X *; NTR 2", // SUBTRACT_INTEGER
    "NTR 3; X-A *; NTR 2", // SUBTRACT_FROM_INTEGER
    "AEX *", // STRING_COMPARE
    "X-A *; UZA *", // JUMP_ZERO_FROM
    "A-X *; UZA *", // JUMP_ZERO
    "X-A *; U1A *", // JUMP_NONZERO_FROM
    "A-X *; U1A *", // JUMP_NONZERO
    "*21 *", // LABEL
    "UJ 4047; *20 0; *21 *", // PROGRAM_END
};
static_assert(std::size(TEMPLATE_TEXT) == TEMPLATE_COUNT);

// The halves of template t.
const std::vector<unsigned> &halves(Template t)
{
    static const auto all = [] {
        std::vector<std::vector<unsigned>> v;
        for (const char *text : TEMPLATE_TEXT) {
            std::vector<unsigned> h;
            std::stringstream parts(text);
            for (std::string part; std::getline(parts, part, ';');)
                h.push_back(besm6_assemble(part));
            v.push_back(h);
        }
        return v;
    }();
    return all.at(t);
}

// An operator not commutative (24113…) and a compare-and-jump (26052…): the
// template for A op stack (left) and the one when the right operand is in A.
enum Pair : unsigned {
    MINUS, // 014 real -
    DIVIDE_PAIR, // 016 /
    GREATER, // 010 >
    LESS, // 005 <
    GREATER_EQUAL, // 007 >=
    LESS_EQUAL, // 006 <=
    MINUS_INTEGER, // 076 integer -
    JUMP_0133, // 0133
    JUMP_0130, // 0130
    JUMP_0131, // 0131
    JUMP_0132, // 0132
};

struct TemplatePair {
    Template left, right;
};

const TemplatePair PAIRS[] = {
    { SUBTRACT, SUBTRACT_FROM },
    { DIVIDE, DIVIDE_INTO },
    { SIGN_FROM, SIGN },
    { SIGN, SIGN_FROM },
    { NOT_SIGN, NOT_SIGN_FROM },
    { NOT_SIGN_FROM, NOT_SIGN },
    { SUBTRACT_INTEGER, SUBTRACT_FROM_INTEGER },
    { JUMP_ZERO_FROM, JUMP_ZERO },
    { JUMP_ZERO, JUMP_ZERO_FROM },
    { JUMP_NONZERO_FROM, JUMP_NONZERO },
    { JUMP_NONZERO, JUMP_NONZERO_FROM },
};

// The handler of each item code (26056…, two per word).
const unsigned HANDLERS[0200] = {
    000000, 025335, 024052, 024053, 024023, 024107, 024111, 024110, // 000
    024106, 024050, 024051, 024046, 024075, 024047, 024076, 024000, // 010
    024405, 024412, 024005, 023775, 024666, 024661, 024022, 024137, // 020
    024246, 024213, 025062, 025071, 025126, 025131, 024746, 024752, // 030
    024616, 025023, 024711, 024055, 024252, 024244, 024260, 024021, // 040
    025346, 024746, 025103, 024206, 024207, 025042, 025130, 024377, // 050
    024673, 024767, 024730, 024027, 024026, 024562, 024531, 024056, // 060
    023764, 024030, 024037, 024551, 024533, 026152, 024112, 024057, // 070
    024221, 024226, 024655, 024311, 024646, 024060, 024061, 024010, // 100
    024361, 024424, 024430, 024440, 024432, 024434, 024461, 024452, // 110
    024444, 024502, 024503, 024516, 024523, 024507, 024547, 025005, // 120
    024162, 024163, 024164, 024161, 024146, 025067, 024016, 025127, // 130
    025125, 025064, 025020, 025010, 025012, 024755, 024761, 025326, // 140
    025323, 025325, 025276, 025300, 024776, 024764, 024423, 025336, // 150
    025370, 025363, 025460, 025365, 025375, 025400, 025433, 025412, // 160
    002005, 020066, 026156, 002020, 000000, PROGRAM_END, 023734, 000000, // 170
};

// The name of the main program, as TEXT (23611).
const Word MAIN_NAME = utf8_to_text("PROGRAM");

// Where an element of a FOR list goes on, by its kind (25151…).
const unsigned BY_KIND_NEXT[] = { 025176, 025176, 025176, 025156, 025163 };

// The six-bit TEXT code of each ISO character (G30034, 30076…); the words
// below 060 are other data of the original.
const uint8_t TEXT_CODES[0200] = {
    000, 000, 000, 000, 000, 000, 000, 032, 000, 000, 000, 000, 000, 000, 000, 027, // 000
    000, 000, 000, 000, 000, 000, 000, 024, 000, 000, 000, 000, 000, 000, 000, 021, // 020
    000, 000, 000, 000, 000, 000, 000, 016, 000, 000, 000, 000, 000, 000, 000, 013, // 040
    020, 021, 022, 023, 024, 025, 026, 027, 030, 031, 000, 000, 000, 000, 000, 000, // 060
    000, 041, 042, 043, 044, 045, 046, 047, 050, 051, 052, 053, 054, 055, 056, 057, // 100
    060, 061, 062, 063, 064, 065, 066, 067, 070, 071, 072, 000, 000, 000, 000, 000, // 120
    077, 041, 002, 003, 004, 045, 005, 006, 070, 007, 013, 053, 014, 055, 050, 057, // 140
    034, 015, 060, 043, 064, 071, 016, 042, 032, 037, 040, 073, 074, 075, 076, 000, // 160
};

//
// G30034: the 6 ISO characters iso as TEXT: each character c its six-bit
// code from the tables at 30076 (word c/8, field c%8), left-justified in 8
// six-bit characters.
//
Word iso_text(Word iso)
{
    Word text = 0;
    for (unsigned k = 0; k < 6; k++) {
        unsigned c = (iso >> (8 * k)) & 0377;
        if (c) {
            text |= Word(TEXT_CODES[c & 0177]) << (12 + 6 * k);
        }
    }
    return text;
}

//
// Pass 4 runs as the original: handlers chained by the addresses they
// return (continuations), the return addresses of operands on its stack.
// An item's handler leaves the descriptor of its value on the stack in
// place of the return address it pops: an address (with 074000000: on the
// stack, bit 48: through a register…), or 0 for a value in A, acc_cell_
// then its stack cell. Operands come after their operator in the stream.
//
class Pass4 {
public:
    Pass4(Context &ctx, const Stream &input);

    Stream run();

private:
    using Handler = std::function<unsigned()>;

    Context &ctx_;
    const Stream &in_;
    size_t pos_{};
    Stream out_;
    std::map<unsigned, Handler> handlers_;

    std::array<Word, STACK_SIZE> stack_{};
    std::array<Word, 1024> names_{}; // at NAMES
    unsigned sp_{ STACK };
    Word zero_{};

    // The item: code (bits 7–1), attribute (12–8), value (24–13).
    unsigned code_{};
    unsigned attr_{};
    unsigned value_{};

    unsigned acc_cell_{}; // the stack cell of a value in A, or 02017, or 0
    unsigned count_{};    // arguments of a non-ALGOL call that are values
    unsigned arg_{};      // the argument pair being passed
    unsigned args_{};     // the first argument pair, where the stack goes back
    unsigned routine_{};  // the routine called

    // Block entry: numbers counted, the TEXT of the next external name.
    unsigned number_{ 010000 };
    unsigned ext_text_{ 04133 };

    // Cells of the pass (M7+36, +37, +166, +1106, +1107).
    Word half_{};      // the half being written; a name being looked up
    Word ext_count_{}; // externals numbered so far
    Word ext_next_{};  // the external entry written next
    Word emit_link_{};
    Word line_{}; // a line marker to write after a call of an external

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
    // The k-th word from the top of the stack.
    Word &top(unsigned k = 1) { return mem((sp_ - k) & 077777); }

    Word read();
    void write(Word w) { out_.push_back(Item{ uint32_t(w & 077777777) }); }
    Word new_label();
    Word constant_address() const { return cyclic_add(value_, ctx_.ws[02010]); }

    // Framework.
    void emit(Word half);
    void emit_template(Template t);
    void save_acc();
    void load_value(Word d);
    void operand_done();
    unsigned dispatch_next();
    unsigned operand(unsigned link);
    unsigned main_loop();
    unsigned start();
    unsigned eop();

    // Operators.
    unsigned give(Word d);
    unsigned give_none();
    unsigned result_in_acc();
    unsigned apply_template(Template t);
    unsigned string_item();
    unsigned variable_item();
    unsigned call_item();
    unsigned formal_item();
    unsigned unary(Template t);
    unsigned unary_2();
    unsigned std_function();
    unsigned std_procedure();
    unsigned apply(Template t);
    void left_to_acc();
    unsigned binary(Template t);
    unsigned binary_3();
    unsigned paired(Pair pair, unsigned link);
    unsigned paired_3();
    unsigned divide_3();
    unsigned assign(Word target);
    unsigned assign_pushed();
    unsigned assign_formal();
    unsigned power(Word flag);
    unsigned power_loop();
    unsigned power_2();
    unsigned octal_shift_2();
    unsigned library_call(unsigned r);
    unsigned slice_2();
    void register_operators();

    // Jumps.
    unsigned jump_ahead(Word half);
    bool comparison_operands();
    unsigned string_jump_3();
    unsigned compare_jump(Pair pair);
    unsigned compare_jump_3();
    unsigned define_label(Word n);
    unsigned else_item();
    unsigned goto_label();
    unsigned switch_element();
    unsigned switch_end();
    void register_jumps();

    // Blocks.
    void set_offset(unsigned p, Word v);
    void offset_up(unsigned p);
    void offset_down(unsigned p);
    void number(unsigned p);
    void external_name(unsigned p, unsigned mod);
    bool array_dope(unsigned &p);
    void block_entry(unsigned list);
    unsigned frame_reset(Word d);
    unsigned label_item();
    unsigned line_item();
    unsigned procedure_item();
    unsigned procedure_end();
    void register_blocks();

    // Arrays.
    unsigned next_subscript();
    unsigned subscript(bool counted);
    void emit_element(unsigned k);
    unsigned element_read();
    unsigned element_store();
    unsigned element_address();
    unsigned array_segment();
    unsigned register_step();
    unsigned preamble_4();
    unsigned preamble_end();
    unsigned register_load();
    unsigned loop_template(std::initializer_list<unsigned> offsets, Template t, unsigned n);
    void register_arrays();

    // FOR loops.
    Word loop_descriptor() { return mem(mem(02031) & 077777); }
    unsigned for_item();
    unsigned for_jump(Word half);
    unsigned do_item();
    unsigned for_end();
    unsigned element_item(unsigned k);
    void store_control();
    unsigned plain_value();
    unsigned by_kind(Template t, Word kind);
    unsigned step_operand();
    unsigned until_test();
    unsigned until_value();
    unsigned step_in_acc();
    unsigned step_in_acc_2();
    void register_loops();

    // Procedures and non-ALGOL calls.
    void pending_label();
    unsigned call_tail(Template t, Word d);
    unsigned argument(Template t, Word a);
    unsigned thunk_start();
    unsigned value_parameter(Template t);
    unsigned push_argument(Word a, unsigned passer);
    unsigned code_value(unsigned passer);
    unsigned code_call(unsigned r, Word count);
    unsigned pass_argument(Template t);
    unsigned pass_value(bool result);
    void register_procedures();
};

Word &Pass4::mem(unsigned addr)
{
    if (Workspace::contains(addr))
        return ctx_.ws[addr];
    if (addr >= STACK && addr < STACK + STACK_SIZE)
        return stack_[addr - STACK];
    if (addr >= NAMES && addr < NAMES + 1024)
        return names_[addr - NAMES];
    if (!addr) {
        zero_ = 0;
        return zero_;
    }
    throw std::runtime_error("pass 4: no data at " + std::to_string(addr));
}

Word Pass4::read()
{
    if (pos_ >= in_.size())
        throw std::runtime_error("pass 4: the stream ends without EOP");
    return in_[pos_++].bits;
}

// The next label.
Word Pass4::new_label()
{
    Word n         = mem(LABELS);
    mem(LABELS) = cyclic_add(n, 1);
    return n;
}

//
// 23655: emit an instruction half. 7403.0000 is dropped. 0037.0002 starts
// EMIT_MODE: until 0037.0003, a half that does not fit the word in
// progress (FITS_CMD, FITS_OP by its opcode) first gets a 0037.0006 written
// before it. A line marker (line_) is written after a call of an external
// (address 4131), preceded by the number of the next external name in the
// table not written yet.
//
void Pass4::emit(Word half)
{
    if (half == DROPPED)
        return;
    if (mem(EMIT_MODE)) {
        unsigned k;
        Word table;
        if (half & 02000000) {
            k     = (half >> 15) & 037;
            table = FITS_CMD;
        } else if (half == ALIGN_END) {
            mem(EMIT_MODE) = 0;
            return;
        } else {
            k     = (half >> 12) & 077;
            table = FITS_OP;
        }
        if (k > 47 || !((table >> k) & 1)) {
            write(ALIGN_FILL);
            mem(EMIT_MODE) = 0;
            write(half);
            return;
        }
    } else if (half == ALIGN) {
        mem(EMIT_MODE) = 1;
        return;
    }
    half_ = half;
    write(half);
    if (line_ && (half & 07777) == EXTERNAL) {
        unsigned e = ext_next_ & 077777;
        while (!(mem((EXT_NAMES + 1 + e) & 077777) & EXT_USED)) {
            ext_next_ = cyclic_add(ext_next_, 3);
            e         = ext_next_ & 077777;
        }
        write(((mem((EXT_NAMES + 2 + e) & 077777) >> 15) & 077777) | EXT_HALF);
        ext_next_ = cyclic_add(ext_next_, 3);
        write(LINE_HALF | line_);
        line_ = 0;
    }
}

//
// 23734: emit the template t (the original goes through it by 02007). A half with address
// 4444 takes the operand from the stack: its address replaces 4444, or, for
// an operand with bit 48, the operand + 4000000002277777 is emitted first
// and the half keeps address 0.
//
void Pass4::emit_template(Template t)
{
    for (Word half : halves(t)) {
        if ((half & 07777) == OPERAND) {
            Word op = pop();
            if (op & BIT48) {
                emit(cyclic_add(op, INDIRECT));
                emit(half & ADDR_CLR);
                continue;
            }
            half = (half & ADDR_CLR) | op;
        }
        emit(half);
    }
}

// 23721: the value in A, if any, saved (15 ATX emitted, its cell marked).
void Pass4::save_acc()
{
    if (acc_cell_) {
        mem(acc_cell_) = ON_STACK;
        emit(ON_STACK);
        acc_cell_ = 0;
    }
}

// 23727: the operand d loaded into A (template 25772), or, with the
// accumulator in use, through the stack (template 25771).
void Pass4::load_value(Word d)
{
    push(d);
    Template t = LOAD;
    if (acc_cell_) {
        mem(acc_cell_) = ON_STACK;
        acc_cell_      = 0;
        t              = PUSH;
    }
    emit_template(t);
}

// 23725: the operand on the stack done: loaded if any (nonzero).
void Pass4::operand_done()
{
    if (Word d = pop())
        load_value(d);
    else
        acc_cell_ = 0;
}

//
// 23753: read an item and go to its handler: the code c (bits 7–1), the
// attribute (12–8) and the value (24–13); the handler of code c is in the
// table HANDLERS.
//
unsigned Pass4::dispatch_next()
{
    Word item = read();
    code_     = item & 0177;
    attr_     = (item >> 7) & 037;
    value_    = (item >> 12) & 07777;
    return HANDLERS[code_];
}

// 23752: dispatch the next item, returning to link (pushed).
unsigned Pass4::operand(unsigned link)
{
    push(link);
    return dispatch_next();
}

// 23625: the main loop: 02021 kept on the stack around each item, the
// value of an expression statement loaded.
unsigned Pass4::main_loop()
{
    push(mem(02021));
    return operand(023627);
}

//
// 23523: init. The return address is pushed; external table entries get
// their numbers (ext_count_ counts them), the names saved by pass 2
// (06000) used as externals lose their marks; then the outer block: the
// main program (named MAIN_NAME) gets the prologue (template 23612: 7 VTM,
// 13 VJM A/START, 15 UTM frame), a procedure ('CODE', 'ALGOL' module) its
// name as TEXT.
//
unsigned Pass4::start()
{
    push(DRIVER);
    ext_count_ = 0;
    for (unsigned e = 0;; e += 3) {
        Word w = mem(EXT_NAMES + 1 + e);
        if (!w)
            break;
        if (!(w & EXT_COUNT)) {
            ext_count_ = 0;
            continue;
        }
        Word v                 = (ext_count_ << 15) | mem(EXT_NAMES + 2 + e);
        mem(EXT_NAMES + 2 + e) = v;
        ext_count_             = cyclic_add(v & ADDR, ext_count_);
    }
    unsigned p = NAMES;
    bool more  = true;
    do {
        p--;
        if (!mem(p)) {
            more = false;
            break;
        }
    } while (mem(p) & BIT45);
    if (more) {
        for (unsigned e = 3; mem(EXT_NAMES + e); e += 3) {
            Word name = (mem(EXT_NAMES + 1 + e) >> 24) & 07777;
            half_     = name;
            if (!name)
                continue;
            do
                p--;
            while ((mem(p + 1) & 07777) != name);
            mem(p + 1) &= MARK_CLR;
        }
    }
    ext_next_     = 0;
    Word head     = mem(02026);
    unsigned list = head & 077777;
    mem(02015)    = head;
    store(list, 05760);
    std::copy_n(ctx_.saved_names.begin(), std::min<size_t>(1024, ctx_.saved_names.size()),
                names_.begin());
    mem(02010)     = cyclic_add(mem(02003) ^ WORD_MASK, CONST_BASE);
    acc_cell_      = 0;
    mem(02006)     = 0;
    mem(EMIT_MODE) = 0;
    mem(02021)     = 5;
    if ((mem(list + 2) & KIND) != BIT45) {
        mem(02005) = MAIN_NAME;
        mem(02014) = FRAME_MAIN;
        block_entry(list);
        store(list, packed_frame(mem(02014)) ^ mem(list));
        push(mem(02014) & ADDR);
        push(number_);
        emit_template(PROGRAM_START);
    } else {
        mem(02014) = FRAME_PROC;
        mem(02006) = mem((06144 + mem(list + 1)) & 077777);
        mem(02005) = iso_text(mem(02006));
        block_entry(list);
        store(list + 1, (Word(number_) << 24) ^ mem(list + 1));
    }
    // 23623: the location counter starts after the prologue.
    mem(02020) = number_;
    mem(02004) = number_ + 1;
    return MAIN_LOOP;
}

//
// 26152: EOP. The main program ends with UJ A/STOP (template 26160); return
// to the driver.
//
unsigned Pass4::eop()
{
    if (mem(02005) == MAIN_NAME) {
        push(mem(02020));
        emit_template(PROGRAM_END);
    }
    pop();
    pop();
    return pop() & 077777;
}

//
// Operators.
//

// 23776: return (the address on the stack) with the descriptor d in its place.
unsigned Pass4::give(Word d)
{
    unsigned ret = pop() & 077777;
    push(d);
    return ret;
}

// 24016: no value.
unsigned Pass4::give_none()
{
    return give(0);
}

// 24015: the value is in A, its cell the return address's.
unsigned Pass4::result_in_acc()
{
    acc_cell_ = (sp_ - 1) & 077777;
    return give_none();
}

// The template t, then the result in A.
unsigned Pass4::apply_template(Template t)
{
    emit_template(t);
    return result_in_acc();
}

// 23764: a string (070): its address loaded into A (template 23771, or 23773
// with A saved).
unsigned Pass4::string_item()
{
    push(constant_address());
    Template t = STRING_ADDRESS;
    if (acc_cell_) {
        t              = STRING_ADDRESS_SAVED;
        mem(acc_cell_) = ON_STACK;
    }
    return apply_template(t);
}

// 24000: a variable (017): from its descriptor.
unsigned Pass4::variable_item()
{
    return give(address_of(mem(value_)));
}

// 24010: a call without parameters (0107), or a formal by name: A saved,
// the call (template 25747, or 25751 for one on the stack).
unsigned Pass4::call_item()
{
    save_acc();
    Word d = mem(value_) & DESC_MASK;
    push(d);
    return apply_template((d & ON_STACK) ? CALL_INDIRECT : CALL);
}

// 24005: a formal by name (022): a value parameter as a variable.
unsigned Pass4::formal_item()
{
    Word d = mem(value_);
    if (!(d & BIT48))
        return call_item();
    return give(d & FORMAL);
}

// 24041: a unary operator, template t: the operand loaded, then the template.
unsigned Pass4::unary(Template t)
{
    push(t);
    return operand(024043);
}

unsigned Pass4::unary_2()
{
    operand_done();
    return apply_template(Template(pop()));
}

// 24030: a standard function n (071): 6 inline (template 25761), else the
// routine 4030+n of its argument (template 25751).
unsigned Pass4::std_function()
{
    if (value_ == 6)
        return unary(ABS);
    value_ = (value_ + 04030) & 077777;
    push(value_);
    return operand(024034);
}

// 24037: a standard procedure n without arguments (072).
unsigned Pass4::std_procedure()
{
    value_ = (value_ + 04030) & 077777;
    push(value_);
    return apply_template(CALL);
}

// 24071: the template t, the operand on the stack; the template's word dropped.
unsigned Pass4::apply(Template t)
{
    emit_template(t);
    pop();
    return result_in_acc();
}

//
// The right operand d2 popped, the left one on the stack: unless in A, it
// is loaded, and the right one takes its place.
//
void Pass4::left_to_acc()
{
    Word d1 = pop();
    Word d2 = mem((sp_ + 1) & 077777);
    if (d1) {
        sp_ = (sp_ + 2) & 077777;
        load_value(d1);
        sp_ = (sp_ - 2) & 077777;
    }
    push(d2);
}

// 24062: a binary operator, template t: both operands; the template works
// on A and the operand on the stack.
unsigned Pass4::binary(Template t)
{
    push(t);
    return operand(024064);
}

unsigned Pass4::binary_3()
{
    if (pop())
        left_to_acc();
    return apply(Template(top(2)));
}

// 24122: an operator not commutative, pair = a word of two templates: the
// left half for A op stack, the right one when the right operand is in A.
unsigned Pass4::paired(Pair pair, unsigned link)
{
    push(pair);
    return operand(link);
}

unsigned Pass4::paired_3()
{
    if (!pop())
        return apply(PAIRS[top(2)].right);
    left_to_acc();
    return apply(PAIRS[top(2)].left);
}

// 24101: real division (016) with both operands on the stack: two halves
// of its own.
unsigned Pass4::divide_3()
{
    if (top(2) != ON_STACK) // the left operand
        return paired_3();
    emit(DIV_1);
    emit(DIV_2);
    sp_ = (sp_ - 3) & 077777;
    return result_in_acc();
}

// 24222: assignment to target: the value, then the store (template 25760).
unsigned Pass4::assign(Word target)
{
    push(target);
    return operand(024223);
}

// 24240: the address computed is pushed (template 24242).
unsigned Pass4::assign_pushed()
{
    emit_template(PUSH_ADDRESS);
    return assign(BY_NAME);
}

//
// 24226: to a formal by name (0101): its address computed (template 25747)
// and kept in a register counted by 02021 from 5 down, or pushed (template
// 24242) when none is left.
//
unsigned Pass4::assign_formal()
{
    Word d = mem(value_) & FORMAL;
    if (d & BIT48)
        return assign(d);
    push(d);
    emit_template(CALL_INDIRECT);
    Word r = mem(02021);
    if (!r)
        return assign_pushed();
    emit(r | REG_STORE);
    mem(02021) = r - 1;
    return assign((r << 20) & WORD_MASK);
}

// 25301: x ** c with flag (1 real, 0 integer): c and the flag kept on the
// stack, then x: c − 1 multiplications (template 25774), then for an
// integer 25321.
unsigned Pass4::power(Word flag)
{
    push(flag);
    push(value_);
    push(0);
    return operand(025303);
}

unsigned Pass4::power_loop()
{
    for (;;) {
        Word c = cyclic_add(top(2), MINUS_ONE);
        if (c & BIT48)
            break;
        top(2) = c;
        push(top(1));
        emit_template(MULTIPLY);
    }
    sp_ = (sp_ - 2) & 077777;
    if (pop())
        return result_in_acc();
    return apply_template(INTEGER_ROUND);
}

unsigned Pass4::power_2()
{
    Word d = pop();
    if (!d) {
        top(1) = STACK_ATX1;
        emit(STACK_ATX1);
        return power_loop();
    }
    top(1) = d;
    sp_    = (sp_ + 1) & 077777;
    operand_done();
    return power_loop();
}

// 25332: an octal base ** c (0352): ASN by c.
unsigned Pass4::octal_shift_2()
{
    operand_done();
    emit(pop() | ASN);
    return result_in_acc();
}

// 25337: a routine r of two operands (pushed): the first loaded, 02017
// for A, the second, then the call (template 25751).
unsigned Pass4::library_call(unsigned r)
{
    push(r);
    return operand(025341);
}

// 25346: a string slice (050): the string, then the bounds; routine 4100
// (template 25361). attr ≠ 0: the string pushed first.
unsigned Pass4::slice_2()
{
    operand_done();
    if (!pop()) {
        acc_cell_ = 02017;
        return operand(025360);
    }
    emit(ON_STACK);
    acc_cell_ = 02017;
    return operand(025354);
}

void Pass4::register_operators()
{
    auto &n   = handlers_;
    n[023764] = [this] { return string_item(); };
    n[023775] = [this] { return give(constant_address()); };
    n[024000] = [this] { return variable_item(); };
    n[024005] = [this] { return formal_item(); };
    n[024010] = [this] { return call_item(); };
    n[RESULT] = [this] { return result_in_acc(); };
    n[024016] = [this] { return give_none(); };
    n[024021] = [this] { return unary(NEGATE); };
    n[024022] = [this] { return unary(NEGATE_INTEGER); };
    n[024023] = [this] { return unary(attr_ ? NOT_OCTAL : NOT); };
    n[024026] = [this] { return unary(TO_REAL); };
    n[024027] = [this] { return unary(TO_INTEGER); };
    n[024030] = [this] { return std_function(); };
    n[024034] = [this] {
        operand_done();
        return apply_template(CALL);
    };
    n[024037] = [this] { return std_procedure(); };
    n[024043] = [this] { return unary_2(); };
    n[024046] = [this] { return binary(ADD); };
    n[024047] = [this] { return binary(MULTIPLY); };
    n[024050] = [this] { return binary(EQUAL); };
    n[024051] = [this] { return binary(NOT_EQUAL); };
    n[024052] = [this] { return binary(OR); };
    n[024053] = [this] { return binary(attr_ ? AND : AND_CALL); };
    n[024055] = [this] { return binary(EQUIV); };
    n[024056] = [this] { return binary(ADD_INTEGER); };
    n[024057] = [this] { return binary(MULTIPLY_INTEGER); };
    n[024060] = [this] { return binary(EQUAL_MIXED); };
    n[024061] = [this] { return binary(NOT_EQUAL_MIXED); };
    n[024064] = [this] { return operand(024065); };
    n[024065] = [this] { return binary_3(); };
    n[024075] = [this] { return paired(MINUS, 024124); };
    n[024076] = [this] { return paired(DIVIDE_PAIR, 024100); };
    n[024100] = [this] { return operand(024101); };
    n[024101] = [this] { return divide_3(); };
    n[024106] = [this] { return paired(GREATER, 024124); };
    n[024107] = [this] { return paired(LESS, 024124); };
    n[024110] = [this] { return paired(GREATER_EQUAL, 024124); };
    n[024111] = [this] { return paired(LESS_EQUAL, 024124); };
    n[024112] = [this] { return paired(MINUS_INTEGER, 024124); };
    n[024124] = [this] { return operand(024125); };
    n[024125] = [this] { return paired_3(); };
    n[024221] = [this] { return assign(mem(value_) & DESC_MASK); };
    n[024223] = [this] {
        operand_done();
        return apply_template(STORE);
    };
    n[024226] = [this] { return assign_formal(); };
    n[025276] = [this] { // a real ** c (0152), an octal base (0352)
        if (attr_) {
            push(value_);
            return operand(025332);
        }
        return power(1);
    };
    n[025300] = [this] { return power(0); }; // an integer ** c (0153)
    n[025303] = [this] { return power_2(); };
    n[025306] = [this] { return power_loop(); };
    // 0150, 0151, 0147 (** by types), 001 (IMPL): routine r, or for an octal
    // operand (attr) another.
    n[025323] = [this] { return library_call(attr_ ? 04122 : 04023); };
    n[025325] = [this] { return library_call(04024); };
    n[025326] = [this] { return library_call(attr_ ? 04121 : 04025); };
    n[025332] = [this] { return octal_shift_2(); };
    n[025335] = [this] { return library_call(04026); };
    n[025336] = [this] { return library_call((value_ + 04067) & 077777); }; // string relation n
    n[025341] = [this] {
        operand_done();
        acc_cell_ = 02017;
        return operand(025343);
    };
    n[025343] = [this] {
        operand_done();
        return apply_template(CALL);
    };
    n[025346] = [this] {
        push(attr_);
        return operand(025350);
    };
    n[025350] = [this] { return slice_2(); };
    n[025353] = [this] {
        acc_cell_ = 02017;
        return operand(025354);
    };
    n[025354] = [this] {
        operand_done();
        return apply_template(STRING_SLICE);
    };
    n[025360] = [this] {
        operand_done();
        acc_cell_ = 02017;
        return operand(025354);
    };
}

//
// Jumps. LABELS (02004) is the next label number; a forward jump pushes
// its label for the item that defines it.
//

// 24142: emit the jump half, push its label, next label; on to the main loop.
unsigned Pass4::jump_ahead(Word half)
{
    emit(half);
    push(new_label());
    return MAIN_LOOP;
}

// Both operands of a comparison: unless the right one is in A, the left
// one is loaded and the right one takes its place. False when the right
// one is in A.
bool Pass4::comparison_operands()
{
    Word d2 = pop();
    if (!d2)
        return false;
    Word d1 = pop();
    if (d1) {
        sp_ = (sp_ + 2) & 077777;
        load_value(d1);
        sp_ = (sp_ - 2) & 077777;
    }
    push(d2);
    return true;
}

// 24150: compare-and-jump of strings (0134), both operands: template
// 26040, then U1A ahead.
unsigned Pass4::string_jump_3()
{
    Word d2 = pop();
    if (d2) {
        if (pop()) {
            sp_ = (sp_ + 2) & 077777;
            operand_done();
        } else {
            push(d2);
        }
    }
    emit_template(STRING_COMPARE);
    acc_cell_ = 0;
    return jump_ahead(LABEL_U1A2 | mem(LABELS));
}

//
// 24165: compare-and-jump (0130–0133), pair = a word of two templates (as
// for 24122): the label pushed under the operands; the template jumps to it.
//
unsigned Pass4::compare_jump(Pair pair)
{
    push(pair);
    push(mem(LABELS));
    return operand(024167);
}

unsigned Pass4::compare_jump_3()
{
    bool left  = comparison_operands();
    const auto &pair = PAIRS[top(3)];
    Template t       = left ? pair.left : pair.right;
    acc_cell_        = 0;
    emit_template(t);
    top(1) = new_label();
    return MAIN_LOOP;
}

// 24210: define label n (template 26051), the value in A.
unsigned Pass4::define_label(Word n)
{
    sp_         = (sp_ - 2) & 077777;
    mem(LABELS) = n;
    return apply_template(LABEL);
}

// 24213: ELSE (031): UJ to a new label, then the IF's label defined.
unsigned Pass4::else_item()
{
    sp_ = (sp_ - 2) & 077777;
    emit(LABEL_UJ | mem(LABELS));
    emit_template(LABEL);
    push(new_label());
    return MAIN_LOOP;
}

// 24666: GOTO a label (024): UJ (template 24671).
unsigned Pass4::goto_label()
{
    push(mem(value_) & DESC_MASK);
    emit_template(JUMP);
    return give_none();
}

// 24661: an element of a switch (025), the index on the stack: loaded
// (template 24360) unless in A, the half SWITCH_UJ with it, then as 024.
unsigned Pass4::switch_element()
{
    Word d = pop();
    if (d) {
        push(d);
        emit_template(INDEX);
    }
    emit(SWITCH_UJ | pop());
    return goto_label();
}

// 25023: the switch (041) with attr words on the stack: template 25040,
// then each word and NEW_WORD; the table's label back to LABELS.
unsigned Pass4::switch_end()
{
    Word d = mem(value_) & ADDR;
    push(d);
    push(d);
    emit_template(SWITCH);
    unsigned base = (sp_ - attr_) & 077777;
    for (unsigned i = 0; i < attr_; i++) {
        emit(mem((base + i) & 077777));
        emit(NEW_WORD);
    }
    sp_         = base;
    mem(LABELS) = pop();
    return DISPATCH_NEXT;
}

void Pass4::register_jumps()
{
    auto &n   = handlers_;
    n[024137] = [this] { return operand(024140); }; // IF (027): the condition
    n[024140] = [this] {
        operand_done();
        return jump_ahead(LABEL_U1A | mem(LABELS));
    };
    n[024146] = [this] { return operand(024147); };
    n[024147] = [this] { return operand(024150); };
    n[024150] = [this] { return string_jump_3(); };
    n[024161] = [this] { return compare_jump(JUMP_0133); };
    n[024162] = [this] { return compare_jump(JUMP_0130); };
    n[024163] = [this] { return compare_jump(JUMP_0131); };
    n[024164] = [this] { return compare_jump(JUMP_0132); };
    n[024167] = [this] { return operand(024170); };
    n[024170] = [this] { return compare_jump_3(); };
    n[024206] = [this] { return define_label(top(3)); }; // the end of IF (053)
    n[024207] = [this] { // the end of a conditional expression (054)
        return define_label(cyclic_add(top(3), MINUS_ONE));
    };
    n[024213] = [this] { return else_item(); };
    n[024661] = [this] { return switch_element(); };
    n[024666] = [this] { return goto_label(); };
    n[025010] = [this] { // switch elements start (0143): the table's label
        push(mem(LABELS));
        return DISPATCH_NEXT;
    };
    n[025012] = [this] { // an element expression (0144): its own label
        emit(mem(LABELS) | LABEL_DEF);
        new_label();
        return operand(025016);
    };
    n[025016] = [this] {
        top(1) = cyclic_add(mem(LABELS), PREV_LABEL);
        return DISPATCH_NEXT;
    };
    n[025020] = [this] { // an element label (0142)
        push((mem(value_) & DESC_MASK) | LABEL_UJ);
        return DISPATCH_NEXT;
    };
    n[025023] = [this] { return switch_end(); };
}

//
// Blocks. The frame size is counted in 02014 (02013 counts down from it);
// 02015 is the head of the innermost block's D-list.
//

// The descriptor at p gets offset v.
void Pass4::set_offset(unsigned p, Word v)
{
    store(p, (mem(p) & OFFSET_CLR) ^ v);
}

// The next offset of the frame.
void Pass4::offset_up(unsigned p)
{
    set_offset(p, mem(02014));
    mem(02014) = cyclic_add(mem(02014), 1);
}

// An offset counted down from 02013.
void Pass4::offset_down(unsigned p)
{
    mem(02013) = cyclic_add(mem(02013), MINUS_TWO);
    set_offset(p, mem(02013));
}

// A number counted in number_.
void Pass4::number(unsigned p)
{
    set_offset(p, number_);
    number_ = (number_ + 1) & 077777;
}

//
// An external or 'CODE' name, its ISO text at 06144+mod: the number of its
// entry in the external table (ext_text_ then in the entry's bits 36–25,
// the name as TEXT at ext_text_ − 1400), ext_text_ counting.
//
void Pass4::external_name(unsigned p, unsigned mod)
{
    Word name = mem((06144 + mod) & 077777);
    half_     = name;
    Word text = 0;
    for (unsigned e = 0;; e += 3) {
        Word w = mem(EXT_NAMES + 1 + e);
        if (!w) {
            text = iso_text(name);
            break;
        }
        if ((w & EXT_COUNT) || mem(EXT_NAMES + e) != name)
            continue;
        store(EXT_NAMES + 1 + e, (Word(ext_text_) << 24) | w);
        Word t     = (iso_text(name) >> 6) | TEXT_HEAD;
        half_      = t;
        unsigned k = 0;
        while (!((Word(077) << k) & t))
            k += 6;
        text = (k >= 6 ? Word(012) << (k - 6) : 0) | t;
        break;
    }
    store((ext_text_ + 076400) & 077777, text);
    set_offset(p, ext_text_);
    ext_text_ = (ext_text_ + 1) & 077777;
}

//
// An array (kind 2) with its dope in the frame: the frame grows by the
// dope of the bounds descriptor (bits 24–13 of the array's: its words,
// bits 12–8: their number, set to DOPE_INIT). Then the walk goes on from
// those words (p). False when the descriptor needs no offset.
//
bool Pass4::array_dope(unsigned &p)
{
    Word d = mem(p);
    if (!(d & BIT48))
        return true;
    unsigned b = (d >> 20) & 07777;
    store(p, (d & HEAD_KEEP) | packed_frame(mem(02014)));
    Word bounds = mem(b);
    Arith u(bounds >> 36, RAW);
    u.rsub(mem(02014));
    u.add(1);
    Word t = cyclic_add(u.acc, FRAME_STEP) & DOPE_MASK;
    store(p, (mem(p) & OFFSET_CLR) | t);
    Word size  = cyclic_add((bounds >> 24) & 07777, cyclic_add(t, 1)) & DOPE_MASK;
    mem(02014) = size;
    if (!(size & 076000))
        return false;
    unsigned at = (bounds >> 12) & 07777;
    unsigned n  = (bounds >> 7) & 037;
    p           = at;
    mem(02014)  = unpacked(mem(at));
    do {
        store(at++, DOPE_INIT);
        n = (n - 1) & 077777;
    } while (n);
    return true;
}

//
// 25525: block entry, the D-list at list (up to the word at its head):
// each descriptor gets its storage offset from 02014 (the frame size),
// counted up (or down from 02013 for some), or a number, or for an
// external or 'CODE' name the number of its entry in the external table.
// A word without bit 47 links to a nested list: it is followed and the
// place pushed; the inner blocks are entered after the list, each from the
// current size, the frame growing to the largest.
//
void Pass4::block_entry(unsigned list)
{
    mem(05777)    = 04067;
    mem(05773)    = 04047;
    Word saved15  = mem(02015);
    push(0); // the end of the inner blocks
    Word head     = mem(list);
    mem(02011)    = head & ADDR;
    mem(02012)    = 0;
    unsigned kind = (head >> 44) & 3;
    unsigned p    = list;
    switch (kind) {
    case 0: // a procedure: its frame descriptor
        if (head & BIT48) {
            mem(02015) = p;
            store(p, packed_frame(mem(02014)) ^ head);
            mem(02014) = cyclic_add(mem(02014), 1);
        }
        break;
    case 1: { // the body of a procedure: its frame
        mem(02012)  = head;
        Word size   = (mem(p - 1) & 03700000) >> 14;
        Word frame  = cyclic_add(mem(02014) & ON_STACK, FRAME_BASE);
        mem(02007)  = frame;
        mem(02013) = mem(02014) = frame ^ size;
        mem(02015)              = p;
        set_offset(p, frame);
        store(p - 1, mem(p - 1) & HEAD_KEEP);
        if (mem(p) & 0200000)
            offset_up(p);
        break;
    }
    default: { // a block, its size in bits 12–1 (kind 3) or 36–25 (kind 2)
        Word x = (kind == 2 ? head >> 24 : head) & 07777;
        store(p, head & HEAD_KEEP);
        Word v = mem(02014);
        if (mem(p) & BIT48) {
            store(p, mem(p) ^ (Word(number_) << 24));
            number_ = (number_ + 1) & 077777;
        } else if (v & ADDR) {
            v = cyclic_add(v, MINUS_ONE);
        }
        set_offset(p, v);
        mem(02014) = cyclic_add(cyclic_add(v, 2), x);
        break;
    }
    }
    for (;;) {
        p = (p + 1) & 077777;
        while (p != mem(02011) && !(mem(p) & BIT47)) {
            push(p); // a link: followed
            p = mem(p) & 077777;
        }
        if (p == mem(02011))
            break;
        Word d = mem(p);
        if (d & BIT46) {
            offset_down(p);
            continue;
        }
        switch ((d >> 35) & 7) { // its kind
        case 1:
            if (!(d & TYPE)) {
                if (d & BIT48)
                    store(p, (d & HEAD_KEEP) ^ ((mem(02015) << 24) & WORD_MASK));
                number(p);
            } else if (!mem(02012)) {
                offset_up(p);
            } else {
                offset_down(p);
            }
            break;
        case 2:
            if (!(d & TYPE))
                number(p);
            else if (array_dope(p))
                offset_up(p);
            break;
        case 4:
        case 5:
            if (!(d & BIT45))
                number(p);
            else
                external_name(p, d & ADDR);
            break;
        case 6:
            if (!(d & TYPE))
                number(p);
            else
                external_name(p, d & ADDR);
            break;
        case 7:
            external_name(p, 0);
            break;
        default:
            throw std::runtime_error("pass 4: descriptor kind " + std::to_string((d >> 35) & 7));
        }
    }
    // The list done: the inner blocks.
    mem(02013) = mem(02014);
    for (Word link = pop(); (link & ADDR) != 0; link = pop()) {
        push(mem(02013));
        push(mem(02014));
        mem(02014) = mem(02013);
        block_entry(link & ADDR);
        Word old = pop();
        if (!unit_less(old, mem(02014)))
            mem(02014) = old;
        mem(02013) = pop();
    }
    mem(02015) = saved15;
    if ((mem(list) & HEAD_KIND) == BIT45)
        store(list, mem(list) ^ packed_frame(mem(02014)));
}

// 24700: the frame reset to the block of descriptor d (template 24705, or
// 24707 for one by a register); then the next item.
unsigned Pass4::frame_reset(Word d)
{
    push(unpacked(d));
    emit_template((d & BIT48) ? FRAME_RESET_INDIRECT : FRAME_RESET);
    return DISPATCH_NEXT;
}

// 24673: a label definition (d<<12 | 060): 0211.n; a label that may be
// jumped to from an inner block resets the frame.
unsigned Pass4::label_item()
{
    emit((mem(value_) & DESC_MASK) | LABEL_DEF);
    Word d = mem(value_);
    if (!(d & BIT48))
        return DISPATCH_NEXT;
    return frame_reset(mem((d >> 24) & 077777));
}

// 24746: a line marker (051) or block begin (036): 0330.n when attr = 1.
unsigned Pass4::line_item()
{
    if (attr_ == 1)
        emit(value_ ^ LINE_HEAD);
    return DISPATCH_NEXT;
}

//
// 24711: a procedure (042): its entry (template 24726 with the frame size
// and the number of parameters); 02021 = parameters − 1, 02023 its kind;
// a procedure with a label inside gets PROC_LABEL with it.
//
unsigned Pass4::procedure_item()
{
    Word d      = mem(value_);
    Word before = mem((value_ - 1) & 077777);
    Word params = (d >> 20) & 017;
    push((d >> 24) & ADDR);
    push(params);
    mem(02021) = cyclic_add(params, MINUS_ONE);
    push(before & ADDR);
    emit_template(PROCEDURE_ENTRY);
    mem(02023) = (mem(value_) & 0100000) ^ 0100000;
    if (Word label = (mem((value_ - 1) & 077777) >> 24) & ADDR)
        emit(label | PROC_LABEL);
    return MAIN_LOOP;
}

// 24730: the end of a procedure body (062): the return (RETURN_1,
// RETURN_2), then 0211 of the label after it or NEW_WORD.
unsigned Pass4::procedure_end()
{
    sp_    = (sp_ - 2) & 077777;
    Word d = mem(value_);
    if (d & 0200000)
        emit((d & DESC_MASK) | FRAME_REG);
    Word r     = mem(value_) & ON_STACK;
    mem(02021) = r >> 20;
    emit(r | RETURN_1);
    emit(RETURN_2);
    Word label = (mem((value_ - 1) & 077777) >> 24) & ADDR;
    emit(label ? label | LABEL_DEF : NEW_WORD);
    return DISPATCH_NEXT;
}

void Pass4::register_blocks()
{
    auto &n   = handlers_;
    n[024673] = [this] { return label_item(); };
    n[024711] = [this] { return procedure_item(); };
    n[024730] = [this] { return procedure_end(); };
    n[024746] = [this] { return line_item(); };
    n[024752] = [this] { return frame_reset(mem(value_)); }; // the end of a block (037)
}

//
// Arrays. Subscripts are pushed as operands, a constant one (045) as c and
// 0 above it.
//

// *0620B: the next subscript: on the stack (1) or loaded through 02011 (3).
unsigned Pass4::next_subscript()
{
    Word x = pop();
    if (!x)
        return 1;
    push(x);
    emit(mem(02011));
    emit_template(INDEX);
    top(1) |= mem(02011) & ON_STACK;
    return 3;
}

//
// 24324 (24327 without a register for a formal: from 02021): the address
// of an element of the array at value_, attr subscripts on the stack: 02011
// = the half that indexes it. Returns k: 0 all constant (the address on
// the stack), 1 in a register, 2 in A, 3 in a register and on the stack.
//
unsigned Pass4::subscript(bool counted)
{
    Word r     = mem(02021);
    mem(02011) = counted && r ? ((r << 20) & WORD_MASK) | INDEX_REG : INDEX_A;
    Word d     = mem(value_);
    unsigned k;
    if (d & BIT48) { // its dope: a constant subscript added to it
        top(2) = cyclic_add(d & DOPE_MASK, top(2)) & DOPE_MASK;
        Word x = pop();
        k      = 0;
        if (x) {
            push(x);
            emit_template(INDEX);
            k = 2;
        }
    } else {
        emit((d & DESC_MASK) | ARRAY_HALF);
        k = next_subscript();
    }
    for (unsigned n = (attr_ - 1) & 077777; n; n--) {
        emit_template(INDEX);
        k = next_subscript();
    }
    return k;
}

// The element's half (24262…24264) with the address on the stack: by k,
// 02011 or ELEMENT_REG, or ELEMENT_A and then 02011.
void Pass4::emit_element(unsigned k)
{
    if (k == 1 || k == 3) {
        emit((k == 1 ? mem(02011) : ELEMENT_REG) | pop());
        return;
    }
    emit(ELEMENT_A | pop());
    emit(mem(02011));
}

// 24252: an element read (044): the address, loaded unless constant.
unsigned Pass4::element_read()
{
    unsigned k = subscript(true);
    Word a     = pop();
    if (k == 0)
        return give(a);
    load_value(a);
    return result_in_acc();
}

//
// 24260: an element stored (046): its address, A/SUBCH in check mode
// (template 24303), then the assignment through a register counted by
// 02021 (or pushed).
//
unsigned Pass4::element_store()
{
    unsigned k = subscript(true);
    if (k == 0)
        return assign(pop());
    emit_element(k);
    if (ctx_.options.check_mode) {
        if (Word r = mem(02021))
            emit(((r << 20) & WORD_MASK) | CHECK_REG);
        push(address_of(mem(value_)));
        emit_template(CHECK_SUBSCRIPT);
    }
    Word r = mem(02021);
    if (!r)
        return assign_pushed();
    mem(02021) = r - 1;
    return assign(mem(02011) & ON_STACK);
}

// 24311: the address of an element (0103), loaded (ADDRESS_OP).
unsigned Pass4::element_address()
{
    emit_element(subscript(false));
    push(ADDRESS_OP);
    operand_done();
    return result_in_acc();
}

//
// 24616: an array segment (040) of the array at value_: its routine (4021,
// 4022 by kind; or for a formal 4131 with the number of its external entry
// in line_), then each bound loaded and pushed (02017 for A) until 0104.
//
unsigned Pass4::array_segment()
{
    half_      = value_;
    unsigned r = 0;
    for (unsigned e = 0; Word w = mem(EXT_NAMES + 1 + e);) {
        e += 3;
        if (!(w >> 36)) {
            emit_link_ = w >> 24;
            continue;
        }
        if ((w >> 36) == half_) {
            r     = 04131;
            line_ = emit_link_;
            break;
        }
    }
    Word d = mem(value_);
    if (!r)
        r = (d & BIT48) ? 04022 : 04021;
    push(r);
    push(address_of(d));
    push(attr_);
    return operand(024644);
}

// 25400: step register R (attr) by c (0165): UTM c, or for c = 0 template
// 25410.
unsigned Pass4::register_step()
{
    if (value_) {
        emit(cyclic_add(Word(attr_) << 20, value_) | ELEMENT_REG);
        return DISPATCH_NEXT;
    }
    push(Word(attr_) << 20);
    push(attr_ + 020);
    emit_template(STEP_REGISTER);
    return DISPATCH_NEXT;
}

// 25416: the preamble of an inner loop (0167) with its control variable,
// initial value, step and until value on the stack; template 25441 (or
// 25447 when the step is not 1).
unsigned Pass4::preamble_4()
{
    unsigned at = (sp_ - 4) & 077777;
    Word cv = mem(at), init = mem(at + 1), step = mem(at + 2), until = mem(at + 3);
    Word label = mem(LABELS);
    Template t = PREAMBLE_CYCLIC;
    push(cyclic_add(label, 2));
    if (!(loop_descriptor() & STEP_ONE)) {
        t = PREAMBLE;
        push(step);
    }
    for (Word w : { until, cv, label, step, cv, cyclic_add(label, 1), label, init })
        push(w);
    emit_template(t);
    sp_ = (sp_ - 4) & 077777;
    return DISPATCH_NEXT;
}

// 25433: the end of a preamble (0166): template 25455 with the count of
// registers (attr).
unsigned Pass4::preamble_end()
{
    Word label = mem(LABELS);
    push(cyclic_add(label, 1));
    Arith u(attr_, RAW);
    u.rsub(1);
    push(u.acc & 07777);
    push(cyclic_add(label, 2));
    emit_template(PREAMBLE_END);
    return DISPATCH_NEXT;
}

// 25460: load a register with the address of an element (0162): the
// element item read here; then 02021 = 1.
unsigned Pass4::register_load()
{
    mem(02021) = (read() >> 7) & 037;
    emit_element(subscript(true));
    mem(02021) = 1;
    return DISPATCH_NEXT;
}

// Push the words at offsets (from the top on entry), template t, drop n.
unsigned Pass4::loop_template(std::initializer_list<unsigned> offsets, Template t, unsigned n)
{
    unsigned s = sp_;
    for (unsigned k : offsets)
        push(mem((s - k) & 077777));
    emit_template(t);
    sp_ = (sp_ - n) & 077777;
    return DISPATCH_NEXT;
}

void Pass4::register_arrays()
{
    auto &n   = handlers_;
    n[024244] = [this] { // a constant subscript (045)
        push(value_);
        push(0);
        return DISPATCH_NEXT;
    };
    n[024246] = [this] { // subscript + c (030): c, then the expression
        push(value_);
        return operand(024250);
    };
    n[024250] = [this] {
        if (!top(1))
            save_acc();
        return DISPATCH_NEXT;
    };
    n[024252] = [this] { return element_read(); };
    n[024260] = [this] { return element_store(); };
    n[024311] = [this] { return element_address(); };
    n[024616] = [this] { return array_segment(); };
    n[024644] = [this] { // a bound
        operand_done();
        acc_cell_ = 02017;
        return operand(024644);
    };
    n[024646] = [this] { // the bounds end (0104): the call (template 24651)
        top(1)    = attr_;
        acc_cell_ = 0;
        emit_template(BLOCK_ARRAYS);
        return DISPATCH_NEXT;
    };
    n[024655] = [this] { // a block with arrays (0102): its frame
        emit(DOPE_HALF);
        emit(unpacked(mem(value_)));
        return DISPATCH_NEXT;
    };
    // Items made by pass 3: an element through index register R (attr) at
    // offset d (value).
    n[025363] = [this] { return give(cyclic_add(value_, mem(02027))); }; // 0161
    n[025365] = [this] { return give(cyclic_add(Word(attr_) << 20, value_)); }; // 0163
    n[025370] = [this] { // 0160: d + 02027 loaded, the half kept
        push(cyclic_add(value_, mem(02027)));
        return operand(025372);
    };
    n[025372] = [this] {
        operand_done();
        emit(pop());
        return DISPATCH_NEXT;
    };
    n[025375] = [this] { return assign(cyclic_add(Word(attr_) << 20, value_)); }; // 0164
    n[025400] = [this] { return register_step(); };
    n[025412] = [this] {
        push(mem(value_) & DESC_MASK);
        return operand(025414);
    };
    n[025414] = [this] { return operand(025415); };
    n[025415] = [this] { return operand(025416); };
    n[025416] = [this] { return preamble_4(); };
    n[025433] = [this] { return preamble_end(); };
    n[025460] = [this] { return register_load(); };
    // 25475: a FOR loop with registers: its control variable, the next item,
    // the elements; template 25522 (or 25516 with bit 0100000).
    n[025477] = [this] {
        read();
        return operand(025501);
    };
    n[025501] = [this] { return loop_template({ 12, 11, 1, 2 }, LOOP_CALL_2, 2); };
    n[025507] = [this] { return operand(025510); };
    n[025510] = [this] { return operand(025511); };
    n[025511] = [this] { return loop_template({ 13, 12, 2, 1, 3 }, LOOP_CALL_3, 3); };
}

//
// FOR loops. 055 reserves 10 stack words for the loop (S−10 … S−1 below,
// top(k) while the stack is at S): S−10 L, S−9 L+1, S−8 L+2 its labels;
// S−7 the control variable's address, S−6 the loop descriptor's word, S−5
// the control variable (032/0141), S−4 the step, S−3 a constant step, S−2
// the jump to the body after the elements, S−1 the kind of the element.
// 02031 = the loop descriptor; with registers (bits 0300000 of it, from
// pass 3) the elements go to 25475.
//

// 25042: FOR (055).
unsigned Pass4::for_item()
{
    sp_         = (sp_ + 012) & 077777;
    mem(02031)  = value_;
    Word label  = mem(LABELS);
    top(10)     = label;
    top(9)      = cyclic_add(label, 1);
    top(8)      = cyclic_add(label, 2);
    mem(LABELS) = cyclic_add(label, 3);
    top(2)      = 0;
    Word d      = mem(value_);
    top(6)      = (d >> 24) & ADDR;
    top(7)      = d & DESC_MASK;
    mem(02027)  = cyclic_add(top(7), 2);
    if (d & 0400000) {
        mem(02030) = mem(02021);
        mem(02021) = 1;
    }
    return DISPATCH_NEXT;
}

// 25075: the half with label L+1, then (unless the descriptor word is 0)
// 0211 of it; on to the main loop.
unsigned Pass4::for_jump(Word half)
{
    emit(half | top(9));
    if (Word w = top(6))
        emit(w | LABEL_DEF);
    return MAIN_LOOP;
}

// 25071: DO (033): the jump to the body (S−2, U1A or UJ).
unsigned Pass4::do_item()
{
    if (loop_descriptor() & BY_REGISTERS)
        return MAIN_LOOP;
    if (Word j = top(2))
        return for_jump(j ^ 0100000);
    if (!top(6))
        return MAIN_LOOP;
    return for_jump(LABEL_UJ);
}

// 25103: the end of a FOR (052): UJ back to L (or by the step), 0211 of
// L+1; the frame dropped, 02004 = L.
unsigned Pass4::for_end()
{
    sp_ = (sp_ - 2) & 077777;
    if (mem(value_) & BY_REGISTERS) {
        emit(top(10) | UJ_BACK);
    } else {
        Word target = top(10);
        if (top(6)) {
            emit(top(7) | ARRAY_HALF);
            target = 0;
        }
        emit(target | LABEL_UJ);
    }
    emit(LABEL_DEF | top(9));
    mem(LABELS) = top(10);
    sp_         = (sp_ - 10) & 077777;
    if (mem(value_) & 0400000)
        mem(02021) = mem(02030);
    return DISPATCH_NEXT;
}

//
// 25132: an element of kind k: 0 STEP-UNTIL integer (034), 1 real (0140),
// 2 integer constant step (0137), 3 WHILE (035), 4 plain (056). The jump
// of the element before (S−2), the address (template 25174) and 0211 L
// for the first; then the value, and its continuation by kind from the
// table at 25151.
//
unsigned Pass4::element_item(unsigned k)
{
    top(1)       = k;
    Word regs    = loop_descriptor() & BY_REGISTERS;
    if (regs) // 25475: the control variable, then the elements
        return operand((regs & 0100000) ? 025477 : 025507);
    if (Word j = top(2))
        emit(j | top(6));
    if (top(6)) {
        Word a = top(7), label = top(10);
        push(a);
        push(label);
        emit_template(SAVE_FRAME);
    }
    if (top(1) == 3)
        emit(top(10) | LABEL_DEF);
    return operand(025147);
}

// The value stored into the control variable (template 25760).
void Pass4::store_control()
{
    push(top(5));
    emit_template(STORE);
}

// 25163: a plain element: stored, UJ to the body, 0211 of L.
unsigned Pass4::plain_value()
{
    store_control();
    Word w = top(6);
    if (!w) {
        top(9) = top(10);
        return DISPATCH_NEXT;
    }
    emit(w | LABEL_UJ);
    top(2) = 0;
    emit(top(10) | LABEL_DEF);
    return DISPATCH_NEXT;
}

// 25206: template t, going on by kind at 25210 + kind.
unsigned Pass4::by_kind(Template t, Word kind)
{
    count_ = kind & 077777;
    emit_template(t);
    return 025210 + count_;
}

// 25200: STEP-UNTIL: the initial value stored (template 25235), then the
// step: a constant one (023, attr 0) is kept in S−3; in A: 25247.
unsigned Pass4::step_operand()
{
    Word d = pop();
    if (!d)
        return step_in_acc();
    top(4) = d;
    code_  = (code_ - 023) & 077777;
    if (!code_ && !attr_)
        top(3) = d;
    Word l0 = top(10), l1 = top(9), kind = top(1);
    push(d);
    push(l0);
    push(l1);
    return by_kind(STEP_START, kind);
}

// 25222: for a step not constant the sign test (template 25227); U1A to
// the body.
unsigned Pass4::until_test()
{
    if (!top(3)) {
        push(top(4));
        emit_template(SIGN_TEST);
    }
    top(2) = LABEL_U1A;
    return DISPATCH_NEXT;
}

// 25217: the until value: compared (template 26020, or 26017 when in A).
unsigned Pass4::until_value()
{
    Word d = pop();
    if (!d)
        return operand(025231);
    push(d);
    emit_template(SUBTRACT_FROM);
    return until_test();
}

// 25247: a step in A: kept in a temporary (template 25265, the until
// after it), then by kind with template 25270.
unsigned Pass4::step_in_acc()
{
    Word temp = cyclic_add(top(7) & DESC_MASK, 1);
    Word l0 = top(10), l1 = top(9), cv = top(5);
    for (Word w : { l0, temp, l1, cv })
        push(w);
    emit_template(STEP_TEMPORARY);
    return operand(025255);
}

unsigned Pass4::step_in_acc_2()
{
    operand_done();
    Word temp = cyclic_add(top(7) & DESC_MASK, 1);
    Word l1 = top(9), l2 = top(8), cv = top(5), kind = top(1);
    top(4)    = ON_STACK;
    for (Word w : { l2, l1, cv, temp, l2, l1, temp })
        push(w);
    return by_kind(STEP_IN_TEMPORARY, kind);
}

void Pass4::register_loops()
{
    auto &n   = handlers_;
    n[025042] = [this] { return for_item(); };
    n[025062] = [this] { // the control variable, simple (032)
        top(5) = mem(value_) & DESC_MASK;
        return DISPATCH_NEXT;
    };
    n[025064] = [this] { // … or by name (0141)
        top(5) = (mem(value_) & DESC_MASK) | BIT48;
        return DISPATCH_NEXT;
    };
    n[025067] = [this] { // a by-name element (0135)
        if (!(mem(value_) & BIT48))
            return give_none();
        return DISPATCH_NEXT;
    };
    n[025071] = [this] { return do_item(); };
    n[025103] = [this] { return for_end(); };
    n[025125] = [this] { return element_item(1); };
    n[025126] = [this] { return element_item(0); };
    n[025127] = [this] { return element_item(2); };
    n[025130] = [this] { return element_item(4); };
    n[025131] = [this] { return element_item(3); };
    n[025147] = [this] { // the element's value: on by its kind
        unsigned next = BY_KIND_NEXT[top(2)];
        operand_done();
        return next;
    };
    n[025156] = [this] { // WHILE: stored; the condition, U1A next
        store_control();
        top(2) = LABEL_U1A2;
        return operand(025162);
    };
    n[025162] = [this] {
        operand_done();
        return DISPATCH_NEXT;
    };
    n[025163] = [this] { return plain_value(); };
    n[025176] = [this] { // STEP-UNTIL
        top(3) = 0;
        return operand(025200);
    };
    n[025200] = [this] { return step_operand(); };
    // 25210…25212 by kind: the increment (templates 25243, 25237, 25241),
    // then the until value.
    n[025210] = [this] {
        loop_template({ 5, 9, 5 }, LOOP_STEP_INTEGER, 0);
        return operand(025217);
    };
    n[025211] = [this] {
        loop_template({ 5, 9, 5 }, LOOP_STEP, 0);
        return operand(025217);
    };
    n[025212] = [this] {
        loop_template({ 5, 9, 5 }, LOOP_STEP_CYCLIC, 0);
        return operand(025217);
    };
    n[025217] = [this] { return until_value(); };
    n[025222] = [this] { return until_test(); };
    n[025231] = [this] {
        operand_done();
        push(top(5));
        emit_template(SUBTRACT);
        return until_test();
    };
    n[025255] = [this] { return step_in_acc_2(); };
}

//
// Procedures. 02017 is a label still to be defined (0211) before the next
// code: after a thunk, the place its jump around goes.
//

// 24372: define the pending label, if any.
void Pass4::pending_label()
{
    if (Word label = mem(02017)) {
        emit(label | LABEL_DEF);
        mem(LABELS) = label;
        mem(02017)  = 0;
    }
}

// 24407: the call of the procedure d: template t, or 24420 for one on the
// stack; the result in A.
unsigned Pass4::call_tail(Template t, Word d)
{
    push(d);
    return apply_template((d & ON_STACK) ? CALL_ON_STACK : t);
}

// 24426: an argument a, template t.
unsigned Pass4::argument(Template t, Word a)
{
    push(a);
    pending_label();
    emit_template(t);
    return DISPATCH_NEXT;
}

// 24452: a thunk starts (0117): the argument (template 24464) and a jump
// around the thunk's code to a new label, pushed.
unsigned Pass4::thunk_start()
{
    pending_label();
    push(((mem(02021) << 20) & WORD_MASK) | mem(LABELS));
    emit_template(ARG_THUNK);
    push(new_label());
    return MAIN_LOOP;
}

// 24767, 24776: a value parameter (061), a real one (0154): its descriptor
// marked; the copy (template 25002, 24777) unless 02023 ≠ 0.
unsigned Pass4::value_parameter(Template t)
{
    Word d = cyclic_add(mem(value_), 1);
    store(value_, d);
    push(d & DESC_MASK);
    push(cyclic_add(d & DESC_MASK, MINUS_ONE));
    if (!mem(02023))
        emit_template(t);
    else
        sp_ = (sp_ - 2) & 077777;
    return DISPATCH_NEXT;
}

//
// Non-ALGOL ('CODE') calls. Each argument pushes its descriptor and the
// continuation (passer) that passes it later, then the next item; the call
// (0126, 073, 065) passes them in order, then calls. count_ counts
// arguments that are values.
//

// 24505: push a and the passer, then the next item.
unsigned Pass4::push_argument(Word a, unsigned passer)
{
    push(a);
    push(passer);
    return DISPATCH_NEXT;
}

// 24513: a value kept in a stack word (count_, saved under the operand,
// counting), passed by passer.
unsigned Pass4::code_value(unsigned passer)
{
    count_    = (pop() - 1) & 077777;
    acc_cell_ = 02017;
    return push_argument(0, passer);
}

//
// 24553: the call of routine r, count words of results: the attr arguments
// are passed in order (each by its passer, arg_ at its pair, 02023
// counting from the count), then the call (template 25751); the stack is
// reset and a result of count_ words in A.
//
unsigned Pass4::code_call(unsigned r, Word count)
{
    routine_   = r & 077777;
    mem(02023) = (count & 07777) | ON_STACK;
    save_acc();
    arg_  = (sp_ - 2 * attr_) & 077777;
    args_ = arg_;
    return mem((arg_ + 1) & 077777) & 077777;
}

// 24567: pass the argument by template t, then the next one or the call.
unsigned Pass4::pass_argument(Template t)
{
    emit_template(t);
    mem(02023) = cyclic_add(mem(02023), MINUS_ONE);
    arg_       = (arg_ + 2) & 077777;
    attr_      = (attr_ - 1) & 077777;
    if (attr_)
        return mem((arg_ + 1) & 077777) & 077777;
    push(routine_);
    emit_template(CALL);
    sp_ = args_;
    if (count_)
        emit(cyclic_add(count_, RESULT_SIZE));
    return RESULT;
}

// 24603, 24607: pass a value from its stack word (02023 counting).
unsigned Pass4::pass_value(bool result)
{
    Word c = mem(02023);
    push(result ? cyclic_add(c, MINUS_ONE) : c);
    mem(02023) = cyclic_add(c, 1);
    return pass_argument(result ? PUSH : PASS_ADDRESS);
}

void Pass4::register_procedures()
{
    auto &n   = handlers_;
    n[024361] = [this] { // a call with parameters starts (0110): the frame
        Template t = CALL_FRAME; // of the arguments (CALL_FRAME_SAVED with A saved)
        if (acc_cell_) {
            mem(acc_cell_) = ON_STACK;
            acc_cell_      = 0;
            t              = CALL_FRAME_SAVED;
        }
        mem(02017) = 0;
        emit_template(t);
        return DISPATCH_NEXT;
    };
    n[024377] = [this] { // a standard function with an expression argument
        pending_label(); // (n<<12 | 057): routine 2n
        value_ = (2 * value_) & 077777;
        return call_tail(STD_FUNCTION_CALL, value_);
    };
    n[024405] = [this] { // the call of a procedure with parameters (020)
        pending_label();
        return call_tail(CALL_PARAMETERS, mem(value_) & DESC_MASK);
    };
    n[024412] = [this] { // … without parameters (021)
        save_acc();
        return call_tail(CALL_NO_PARAMETERS, mem(value_) & DESC_MASK);
    };
    // Arguments: a string (0156), a constant (0111), a variable (0112), a
    // label (0114), an array (0115), a procedure (0113, with 02021).
    n[024423] = [this] { return argument(ARG_STRING, constant_address()); };
    n[024424] = [this] { return argument(ARG_CONSTANT, constant_address()); };
    n[024430] = [this] { return argument(ARG_VARIABLE, mem(value_) & DESC_MASK); };
    n[024432] = [this] { return argument(ARG_ADDRESS, mem(value_) & DESC_MASK); };
    n[024434] = [this] { return argument(ARG_ADDRESS, address_of(mem(value_))); };
    n[024440] = [this] {
        return argument(ARG_PROCEDURE, ((mem(02021) << 20) & WORD_MASK) | (mem(value_) & ADDR));
    };
    n[024444] = [this] { // an expression argument (0120): its value
        pending_label();
        acc_cell_ = 02017;
        return operand(024446);
    };
    n[024446] = [this] {
        operand_done();
        mem(02017) = 0;
        emit_template(ARG_EXPRESSION);
        return DISPATCH_NEXT;
    };
    n[024452] = [this] { return thunk_start(); };
    n[024461] = [this] { // a thunk ends (0116): its return; its label pending
        sp_        = (sp_ - 2) & 077777;
        mem(02017) = pop();
        emit(RETURN_2);
        return DISPATCH_NEXT;
    };
    n[024755] = [this] { // a jump around procedure bodies (0146): UJ to a
        Word label = new_label(); // new label, pushed twice
        push(label);
        push(label);
        emit_template(JUMP);
        return DISPATCH_NEXT;
    };
    n[024761] = [this] { // its target (0145): the label defined
        mem(LABELS) = top(1);
        emit_template(LABEL);
        return DISPATCH_NEXT;
    };
    // VALUE ends (0155), starts (0127): for a procedure with value
    // parameters (02023 = 0) a half with 02004.
    n[024764] = [this] {
        if (!mem(02023))
            emit(VALUE_START | mem(LABELS));
        return DISPATCH_NEXT;
    };
    n[025005] = [this] {
        if (!mem(02023))
            emit(LABEL_DEF | mem(LABELS));
        return DISPATCH_NEXT;
    };
    n[024767] = [this] { return value_parameter(VALUE); };
    n[024776] = [this] { return value_parameter(VALUE_REAL); };

    // Non-ALGOL calls.
    n[024502] = [this] { // no argument count (0121)
        count_ = 0;
        return DISPATCH_NEXT;
    };
    n[024503] = [this] { return push_argument(mem(value_) & DESC_MASK, 024565); }; // 0122
    n[024531] = [this] { return push_argument(constant_address(), 024565); };     // 066
    n[024516] = [this] { return push_argument(address_of(mem(value_)), 024602); }; // 0123
    n[024507] = [this] { // an expression (0125), passed by 24603
        push(count_);
        return operand(024511);
    };
    n[024511] = [this] {
        operand_done();
        return code_value(024603);
    };
    // 24523: an expression of a non-ALGOL type (0124): in A, as 074's
    // result; else its address, passed by 24612.
    n[024523] = [this] {
        push(count_);
        return operand(024525);
    };
    n[024525] = [this] {
        Word d = pop();
        if (!d) {
            emit(CODE_RESULT);
            return code_value(024607);
        }
        count_ = pop() & 077777;
        return push_argument(d & DESC_MASK, 024612);
    };
    // 24533: a call of an ALGOL procedure as argument (074): routine 4111
    // (template 25751), the result passed by 24607.
    n[024533] = [this] {
        push(count_);
        return operand(024535);
    };
    n[024535] = [this] {
        operand_done();
        push(04111);
        emit_template(CALL);
        return code_value(024607);
    };
    n[024547] = [this] { return code_call(mem(value_) & ADDR, count_); }; // 0126
    n[024551] = [this] { return code_call(value_ + 04030, count_); };     // 073
    n[024562] = [this] { // 065: one result fewer
        Word count = count_;
        count_     = (count_ - 1) & 077777;
        return code_call(mem(value_) & ADDR, count);
    };
    // Passers: the argument word at arg_ (template 24543, 24545, 24613), or
    // a value.
    n[024565] = [this] {
        push(mem(arg_));
        return pass_argument(PASS_ADDRESS);
    };
    n[024602] = [this] {
        push(mem(arg_));
        return pass_argument(PASS_ARRAY);
    };
    n[024612] = [this] {
        push(mem(arg_));
        return pass_argument(PASS_INDIRECT);
    };
    n[024603] = [this] { return pass_value(false); };
    n[024607] = [this] { return pass_value(true); };
}

Pass4::Pass4(Context &ctx, const Stream &input) : ctx_(ctx), in_(input)
{
    auto &n         = handlers_;
    n[MAIN_LOOP]     = [this] { return main_loop(); };
    n[023627]        = [this] { // an expression statement: its value loaded
        operand_done();
        mem(02021) = pop();
        return MAIN_LOOP;
    };
    n[DISPATCH_NEXT] = [this] { return dispatch_next(); };
    n[026152]        = [this] { return eop(); };
    register_operators();
    register_jumps();
    register_blocks();
    register_arrays();
    register_loops();
    register_procedures();
}

Stream Pass4::run()
{
    unsigned pc = start();
    while (pc != DRIVER) {
        auto it = handlers_.find(pc);
        if (it == handlers_.end())
            throw std::runtime_error("pass 4: no handler at " + std::to_string(pc));
        pc = it->second();
    }
    return out_;
}

} // namespace

Stream pass4(Context &ctx, const Stream &input)
{
    return Pass4(ctx, input).run();
}
