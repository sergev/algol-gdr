//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "pass2.h"

#include <array>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "arith.h"
#include "card.h"
#include "listing.h"

namespace {

using namespace std::string_view_literals;

// Cells of the workspace.
constexpr unsigned FREE_CONST  = 02003; // the lowest constant slot
constexpr unsigned DLIST_TOP   = 02004; // the innermost procedure (or thunk, then 0)
constexpr unsigned HIDDEN      = 02005; // a block whose names are not visible here
constexpr unsigned ITEM        = 02006; // the item read
constexpr unsigned ARGS        = 02007; // data of the messages: the name, 0, the position
constexpr unsigned POSITION    = 02011; // 6 words: "PROGR/END" before any marker
constexpr unsigned LINE_TEXT   = 02015; // of POSITION: the line number
constexpr unsigned STMT_TEXT   = 02016; // the statement on the line
constexpr unsigned BLOCK_END   = 02017; // the end of the block being scanned
constexpr unsigned ARGS2       = 02017; // the second name of NO ACTUAL-FORMAL CORRESPONDENCE
constexpr unsigned BLOCK_DLIST = 02022; // thunks counted, <<12
constexpr unsigned STEP_KIND   = 02023; // the last item of a STEP element: 0137, 0140, 034
constexpr unsigned ELEMENTS    = 02024; // the elements of the FOR list
constexpr unsigned FREE_SAVED  = 02025;
constexpr unsigned DLIST_TOP2  = 02026; // the outermost block
constexpr unsigned REPORTED    = 02027; // a message in this subtree
constexpr unsigned BLOCK       = 02030; // the innermost open block
constexpr unsigned BOUNDS      = 02031; // constant bounds of an array segment, or 0
constexpr unsigned ARRAYS_END  = 02032; // end of the array records at 04000
constexpr unsigned NAMES       = 02144; // slot i at NAMES + i
constexpr unsigned CELLS       = 01736; // the work cells of the original
constexpr unsigned ARRAYS      = 04000;
constexpr unsigned DLIST_END   = 06000;
constexpr unsigned COMMON      = 027245; // 3 words per element: name, D3<<36 | slot<<24, size
constexpr unsigned FLGEX       = 00702;

constexpr unsigned ONE        = 01632; // the slot of the constant 1
constexpr unsigned TRUE_SLOT  = 01625;
constexpr unsigned FALSE_SLOT = 01624;
constexpr unsigned STD_PROC   = 05776; // the standard procedure with an expression argument

// Per item code 0-0105 (TYPES at 21720…): the handler (a UJ in the high
// half) and the classes of the code in some contexts.
struct CodeAttr {
    unsigned handler;
    unsigned subscript; // how a subscript ends: 1 a constant, 2 +, 3 -
    bool variable;      // can be a left part
    unsigned condition; // a relation 1-6
    unsigned actual;    // the kind of an actual parameter
    unsigned io;        // an argument of I/O: 1 a name, 2 a string, 3 an element
};

// clang-format off
const CodeAttr CODES[0106] = {
    { 017605, 0, 1, 0, 1, 1 }, // 000
    { 020325, 0, 0, 0, 0, 0 }, // 001
    { 020325, 0, 0, 0, 0, 0 }, // 002
    { 020325, 0, 0, 0, 0, 0 }, // 003
    { 020350, 0, 0, 0, 0, 0 }, // 004
    { 020354, 0, 0, 1, 0, 0 }, // 005
    { 020354, 0, 0, 2, 0, 0 }, // 006
    { 020354, 0, 0, 3, 0, 0 }, // 007
    { 020354, 0, 0, 4, 0, 0 }, // 010
    { 020354, 0, 0, 5, 0, 0 }, // 011
    { 020354, 0, 0, 6, 0, 0 }, // 012
    { 020106, 2, 0, 0, 0, 0 }, // 013
    { 020152, 3, 0, 0, 0, 0 }, // 014
    { 020153, 0, 0, 0, 0, 0 }, // 015
    { 020154, 0, 0, 0, 0, 0 }, // 016
    { 020221, 0, 0, 0, 0, 0 }, // 017
    { 020571, 0, 0, 0, 0, 0 }, // 020
    { 020002, 0, 1, 0, 6, 3 }, // 021
    { 017675, 0, 0, 0, 0, 0 }, // 022
    { 017770, 0, 0, 0, 3, 0 }, // 023
    { 017772, 1, 0, 0, 2, 0 }, // 024
    { 017775, 0, 0, 0, 8, 2 }, // 025
    { 017646, 0, 0, 0, 0, 0 }, // 026
    { 022026, 0, 0, 0, 0, 0 }, // 027
    { 021461, 0, 0, 0, 0, 0 }, // 030
    { 021206, 0, 0, 0, 0, 0 }, // 031
    { 021617, 0, 0, 0, 0, 0 }, // 032
    { 021524, 0, 0, 0, 0, 0 }, // 033
    { 021546, 0, 0, 0, 0, 0 }, // 034
    { 021540, 0, 0, 0, 0, 0 }, // 035
    { 017523, 0, 0, 0, 0, 0 }, // 036
    { 017511, 0, 0, 0, 0, 0 }, // 037
    { 020200, 0, 0, 0, 0, 0 }, // 040
    { 021637, 0, 0, 0, 0, 0 }, // 041
    { 017544, 0, 0, 0, 0, 0 }, // 042
    { 020325, 0, 0, 0, 0, 0 }, // 043
    { 017533, 0, 0, 0, 0, 0 }, // 044
    { 021366, 0, 0, 0, 0, 0 }, // 045
    { 020424, 0, 0, 0, 0, 0 }, // 046
    { 020430, 0, 0, 0, 0, 0 }, // 047
    { 020546, 0, 0, 0, 0, 0 }, // 050
    { 017535, 0, 0, 0, 0, 0 }, // 051
    { 017601, 0, 0, 0, 0, 0 }, // 052
    { 021206, 0, 0, 0, 0, 0 }, // 053
    { 021206, 0, 0, 0, 0, 0 }, // 054
    { 017543, 0, 0, 0, 0, 0 }, // 055
    { 021534, 0, 0, 0, 0, 0 }, // 056
    { 022026, 0, 0, 0, 0, 0 }, // 057
    { 021206, 0, 0, 0, 0, 0 }, // 060
    { 021210, 0, 0, 0, 0, 0 }, // 061
    { 017574, 0, 0, 0, 0, 0 }, // 062
    { 020446, 0, 0, 0, 0, 0 }, // 063
    { 020445, 0, 0, 0, 0, 0 }, // 064
    { 020466, 0, 0, 0, 0, 0 }, // 065
    { 020541, 0, 0, 0, 0, 0 }, // 066
    { 020542, 0, 0, 0, 0, 0 }, // 067
    { 020535, 0, 0, 0, 0, 0 }, // 070
    { 020534, 0, 0, 0, 0, 0 }, // 071
    { 020536, 0, 0, 0, 0, 0 }, // 072
    { 020521, 0, 0, 0, 0, 0 }, // 073
    { 020511, 0, 0, 0, 0, 0 }, // 074
    { 022026, 0, 0, 0, 0, 0 }, // 075
    { 017765, 0, 0, 0, 4, 0 }, // 076
    { 017766, 0, 0, 0, 5, 0 }, // 077
    { 020447, 0, 0, 0, 0, 0 }, // 100
    { 020523, 0, 0, 0, 0, 0 }, // 101
    { 022027, 0, 0, 0, 0, 0 }, // 102
    { 020450, 0, 0, 0, 0, 0 }, // 103
    { 020162, 0, 0, 0, 0, 0 }, // 104
    { 017771, 0, 0, 0, 7, 0 }, // 105
};
// clang-format on

// The same table read by the type field of a descriptor (bits 44-41): its
// type (3 bits), a formal's type (4 bits); by kind_index(); by the packed
// kind bits of a formal (its class in CORRESPONDENCE).
// clang-format off
const unsigned TYPE_OF[020] = {
    0, 1, 2, 5, 3, 0, 0, 0,   4, 0, 0, 0, 0, 0, 0, 0,
};
const unsigned FORMAL_TYPE_OF[020] = {
    00, 01, 02, 05, 03, 00, 00, 00,   04, 00, 00, 00, 010, 00, 00, 00,
};
const unsigned KIND_OF[040] = {
    00, 00, 01, 00, 02, 03, 00, 00,   00, 01, 00, 00, 04, 03, 00, 00,
    00, 05, 01, 00, 06, 07, 00, 00,   00, 05, 01, 00, 06, 07, 00, 00,
};
const unsigned FORMAL_CLASS[0100] = {
    00,  02,  00,  00,  00,  00,  00,  00,   00,  00,  00,  00,  00,  00,  00,  00,
    00,  00,  00,  00,  00,  00,  00,  00,   00,  00,  00,  00,  00,  00,  00,  00,
    00,  04,  012, 00,  014, 016, 00,  00,   00,  06,  00,  00,  00,  00,  00,  00,
    00,  010, 00,  00,  00,  00,  00,  00,   00,  00,  00,  00,  00,  00,  00,  00,
};
// clang-format on
constexpr Word FORMAL_BITS = 01017400000000000; // packed for FORMAL_CLASS

// How a type combines with another (TYPE_PAIRS, 17667…): 0 the same, 1 or 2
// an integer and a real, 3 an error. Rows 6, 7 are other words there.
const unsigned TYPE_COMBINE[8][8] = {
    { 0, 3, 3, 3, 3, 3, 0, 0 }, // 0
    { 3, 0, 2, 3, 3, 3, 0, 0 }, // 1
    { 3, 1, 0, 3, 3, 3, 0, 0 }, // 2
    { 3, 3, 3, 0, 3, 3, 0, 0 }, // 3
    { 3, 3, 3, 3, 0, 3, 0, 0 }, // 4
    { 3, 3, 3, 3, 3, 0, 0, 0 }, // 5
    { 0, 0, 0, 0, 0, 0, 0, 0 }, // 6
    { 2, 0, 0, 0, 0, 0, 0, 2 }, // 7
};

// Actual of kind k (row) for a formal of class c (column c/2): which marks
// they exchange and what must agree (CORRESPONDENCE, 21075…).
const unsigned CORRESPONDENCE[013][8] = {
    { 00, 00, 00, 00, 00, 010, 010, 010 },   // 000
    { 00, 00, 010, 010, 010, 01, 010, 010 }, // 001
    { 00, 00, 010, 010, 010, 010, 01, 010 }, // 002
    { 00, 00, 05, 010, 03, 010, 02, 00 },    // 003
    { 00, 00, 07, 010, 06, 010, 01, 06 },    // 004
    { 00, 00, 03, 04, 03, 010, 010, 010 },   // 005
    { 00, 00, 07, 010, 06, 010, 01, 06 },    // 006
    { 00, 00, 07, 010, 03, 010, 02, 00 },    // 007
    { 00, 00, 05, 010, 00, 010, 010, 010 },  // 010
    { 00, 00, 03, 03, 03, 010, 010, 010 },   // 011
    { 00, 00, 07, 010, 03, 010, 02, 00 },    // 012
};

// The relations of an integer and a real, by relation code 5-012 (20405…).
const Word REL_MIXED[] = { 05, 06, 07, 010, 0105, 0106 };

// The numeric types: real and integer (20317…; 6, 7 are other words there).
const bool NUMERIC[8] = { false, true, true, false, false, false, true, true };

// The last item of a STEP element by the type of the step (21614…): 0140
// real, 034 integer; the others are other words there.
// clang-format off
const Word STEP_KINDS[8] = {
    05642160003017364, 0140,             034,              010202434122646,
    0262162334102444,  0230203000150000, 0230203002200000, 01000074100000,
};
// clang-format on

// The arithmetic operators + - * / (20142…): the item for integers at bits
// 35-30, for reals at 29-24; their texts are OP_TEXT.
// The low bits hold the operator's index.
const Word OPS[] = {
    067'13'00000000,
    076'14'00000000 | 1,
    077'15'00000000 | 2,
    000'16'00000000 | 3,
};

// Items and constants of the module.
constexpr Word STRING_PLUS     = 070157; // + on strings
constexpr Word SLICE_CODE      = 050;
constexpr Word STRING_DESC     = 0363400000000000; // a string's descriptor under STRING_MASK
constexpr Word STRING_MASK     = 0200400000000000;
constexpr unsigned STRING_TYPE = 4;
constexpr Word STD_PROC_BITS   = 0340000000000; // bits 39-37
constexpr Word IO_ITEM         = 0270072;       // I/O items
constexpr Word IO_ITEM_NAME    = 0300071;
constexpr Word IO_ITEM_ARRAY   = 0310071;
constexpr Word IO_073          = 0320072;
constexpr Word IO_101          = 016300024;
constexpr Word IO_TWO_0101     = 060673;
constexpr Word IO_TWO_0102     = 0630673;
constexpr Word INTEGER         = 06400000000000000; // an integer's exponent
constexpr Word MINUS_4         = 07777777777777773; // cyclic -4
constexpr Word MINUS_0127      = 07777777777777650; // cyclic -0127
constexpr Word LEVEL_DOWN      = 07777777777767777; // cyclic -(1 << 12)

// Message formats (G27733): ISO text, 0200+k inserts data word k.
using Format = std::vector<Word>;

Format iso_format(std::string_view text)
{
    return iso_words(text);
}

const Format UNDECLARED    = iso_format("IDENTIFIER ′\200′ IS NOT DECLARED\202");
const Format ILLEGAL_USE   = iso_format("ILLEGAL USE OF IDENTIFIER ′\200′\202");
const Format TWICE         = iso_format("TWICE DECLARED IDENTIFIER ′\200′\202");
const Format TYPE_ERROR    = iso_format("TYPE ERROR IN CONNECTION WITH \200\202");
const Format NO_ASSIGNMENT = iso_format("NO ASSIGNMENT TO ′\200′\202");
const Format BAD_LEFT      = iso_format("ILLEGAL LEFT PART OR INPUT ELEMENT\202");
const Format IO_ERROR      = iso_format("ILLEGAL ARGUMENT OF I/O STATEMENT\202");
const Format BAD_INPUT     = iso_format("ILLEGAL INPUT OF ′\200′\202");
const Format NO_CORRESPONDENCE =
    iso_format("NO ACTUAL-FORMAL CORRESPONDENCE BETWEEN ′\210′ AND ′\200′\202");
const Format LABEL_ARG  = iso_format("LABEL AS ARGUMENT OF NON-ALGOL CALL \202");
const Format BAD_VALUE  = iso_format("ILLEGAL VALUE PARAMETER ′\200′\202");
const Format BAD_LENGTH = iso_format("ILLEGAL ARGUMENT OF LENGTH\202");
const Format BAD_PRINT  = iso_format("ILLEGAL ARGUMENT OF PRINT\202");
const Format INCORRECT  = iso_format(" * * INCORRECT ALGOL PROGRAM * * ");

// The position of the next statement (copied to POSITION, 6 words): the
// format, then the texts of the line and the statement number.
const Format POSITION_TEXT = iso_format(". NEXT STATEMENT IS \206/\207\0PROGR\0END\0  "sv);

// Messages of the 'COMMON' check: 8 words padded with blanks, then the
// element's name.
Format blank_padded(std::string_view text)
{
    Format w = iso_format(text);
    w.resize(010);
    for (Word &x : w)
        for (int shift = 0; shift < 48; shift += 8)
            if (!((x >> shift) & 0377))
                x |= Word(' ') << shift;
    return w;
}

const Format NOT_DECLARED = blank_padded("**HECOOTBETCTBИE B OПИCAHИЯX COMMON ЭЛ-TA");
const Format WRONG_ORDER  = blank_padded("*****ПEPEПYTAH ПOPЯДOK B OПИCAHИЯX COMMON ЭЛ-TA");

// Arguments of TYPE ERROR IN CONNECTION WITH (one word; two for some).
const Word GOTO_TEXT   = iso_word("GOTO\0 "sv);
const Word INDEX_TEXT  = iso_word("INDEX\0"sv);
const Word POWER_TEXT  = iso_word("′**′\0 "sv);
const Word IF_TEXT     = iso_word("′IF′\0 "sv);
const Word FOR_TEXT    = iso_word("′FOR′\0"sv);
const Word SWITCH_TEXT = iso_word("SWITCH");
const Word ASSIGN_TEXT = iso_word("′:=′\0 "sv);
const Format BOOL_TEXT = iso_format("BOOLEAN OP\0 "sv);
const Format REL_TEXT  = iso_format("RELATION\0   "sv);
const Word OP_TEXT[]   = {
    iso_word("′+′\0  "sv),
    iso_word("′-′\0  "sv),
    iso_word("′*′\0  "sv),
    iso_word("′/′\0  "sv),
};

// The actual of NO ACTUAL-FORMAL CORRESPONDENCE that is not a name, by kind
// 010-012 (two words).
const Format ACTUAL_TEXT[] = {
    iso_format("CONSTANT\0   "sv),
    iso_format("SUBSCR.VAR.\0"sv),
    iso_format("EXPRESSION\0 "sv),
};

// Addresses of the original: handlers by item code and continuations,
// kept on the stack as return addresses.
constexpr unsigned DRIVER           = 01627;
constexpr unsigned NEXT_ITEM        = 017442;
constexpr unsigned AFTER_END        = 017443;
constexpr unsigned DISPATCH         = 017450;
constexpr unsigned RETURN_SUB       = 017451;
constexpr unsigned SET_POSITION     = 017535;
constexpr unsigned DECL_END         = 017553;
constexpr unsigned FOR_END          = 017601;
constexpr unsigned NEXT_RETURN      = 017625;
constexpr unsigned LEFT_PART        = 017702;
constexpr unsigned AFTER_LEFT       = 017720;
constexpr unsigned NEXT_LEFT        = 017736;
constexpr unsigned LEFT_COUNT       = 017737;
constexpr unsigned NEXT_SUB         = 020020;
constexpr unsigned INDEX_DONE       = 020041;
constexpr unsigned SUBSCRIPT_DONE   = 020044;
constexpr unsigned ARITH_LEFT       = 020110;
constexpr unsigned BOUND_LOOP       = 020176;
constexpr unsigned BOUND_DONE       = 020177;
constexpr unsigned ACTUAL_DONE      = 020643;
constexpr unsigned THUNK_BODY       = 021062;
constexpr unsigned DESIGNATOR_THUNK = 021071;
constexpr unsigned THUNK_DESIGNATOR = 021074;
constexpr unsigned CODE_CALL        = 021110;
constexpr unsigned CODE_CALL_ARGS   = 021112;
constexpr unsigned ARG_DESIGNATOR   = 021134;
constexpr unsigned ARG_EXPRESSION   = 021140;
constexpr unsigned ARG_LABEL        = 021145;
constexpr unsigned ARG_RETURN       = 021150;
constexpr unsigned ARG_COUNT        = 021151;
constexpr unsigned STD_END          = 021257;
constexpr unsigned PRINT_COUNT      = 021324;
constexpr unsigned READ_COUNT       = 021342;

// The machine stack, as the driver leaves it.
constexpr unsigned STACK      = 053401;
constexpr unsigned STACK_SIZE = 010000;

constexpr unsigned RAW = Arith::NORM_DISABLE | Arith::ROUND_DISABLE; // NTR 3

constexpr Word HEADER         = 0'4000'0000'0000'0000; // D-list: a block header
constexpr Word DESCRIPTOR     = 0'2000'0000'0000'0000;
constexpr Word NEGATIVE       = 0'0020'0000'0000'0000; // bit 41
constexpr Word BIT48          = 0'4000'0000'0000'0000;
constexpr Word BIT39          = 0'0000'4000'0000'0000;
constexpr Word PARAMS         = 0'0000'0000'0370'0000; // number of parameters
constexpr Word ASSIGNED       = 0'0004'0000'0000'0000; // a formal by name assigned to
constexpr Word NOT_ASSIGNABLE = 0'0010'0000'0000'0000;
constexpr Word RESULT_SET     = 0200000; // in a function's word after it: assigned
constexpr Word MANTISSA       = 0'0017'7777'7777'7777;
constexpr Word HAS_ARRAYS     = 0100000;               // a block header: arrays declared
constexpr Word FUNCTION       = 0'0360'0000'0000'0000; // a procedure's type
constexpr Word VALUE_FORMAL   = 0'1000'0000'0000'0000;
constexpr Word IN_COMMON_CLR  = 0'7770'0777'7777'7777;
constexpr Word IN_COMMON      = 0'0001'0000'0000'0000;
constexpr Word DIMENSIONS     = 0'0000'0000'0070'0000;
constexpr Word COMMON_SIMPLE  = 0'0000'0000'0010'0000;
constexpr Word RECORD_CLR     = 0'7777'7400'0377'7777; // descriptor without bits 33-21: the
                                                       // shadowed binding, then the array record
constexpr Word MINUS_ONE = 0'7777'7777'7777'7776;      // cyclic −1
constexpr Word NOT_BLANK = 0'2765'7537'2765'7537;      // a word of blanks and zeros has none

Word cyclic_add(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & WORD_MASK;
    return s;
}

unsigned slot_of(Word w)
{
    return (w >> 24) & 07777;
}

unsigned addr(Word w)
{
    return w & 077777;
}

// The kind index of a descriptor: bits 46, 45, 38, 37, 36.
unsigned kind_index(Word d)
{
    return ((d >> 41) & 030) | ((d >> 35) & 07);
}

// A shift of the machine: right by n, left when n < 0.
// a - x, x - a on the arithmetic unit without normalization or rounding.
Word raw_sub(Word a, Word x)
{
    Arith u(a, RAW);
    u.sub(x);
    return u.acc;
}

Word raw_rsub(Word a, Word x)
{
    Arith u(a, RAW);
    u.rsub(x);
    return u.acc;
}

Word raw_add(Word a, Word x)
{
    Arith u(a, RAW);
    u.add(x);
    return u.acc;
}

bool less(Word a, Word x)
{
    return raw_sub(a, x) & NEGATIVE;
}

//
// G30001: six ISO characters of n, decimal, leading zeros blank: "%6u" of
// (n & 06777777) % 1000000 below a million (above it the first digit is the
// quotient & 017). fill: the last fill character.
//
Word itoa6(Word n, unsigned &fill)
{
    unsigned rest = unsigned(n & 06777777);
    Word text     = 0;
    fill          = 040;
    for (unsigned d = 100000; d; d /= 10) {
        unsigned q = rest / d;
        rest -= q * d;
        unsigned digit = q & 017;
        unsigned c     = fill;
        if (digit) {
            fill = 060;
            c    = 060 ^ digit;
        }
        text = ((text << 8) ^ c) & WORD_MASK;
    }
    return text | 060;
}

//
// Pass 2 runs as the original: handlers chained by the addresses they
// return, the return addresses of subtrees on its stack. The stream is read
// backward, so an operator comes before its operands, a block's END before
// its BEGIN. Each subtree leaves the type of its value in type_. Members
// named after their role keep what the registers of the original kept
// between routines: a routine may read what another left in them.
//
class Pass2 {
public:
    Pass2(Context &ctx, const Stream &input);

    Stream run();

private:
    using Handler = std::function<unsigned()>;

    Context &ctx_;
    const Stream &in_;
    size_t pos_;
    Stream out_; // in the order written
    std::map<unsigned, Handler> handlers_;

    std::array<Word, STACK_SIZE> stack_{};
    unsigned sp_{ STACK };
    Word zero_{};
    Word flgex_{};
    std::array<Word, NAMES - CELLS> cells_{};                  // 01736-02143: the work cells
    std::array<Word, Tables::DLIST_BEGIN - ARRAYS> records_{}; // 04000-04377: the array records

    // The item read: code (M1), count (M2), operand (M3).
    unsigned code_{}, count_{}, operand_{};
    // An identifier looked up: descriptor (M4), type (M5), kind (M6).
    unsigned desc_{}, type_{}, kind_{};
    unsigned formal_{};    // M9: the formal of the argument
    unsigned emit_code_{}; // M14: the code of the item written next

    Word &mem(unsigned a);
    void store(unsigned a, Word w)
    {
        a &= 077777;
        if (a)
            mem(a) = w;
    }
    void push(Word w)
    {
        stack_.at(sp_ - STACK) = w;
        sp_++;
    }
    Word pop()
    {
        sp_--;
        return stack_.at(sp_ - STACK);
    }
    Word &top(unsigned k = 1) { return stack_.at(sp_ - k - STACK); }
    Word &name(unsigned slot) { return mem(NAMES + slot); }
    Word saved_name(unsigned slot);

    // Streams.
    void read();
    void emit(Word w) { out_.push_back(Item{ uint32_t(w & Item::MASK) }); }
    void emit_shifted(Word a) { emit(((a << 12) & WORD_MASK) ^ emit_code_); }
    void emit_code() { emit(emit_code_); }
    void emit_item() { emit(mem(ITEM)); }

    // Subtrees.
    unsigned parse_read(unsigned ret);
    unsigned parse(unsigned ret);
    void loop(unsigned pc, unsigned stop);
    void subtree(unsigned ret) { loop(parse_read(ret), ret); }
    void parse_sub(unsigned ret) { loop(parse(ret), ret); }
    unsigned dispatch();
    unsigned return_sub();

    // Messages.
    void print_words(const std::vector<Word> &w);
    void format_line(const Format &format, unsigned data);
    void common_message(const Format &text, Word element);
    Word after_message();
    Word message(const Format &format);
    Word message_name(const Format &format);
    Word illegal_use();
    Word type_error_args();
    Word type_error(Word text);

    // Scopes and identifiers.
    bool inside(unsigned block);
    void descriptor_types();
    void lookup();
    void open_scope(unsigned header, Word end);
    void open_scope_at() { open_scope(operand_, mem(operand_)); }
    void close_scope();
    unsigned type_pair(Word other);

    // Statements and expressions.
    unsigned start();
    unsigned finish();
    unsigned end_item();
    unsigned begin_item();
    unsigned emit_operand(Word a);
    bool proc_operand();
    unsigned identifier();
    unsigned constant(unsigned type);
    unsigned string_item();
    unsigned arith_left();
    unsigned arith_op(unsigned op);
    unsigned divide_item();
    void index_type();
    void array_dims();
    unsigned subscripted(unsigned kind);
    unsigned subscript_item(unsigned kind);
    unsigned left_designator();
    unsigned index_offset(Word c);
    unsigned index_general(unsigned op);
    bool small_constant(Word limit) { return less(name(operand_), limit); }
    unsigned next_subscript();
    unsigned subscript_done();
    unsigned index_done();
    unsigned power_item();
    unsigned bool_error();
    unsigned bool_result();
    unsigned bool_item();
    unsigned not_item();
    unsigned relation_item();
    unsigned uplus_item();
    unsigned uminus_item();
    unsigned next_left();
    unsigned left_count();
    unsigned emit_left();
    unsigned after_left();
    unsigned left_part();
    unsigned assign_item();
    Word relation_error();
    bool relation_condition(Word &code);
    unsigned condition(Word conversion);
    unsigned cond_expr_item();
    unsigned then_item();
    unsigned for_element();
    unsigned count_element();
    unsigned for_end_item();
    unsigned do_item();
    unsigned plain_item();
    unsigned while_item();
    unsigned step_item();
    unsigned control_item();
    void close_block();
    unsigned switch_item();
    unsigned bounds_item();
    unsigned array_item();
    unsigned syntax_error_item();
    bool outermost(Word a) { return raw_sub(a, mem(DLIST_TOP2)) == 2; }
    unsigned decl_end();
    unsigned procedure_item();
    unsigned body_end_item();
    unsigned slice();
    unsigned string_element(unsigned kind);
    unsigned set_position();
    unsigned goto_item();
    unsigned value_item();

    // I/O statements.
    unsigned io_last();
    unsigned io_string();
    unsigned io_write(unsigned code);
    unsigned io_control(unsigned code);
    unsigned io_065();
    unsigned io_074();
    unsigned io_101();
    unsigned io_two(Word item);

    // Calls.
    unsigned call_end();
    unsigned formal_type();
    unsigned actual_count();
    unsigned actual_done();
    unsigned mismatch();
    unsigned check_types();
    unsigned correspond();
    unsigned actual_emit(Word a);
    void thunk();
    unsigned thunk_end();
    Word proc_params();
    unsigned actual_ident();
    unsigned actual_kind();
    unsigned next_actual();
    unsigned call_head();
    unsigned call_item();

    // Non-ALGOL calls.
    unsigned arg_return() { return addr(pop()); }
    unsigned arg_label();
    unsigned arg_item();
    unsigned io_arg(unsigned ret);
    unsigned arg_count();
    unsigned arg_ident();
    unsigned std_end();
    unsigned print_count();
    unsigned print_item();
    unsigned print_arg();
    unsigned read_count();
    unsigned as_left_part(unsigned ret);
    unsigned read_arg();
    unsigned std_proc(Word number);
    unsigned code_call();
    unsigned code_call_args();

    void register_handlers();
};

Word &Pass2::mem(unsigned a)
{
    a &= 077777;
    if (a == FREE_CONST)
        return ctx_.tables.free_const;
    if (a >= NAMES && a < NAMES + Tables::SLOTS)
        return ctx_.tables.name(a - NAMES);
    if (a >= Tables::DLIST_BEGIN && a < Tables::DLIST_END)
        return ctx_.tables.dlist(a);
    if (a >= COMMON && a < COMMON + Tables::COMMON_SIZE)
        return ctx_.tables.common(a - COMMON);
    if (a >= CELLS && a < NAMES)
        return cells_[a - CELLS];
    if (a >= ARRAYS && a < Tables::DLIST_BEGIN)
        return records_[a - ARRAYS];
    if (a >= STACK && a < STACK + STACK_SIZE)
        return stack_[a - STACK];
    if (a == FLGEX)
        return flgex_;
    if (!a) {
        zero_ = 0;
        return zero_;
    }
    throw std::runtime_error("pass 2: no data at " + std::to_string(a));
}

//
// The names of the slots, saved at the start (the slots are cleared then).
// The original swaps the page out to read a name for a message.
//
Word Pass2::saved_name(unsigned slot)
{
    slot &= 077777;
    if (slot < ctx_.saved_names.size())
        return ctx_.saved_names[slot];
    return mem(NAMES + slot);
}

//
// *0127B: the previous item into ITEM, code_, count_, operand_.
//
void Pass2::read()
{
    if (!pos_)
        throw std::runtime_error("pass 2: read before the start of the stream");
    Word w    = in_[--pos_].bits;
    mem(ITEM) = w;
    code_     = w & 0177;
    count_    = (w >> 7) & 037;
    operand_  = (w >> 12) & 07777;
}

// *0223B: a subtree from the item read: its return address and REPORTED
// pushed, REPORTED cleared.
unsigned Pass2::parse_read(unsigned ret)
{
    push(ret);
    push(mem(REPORTED));
    mem(REPORTED) = 0;
    return DISPATCH;
}

// *0222B: read, then the subtree.
unsigned Pass2::parse(unsigned ret)
{
    read();
    return parse_read(ret);
}

// Run the handlers from pc until one returns stop.
void Pass2::loop(unsigned pc, unsigned stop)
{
    while (pc != stop) {
        auto it = handlers_.find(pc);
        if (it == handlers_.end())
            throw std::runtime_error("pass 2: no handler at " + std::to_string(pc));
        pc = it->second();
    }
}

// *0225B: the handler of the item's code.
unsigned Pass2::dispatch()
{
    unsigned h = code_ < std::size(CODES) ? CODES[code_].handler : 0;
    if (!h)
        throw std::runtime_error("pass 2: no handler for item code " + std::to_string(code_));
    return h;
}

// *0226B: return from a subtree, its REPORTED ORed into the caller's.
unsigned Pass2::return_sub()
{
    mem(REPORTED) |= pop();
    return addr(pop());
}

//
// Messages.
//

// AG01644 and ASMPRINT: print the words of a text on one row. ASMPRINT
// leaves M14 = 0.
void Pass2::print_words(const std::vector<Word> &w)
{
    if (ctx_.listing)
        ctx_.listing->print(listing_text(w.data(), 0, 6 * w.size()));
    emit_code_ = 0;
}

//
// G27733: print a line of the format with the data at data: bytes 0200+k
// insert the text of data word k (a format itself), 0 ends it.
//
void Pass2::format_line(const Format &format, unsigned data)
{
    std::vector<unsigned> chars;
    std::function<void(const std::function<Word(unsigned)> &)> expand = [&](const auto &word) {
        for (unsigned i = 0;; i++)
            for (int shift = 40; shift >= 0; shift -= 8) {
                unsigned c = (word(i) >> shift) & 0377;
                if (!c)
                    return;
                if (c & 0200)
                    expand([this, a = data + c - 0200](unsigned j) { return mem(a + j); });
                else
                    chars.push_back(c);
            }
    };
    expand([&](unsigned i) { return i < format.size() ? format[i] : 0; });
    std::vector<Word> w(chars.size() / 6 + 1);
    for (size_t i = 0; i < chars.size(); i++)
        w[i / 6] |= Word(chars[i]) << (40 - 8 * (i % 6));
    print_words(w);
}

// A message of the 'COMMON' check: the text with the element's name.
void Pass2::common_message(const Format &text, Word element)
{
    flgex_   = cyclic_add(flgex_, 1);
    Format w = text;
    w.push_back(element);
    print_words(w);
}

//
// *0205B, after a message: the first one in a subtree sets FLGEX* and
// REPORTED. Returns what A holds then: REPORTED, or 1.
//
Word Pass2::after_message()
{
    desc_ = 0;
    type_ = 0;
    if (Word r = mem(REPORTED))
        return r;
    flgex_        = 1;
    mem(REPORTED) = 1;
    return 1;
}

// *0203B: the message of the format, with the data at ARGS.
Word Pass2::message(const Format &format)
{
    format_line(format, ARGS);
    return after_message();
}

// *0175B: the message of the format about the name of slot operand_.
Word Pass2::message_name(const Format &format)
{
    mem(ARGS)     = saved_name(operand_);
    mem(ARGS + 1) = 0;
    return message(format);
}

// *0211B: ILLEGAL USE OF IDENTIFIER of slot operand_, unless reported in this subtree.
Word Pass2::illegal_use()
{
    if (Word r = mem(REPORTED))
        return r;
    return message_name(ILLEGAL_USE);
}

// *0430B: TYPE ERROR IN CONNECTION WITH the text at ARGS, unless reported in this subtree.
Word Pass2::type_error_args()
{
    if (Word r = mem(REPORTED))
        return r;
    return message(TYPE_ERROR);
}

// *0427B: the same with the text given; FLGEX* gets it too.
Word Pass2::type_error(Word text)
{
    mem(ARGS) = text;
    flgex_    = text;
    return type_error_args();
}

//
// Scopes and identifiers.
//

// *0146B: is the binding desc_ inside the block whose header is at block,
// between the header and its end?
bool Pass2::inside(unsigned block)
{
    block        = addr(block);
    emit_code_   = block;
    unsigned end = addr(mem(block));
    return desc_ > block && desc_ < end;
}

// *0162B: type_ and kind_ of the descriptor at desc_.
void Pass2::descriptor_types()
{
    Word d     = mem(desc_);
    type_      = TYPE_OF[(d >> 40) & 017];
    emit_code_ = kind_index(d);
    kind_      = KIND_OF[emit_code_];
}

//
// *0154B: the identifier of slot operand_: desc_, type_, kind_. An
// undeclared one is reported once (its slot gets 1); one declared inside
// the HIDDEN block is an illegal use.
//
void Pass2::lookup()
{
    Word w = name(operand_);
    desc_  = addr(w);
    if (!desc_) {
        name(operand_) = 1;
        message_name(UNDECLARED);
        return;
    }
    if (w == 1) {
        after_message();
        return;
    }
    if (mem(HIDDEN) && inside(mem(HIDDEN))) {
        illegal_use();
        return;
    }
    descriptor_types();
}

//
// *0232B: open the scope of the block whose header is at header, up to end.
// Each declaration is bound to its name slot and keeps the binding it
// shadows in bits 33-21; nested blocks are skipped. TWICE DECLARED when the
// shadowed binding is inside the block. (The original then checks the next
// declarations against the slot instead of the block, which reads a word of
// low memory: it reports later ones that shadow an outer binding.)
//
void Pass2::open_scope(unsigned header, Word end)
{
    mem(BLOCK_END) = end & 07777;
    unsigned p     = header;
    for (;;) {
        p++;
        for (;;) {
            if (p == mem(BLOCK_END))
                return;
            Word d = mem(p);
            if (d & DESCRIPTOR)
                break;
            p = addr(d);
        }
        Word d    = mem(p);
        kind_     = addr(d);
        Word prev = name(kind_);
        desc_     = addr(prev);
        store(p, (d & RECORD_CLR) | ((prev << 20) & WORD_MASK));
        name(kind_) = p;
        if (inside(header)) {
            operand_ = kind_;
            message_name(TWICE);
            operand_ = header;
        }
    }
}

// *0252B: close the scope of the block at BLOCK: each declaration's slot gets
// back the binding it shadowed.
void Pass2::close_scope()
{
    unsigned p     = addr(mem(BLOCK));
    mem(BLOCK_END) = mem(p) & 07777;
    for (;;) {
        p++;
        for (;;) {
            if (p == mem(BLOCK_END))
                return;
            Word d = mem(p);
            if (d & DESCRIPTOR)
                break;
            p = addr(d);
        }
        Word d      = mem(p);
        kind_       = addr(d);
        name(kind_) = (d >> 20) & 07777;
    }
}

// *0440B: how types type_ and other combine (TYPE_COMBINE).
unsigned Pass2::type_pair(Word other)
{
    emit_code_ = TYPE_COMBINE[type_ & 7][addr(other) & 7];
    return emit_code_;
}

//
// G17223: start of pass 2. Read the stream from its end, check the 'COMMON'
// elements against their declarations and mark them in the D-list, save the
// names to drum 26 zone 0, clear the identifier slots, open the outer scope
// and emit EOP. operand_ is the outer block, as pass 1 leaves M3.
//
unsigned Pass2::start()
{
    type_ = 0;
    push(DRIVER); // the first read of the original is past the end: nothing
    mem(DLIST_TOP2)  = operand_;
    mem(DLIST_TOP)   = operand_;
    mem(BLOCK_DLIST) = Word(operand_) << 12;

    // Each element: its slot holds its name, and the declaration at D3 is its own.
    for (unsigned k = 0; mem(COMMON + k + 1); k += 3) {
        Word name_w = mem(COMMON + k);
        Word info   = mem(COMMON + k + 1);
        if (!slot_of(info))
            continue; // the block
        if (name(slot_of(info)) != name_w) {
            common_message(NOT_DECLARED, name_w);
            continue;
        }
        if ((mem(unsigned(info >> 36)) & 07777) != slot_of(info))
            common_message(WRONG_ORDER, name_w);
    }

    // Mark the declarations of the elements.
    for (unsigned p = DLIST_END - 1; mem(p); p--) {
        Word &d = mem(p);
        if (d & HEADER)
            continue;
        for (unsigned k = 0; mem(COMMON + 1 + k); k += 3) {
            if (slot_of(mem(COMMON + 1 + k)) != (d & 077777))
                continue;
            d = (d & IN_COMMON_CLR) | IN_COMMON;
            if (!(d & DIMENSIONS))
                d |= COMMON_SIMPLE;
            break;
        }
    }

    mem(FREE_SAVED) = mem(FREE_CONST);
    mem(ARRAYS_END) = ARRAYS;
    ctx_.saved_names.assign(ctx_.tables.names().begin(), ctx_.tables.names().end());
    for (unsigned k = addr(mem(FREE_CONST)); k;)
        name(--k) = 0;
    store(operand_, DLIST_END);
    open_scope(operand_, DLIST_END);
    for (unsigned i = 0; i < 6; i++)
        mem(POSITION + i) = POSITION_TEXT.at(i);
    emit_code_ = 075;
    emit_code();
    read();
    return NEXT_ITEM;
}

//
// *2604B, at the first item: link the descriptors of each array record to
// it, print INCORRECT ALGOL PROGRAM after errors, return into the driver.
//
unsigned Pass2::finish()
{
    sp_ -= 2;
    for (unsigned p = ARRAYS; p != mem(ARRAYS_END); p++) {
        Word rec  = mem(p);
        count_    = (rec >> 7) & 037;
        desc_     = (rec >> 12) & 07777;
        Word link = Word(p) << 20;
        do {
            store(desc_, (mem(desc_) & RECORD_CLR) | link | HEADER);
            count_ = (count_ - 1) & 077777;
            desc_  = (desc_ + 1) & 077777;
        } while (count_);
    }
    if (flgex_)
        format_line(INCORRECT, 0);
    return addr(pop());
}

//
// *0266B, END (the stream is read backwards, so it opens a scope): the block
// at operand_ becomes BLOCK, the old one pushed. A block header (bit 48) is
// written out with the old DLIST_TOP, which is pushed and replaced by the
// block; else 0 is pushed.
//
unsigned Pass2::end_item()
{
    push(mem(BLOCK));
    mem(BLOCK) = operand_;
    if (mem(operand_) & BIT48) {
        push(mem(DLIST_TOP));
        mem(DLIST_TOP) = operand_;
        emit_code_     = code_;
        emit_shifted(top(1));
    } else {
        push(0);
    }
    open_scope_at();
    read();
    return AFTER_END;
}

//
// *0300B, BEGIN: closes the scope. For a block header, the item is written
// with BLOCK and DLIST_TOP restored first. BLOCK gets the outer block back.
//
unsigned Pass2::begin_item()
{
    sp_ -= 2;
    if (Word top_saved = pop()) {
        mem(DLIST_TOP) = top_saved;
        emit_code_     = code_;
        emit_shifted(mem(BLOCK));
    }
    close_scope();
    mem(BLOCK) = pop();
    read();
    return DISPATCH;
}

//
// *0377B: write the item a<<12 ^ emit_code_. An operand alone as a statement
// (the subtree called from the statement loop) is an illegal use.
//
unsigned Pass2::emit_operand(Word a)
{
    emit_shifted(a);
    if (top(2) == AFTER_END) {
        illegal_use();
        return NEXT_RETURN;
    }
    read();
    return RETURN_SUB;
}

//
// *0416B: a procedure without parameters used as an operand: with parameters
// it is an illegal use (true); else bit 39 of its descriptor is flipped
// unless bit 48 is set.
//
bool Pass2::proc_operand()
{
    if (mem(desc_) & PARAMS) {
        illegal_use();
        return true;
    }
    if (!(mem(desc_) & BIT48))
        mem(desc_) ^= BIT39;
    return false;
}

//
// Code 000, an identifier: looked up, then by its kind the item for its
// value (017), a call (021, 022, 0107), a label (024: bit 48 of the
// descriptor marks it used outside the procedure at DLIST_TOP); arrays and
// procedures with parameters are illegal here.
//
unsigned Pass2::identifier()
{
    lookup();
    if (!desc_) {
        read();
        return RETURN_SUB;
    }
    switch (kind_) {
    case 0:
    case 5:
        if (type_) {
            emit_code_ = kind_ ? 022 : 017;
            return emit_operand(desc_);
        }
        if (!inside(mem(DLIST_TOP)))
            mem(desc_) |= BIT48;
        emit_code_ = 024;
        return emit_operand(desc_);
    case 1:
    case 2:
        illegal_use();
        return NEXT_RETURN;
    case 3:
    case 4:
    case 6:
    case 7:
        if ((kind_ == 4 || kind_ == 6) && proc_operand())
            return NEXT_RETURN;
        if (kind_ <= 4)
            emit_code_ = (mem(desc_) & BIT48) ? 0107 : 021;
        else
            emit_code_ = 022;
        emit_shifted(desc_);
        return NEXT_RETURN;
    }
    throw std::runtime_error("pass 2: kind " + std::to_string(kind_));
}

// *0550B: a constant of the type in slot operand_: item 023.
unsigned Pass2::constant(unsigned type)
{
    type_      = type;
    emit_code_ = 023;
    return emit_operand(operand_);
}

// Code 025: a string, item 070.
unsigned Pass2::string_item()
{
    type_      = 4;
    emit_code_ = 070;
    return emit_operand(operand_);
}

//
// *0665B: the left operand of + - * / (the right one's type pushed, over the
// operator's word). Both of type 0 or not alike is a TYPE ERROR; with an
// integer and a real (/ on two integers too) the left one gets 064 and the
// result is real; strings only add. The item: the operator's code for the
// type.
//
unsigned Pass2::arith_left()
{
    push(type_);
    subtree(020112);
    unsigned left = type_;
    unsigned r    = type_pair(pop());
    auto op_code  = [&](int n, unsigned type) {
        Word op = pop();
        type_   = type;
        emit((op >> n) & 077);
        return RETURN_SUB;
    };
    auto error = [&] {
        type_error(OP_TEXT[addr(pop()) & 3]);
        return RETURN_SUB;
    };
    switch (r) {
    case 0:
        if (!left)
            return error();
        if (left == 1)
            break;
        type_ = (left - 2) & 077777;
        if (left == 2)
            return op_code(30, 2);
        type_ = (left - 4) & 077777;
        if (left != 4 || top(1) != OPS[0])
            return error();
        type_ = 4;
        pop();
        emit(STRING_PLUS);
        return RETURN_SUB;
    case 1:
        if (top(1) == OPS[3] && type_ == 2) {
            emit_code_ = 064;
            emit_code();
        }
        break;
    case 2:
        break;
    case 3:
        return error();
    default:
        throw std::runtime_error("pass 2: type pair " + std::to_string(r));
    }
    return op_code(24, 1);
}

// Codes 013-015: + - *.
unsigned Pass2::arith_op(unsigned op)
{
    push(OPS[op]);
    return parse(ARITH_LEFT);
}

// Code 016, /: an integer right operand gets 064.
unsigned Pass2::divide_item()
{
    push(OPS[3]);
    parse_sub(020156);
    if (type_ == 2) {
        emit_code_ = 064;
        type_      = 1;
        emit_code();
    }
    return arith_left();
}

// *0644B: a subscript of type_: a real one gets 063 (to integer), an
// integer one is fine, else TYPE ERROR INDEX.
void Pass2::index_type()
{
    switch (type_) {
    case 1:
        emit(063);
        break;
    case 2:
        break;
    case 0:
    case 3:
    case 4:
    case 5:
        type_error(INDEX_TEXT);
        break;
    default:
        throw std::runtime_error("pass 2: type " + std::to_string(type_));
    }
}

//
// *0654B: the dimensions in bits 19-15 of the array desc_; another number of
// subscripts (on the stack) is an illegal use; a formal array without them
// gets them.
//
void Pass2::array_dims()
{
    if (Word dims = (mem(desc_) >> 15) & 037) {
        if (dims != top(1))
            illegal_use();
        return;
    }
    mem(desc_) ^= (top(1) << 15) & WORD_MASK;
}

//
// *0560B: an array element or a switch designator: slot operand_ with
// count_ subscripts (kind: 044, 046, 025 for a switch). The item
// a<<12 | n<<7 | kind goes out first, then each subscript (NEXT_SUB).
//
unsigned Pass2::subscripted(unsigned kind)
{
    push(count_);
    lookup();
    if (desc_) {
        kind_ = (kind_ - 1) & 077777;
        if (kind_)
            return string_element(kind);
        if (type_)
            array_dims();
        else
            kind = 025;
    }
    return subscript_item(kind);
}

// *0567B: the item of an element or a designator, then its subscripts.
unsigned Pass2::subscript_item(unsigned kind)
{
    push(type_);
    emit((Word(desc_) << 12) | ((top(2) << 7) & WORD_MASK) | kind);
    read();
    return NEXT_SUB;
}

// *0554B: a switch designator in a left part: as if called from *0514B.
unsigned Pass2::left_designator()
{
    push(LEFT_COUNT);
    push(0);
    return subscripted(046);
}

// *0612B: the left operand of i+c or i-c, c pushed as c<<12 (030 with c
// comes out at INDEX_DONE).
unsigned Pass2::index_offset(Word c)
{
    push((c << 12) & WORD_MASK);
    return parse(INDEX_DONE);
}

//
// A general subscript i+e or i-e: as the operator's handler would, in a
// frame returning to INDEX_DONE with 0 pushed.
//
unsigned Pass2::index_general(unsigned op)
{
    push(0);
    push(INDEX_DONE);
    push(0);
    push(OPS[op]);
    emit_code_ = 020142 + op;
    return parse_read(ARITH_LEFT);
}

//
// *0575B: the next subscript (read backwards), by how it ends: a constant
// below 2048 is an item 045 c; i+c, i-c with c below 2048, 64 are 030 c
// after i; anything else is the expression, then 030 0.
//
unsigned Pass2::next_subscript()
{
    emit_code_ = CODES[code_].subscript;
    switch (emit_code_) {
    case 1:
        if (small_constant(INTEGER | 2048)) {
            emit_code_ = 045;
            emit_shifted(name(operand_) & 07777);
            read();
            return SUBSCRIPT_DONE;
        }
        [[fallthrough]];
    case 0:
        push(0);
        return parse_read(INDEX_DONE);
    case 2:
        read();
        if (code_ != 024 || !small_constant(INTEGER | 2048))
            return index_general(0);
        return index_offset(name(operand_) & 07777);
    default:
        read();
        if (code_ != 024 || !small_constant(INTEGER | 64))
            return index_general(1);
        return index_offset(raw_rsub(name(operand_), INTEGER | 4096) & 07777);
    }
}

// *0621B: one subscript less; after the last, type_ back and return.
unsigned Pass2::subscript_done()
{
    top(2) = raw_sub(top(2), 1);
    if (top(2))
        return NEXT_SUB;
    type_ = addr(pop());
    pop();
    return RETURN_SUB;
}

// *0616B: after a subscript: its type, then 030 with the offset pushed.
unsigned Pass2::index_done()
{
    index_type();
    emit_code_ = 030;
    emit(030 ^ pop());
    return subscript_done();
}

//
// Code 017, **: unary + and - before the exponent are dropped and counted.
// An integer constant exponent 1..63: the item c<<12 ^ 0152 (real base),
// 0153 (integer), negative as 1 / x**c; on an octal base 0352 with 64+c or
// 64-c. Otherwise by the types: 0147 real**real, 0150 real**int, 0151
// int**int constant, int**int as 0150 after 064, 0350, 0347 for octal.
//
unsigned Pass2::power_item()
{
    push(0); // [2]: the constant exponent
    push(0); // [1]: its sign
    for (;;) {
        read();
        emit_code_ = (code_ - 024) & 077777;
        if (code_ == 046)
            continue;
        if (code_ != 047)
            break;
        top(1) ^= 1;
    }
    auto done = [&] {
        sp_ -= 2;
        emit_code();
        return RETURN_SUB;
    };
    auto error = [&] {
        type_error(POWER_TEXT);
        return done();
    };
    if (code_ == 024) {
        Word c     = name(operand_);
        top(2)     = c;
        emit_code_ = addr(c);
        if (emit_code_ && less(c, INTEGER | 64)) {
            parse_sub(020270);
            emit_code_ = type_ == 2 ? 0153 : 0152;
            if (type_ == 5) {
                Word e = top(1) ? raw_rsub(top(2), INTEGER | 64) : raw_add(top(2), INTEGER | 64);
                emit_code_ = 0352;
                emit_shifted(e);
                sp_ -= 2;
                return RETURN_SUB;
            }
            if (type_ != 1 && type_ != 2)
                return error();
            if (top(1)) {
                bool integer = type_ == 2;
                type_        = (type_ - 2) & 077777;
                if (integer) {
                    emit_code_ = 064;
                    emit_code();
                }
                type_      = 1;
                desc_      = ONE;
                emit_code_ = 023;
                emit_shifted(ONE);
                emit_code_ = 064;
                emit_code();
                emit_code_ = 016;
                emit_code();
                emit_code_ = 0152;
            }
            emit_shifted(top(2));
            sp_ -= 2;
            return RETURN_SUB;
        }
    }
    subtree(020231);
    push(type_);
    if (top(2)) {
        type_      = (type_ - 1) & 077777;
        emit_code_ = type_ ? 026 : 047;
        emit_code();
    }
    subtree(020237);
    Word pair = (Word(type_) << 3) | top(1);
    sp_--;
    switch (pair) {
    case 011:
        emit_code_ = 0147;
        break;
    case 012:
        emit_code_ = 0150;
        break;
    case 052:
        emit_code_ = 0350;
        break;
    case 051:
        emit_code_ = 0347;
        break;
    case 022:
        emit_code_ = 0151;
        if (!top(2)) {
            emit_code_ = 064;
            emit_code();
            emit_code_ = 0150;
            type_      = 1;
        }
        break;
    case 021:
        emit_code_ = 064;
        emit_code();
        emit_code_ = 0147;
        type_      = 1;
        break;
    default:
        return error();
    }
    return done();
}

// *1121B: TYPE ERROR IN CONNECTION WITH BOOLEAN OP. (the code popped).
unsigned Pass2::bool_error()
{
    pop();
    mem(ARGS)     = BOOL_TEXT[0];
    mem(ARGS + 1) = BOOL_TEXT[1];
    type_error_args();
    return RETURN_SUB;
}

//
// *1110B: a boolean operator (its code pushed) on operands of type_:
// boolean as is; octal OR as is, AND and NOT as 0200 + the code; else TYPE
// ERROR.
//
unsigned Pass2::bool_result()
{
    emit_code_ = type_;
    Word code  = pop();
    if (type_ == 3) {
        emit(code);
        return RETURN_SUB;
    }
    if (type_ == 5) {
        unsigned c = addr(code);
        if (c == 2) {
            emit(code);
            return RETURN_SUB;
        }
        code |= 0200;
        if (c == 3 || c == 4) {
            emit(code);
            return RETURN_SUB;
        }
    }
    push(code);
    return bool_error();
}

// Codes 001-003, 043: IMPL OR AND EQUIV.
unsigned Pass2::bool_item()
{
    push(code_);
    parse_sub(020327);
    push(type_);
    subtree(020331);
    if (type_pair(pop()))
        return bool_error();
    return bool_result();
}

// Code 004: NOT.
unsigned Pass2::not_item()
{
    push(code_);
    parse_sub(020333);
    return bool_result();
}

//
// Codes 005-012: relations. Operands of one type, real, integer or string;
// octal ones only for = and ≠; an integer and a real: = ≠ as 0105, 0106.
// Strings: the item (code-4)<<12 ^ 0157. The result is boolean.
//
unsigned Pass2::relation_item()
{
    push(code_);
    parse_sub(020356);
    push(type_);
    bool ok = false;
    switch (type_) {
    case 1:
    case 2:
    case 4:
        ok = true;
        break;
    case 0:
    case 3:
        break;
    case 5:
        emit_code_ = addr(top(2));
        ok         = emit_code_ == 011 || emit_code_ == 012;
        break;
    default:
        throw std::runtime_error("pass 2: type " + std::to_string(type_));
    }
    bool error = !ok;
    Word code  = 0;
    subtree(ok ? 020372 : 020413);
    if (ok) {
        switch (type_pair(pop())) {
        case 0:
            code = pop();
            break;
        case 1:
        case 2:
            code = REL_MIXED[addr(pop()) - 5];
            break;
        default:
            pop();
            error = true;
            break;
        }
    } else {
        sp_ -= 2;
    }
    if (error)
        code = relation_error();
    type_ = (type_ - 4) & 077777;
    if (!type_) {
        emit_code_ = 0157;
        type_      = 3;
        emit_shifted(cyclic_add(code, MINUS_4));
        return RETURN_SUB;
    }
    type_ = 3;
    emit(code);
    return RETURN_SUB;
}

// Code 046: unary +, on a number only.
unsigned Pass2::uplus_item()
{
    parse_sub(020425);
    if (!NUMERIC[type_ & 7])
        type_error(OP_TEXT[0]);
    return RETURN_SUB;
}

// Code 047: unary -, on a number: 047 real, 026 integer.
unsigned Pass2::uminus_item()
{
    parse_sub(020431);
    if (!NUMERIC[type_ & 7]) {
        type_error(OP_TEXT[1]);
        return RETURN_SUB;
    }
    emit_code_ = type_ == 2 ? 026 : 047;
    emit_code();
    return RETURN_SUB;
}

//
// Assignments. The frame: the return address, REPORTED, the count of left
// parts, the type of the right side (0 after the first left part), the type
// to check the next left part against.
//

// *0513B: read the next left part.
unsigned Pass2::next_left()
{
    read();
    return left_count();
}

// *0514B: one left part less (the count is one less than their number);
// after the last, return from the statement.
unsigned Pass2::left_count()
{
    top(3) = cyclic_add(top(3), MINUS_ONE);
    if (top(3) & BIT48) {
        sp_ -= 5;
        return addr(stack_.at(sp_ - STACK));
    }
    top(1) = 0;
    return LEFT_PART;
}

// *0512B: write the left part desc_ with the code emit_code_.
unsigned Pass2::emit_left()
{
    emit_shifted(desc_);
    return next_left();
}

//
// *0475B: after its type check, a left part by its kind: a variable (0100),
// a formal by name (0101, marked assigned; illegal if not assignable), the
// name of a function inside its body (0100 with its word after it, which is
// marked); an element or a switch designator goes to *0554B.
//
unsigned Pass2::after_left()
{
    if (code_)
        return left_designator();
    if (!desc_)
        return next_left();
    switch (kind_) {
    case 0:
        emit_code_ = 0100;
        return emit_left();
    case 2:
    case 3:
        mem(desc_ + 1) |= RESULT_SET;
        if (!less(desc_, mem(BLOCK)) || !less(mem(BLOCK), addr(mem(desc_ + 1)))) {
            illegal_use();
            return NEXT_LEFT;
        }
        desc_      = (desc_ + 1) & 077777;
        emit_code_ = 0100;
        return emit_left();
    case 5:
        mem(desc_) |= ASSIGNED;
        if (mem(desc_) & NOT_ASSIGNABLE) {
            illegal_use();
            return NEXT_LEFT;
        }
        emit_code_ = 0101;
        return emit_left();
    case 1:
    case 4:
    case 6:
    case 7:
        illegal_use();
        return NEXT_LEFT;
    }
    throw std::runtime_error("pass 2: kind " + std::to_string(kind_));
}

//
// *0457B: a left part, against the type at [2]: 063 or 064 when integer and
// real, TYPE ERROR ':=' when [1] is 0, ILLEGAL USE when not alike.
//
unsigned Pass2::left_part()
{
    if (!CODES[code_].variable) {
        subtree(017755);
        message(BAD_LEFT);
        return LEFT_COUNT;
    }
    lookup();
    if (!desc_)
        return after_left();
    switch (type_pair(top(2))) {
    case 0:
        return after_left();
    case 1:
        emit_code_ = 063;
        break;
    case 2:
        emit_code_ = 064;
        break;
    default:
        illegal_use();
        return AFTER_LEFT;
    }
    emit(emit_code_);
    top(2) = type_;
    if (top(1))
        return after_left();
    type_error(ASSIGN_TEXT);
    return AFTER_LEFT;
}

// Code 022, := with n+1 left parts: the right side first.
unsigned Pass2::assign_item()
{
    push(count_);
    parse_sub(017677);
    push(type_);
    push(type_);
    if (!type_) {
        type_error(ASSIGN_TEXT);
        return LEFT_PART;
    }
    return left_part();
}

//
// Conditions.
//

// *2221B: TYPE ERROR IN CONNECTION WITH RELATION.
Word Pass2::relation_error()
{
    mem(ARGS + 1) = REL_TEXT[1];
    return type_error(REL_TEXT[0]);
}

//
// *2205B: a condition ending with a relation (code 0130-0134 in
// emit_code_): a compare-and-jump item. Its operands must be alike; = (0134)
// on an integer and a real becomes 0105; strings (code-0127)<<12 ^ 0157.
// True when the item is the relation's code, left in code.
//
bool Pass2::relation_condition(Word &code)
{
    push(emit_code_);
    parse_sub(021432);
    push(type_);
    bool ok = false;
    switch (type_) {
    case 1:
    case 2:
    case 4:
        ok = true;
        break;
    case 0:
    case 3:
        break;
    case 5:
        ok = top(2) == 0134;
        break;
    default:
        throw std::runtime_error("pass 2: type " + std::to_string(type_));
    }
    subtree(ok ? 021450 : 021447);
    if (!ok)
        relation_error();
    emit_code_ = (type_pair(pop()) - 3) & 077777;
    if (!emit_code_)
        relation_error();
    Word rel = top(1);
    type_    = (type_ - 4) & 077777;
    if (!type_) {
        emit_code_ = 0157;
        emit_shifted(cyclic_add(pop(), MINUS_0127));
        return false;
    }
    if (rel != 0134)
        emit_code_ = 077775;
    emit_code_ = (emit_code_ + 3) & 077777;
    code       = pop();
    if (!emit_code_)
        return true;
    emit_code_ = 0105;
    emit_code();
    return false;
}

//
// *2163B: the condition of an IF, the conversion for the result (0 or 064)
// given, the type of the ELSE branch under it. It must be boolean; it ends
// with 027, the conversion, and type_ = the type of the result.
//
unsigned Pass2::condition(Word conversion)
{
    push(conversion);
    emit_code_  = CODES[code_].condition;
    Word code   = 0;
    bool direct = false;
    switch (emit_code_) {
    case 0:
    case 6:
        subtree(021420);
        type_ = (type_ - 3) & 077777;
        if (type_)
            type_error(IF_TEXT);
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        emit_code_ = 0127 + emit_code_;
        direct     = relation_condition(code);
        break;
    default:
        throw std::runtime_error("pass 2: condition " + std::to_string(emit_code_));
    }
    if (!direct) {
        emit_code_ = 027;
        code       = 027;
    }
    emit(code);
    if (Word c = pop())
        emit(c);
    type_ = addr(pop());
    return RETURN_SUB;
}

//
// Code 045, the end of a conditional expression (read first): 054, the ELSE
// branch, 031, the THEN branch, then the condition. Branches of integer and
// real give a real result (064 after the integer one).
//
unsigned Pass2::cond_expr_item()
{
    emit_code_ = 054;
    emit_code();
    parse_sub(021370);
    emit_code_ = 031;
    push(type_);
    emit(031);
    subtree(021373);
    switch (type_pair(top(1))) {
    case 0:
        break;
    case 1:
        emit_code_ = 064;
        emit(064);
        break;
    case 2:
        top(1)     = 1;
        emit_code_ = 064;
        return condition(064);
    default:
        type_error(IF_TEXT);
        break;
    }
    return condition(0);
}

// Code 030, THEN of an IF statement: a word left on the stack, then the condition.
unsigned Pass2::then_item()
{
    sp_++;
    read();
    return condition(0);
}

//
// FOR statements.
//

//
// *2243B: an element of a FOR list (its last item read), against the
// control variable's type at [1]. A variable of that type (017), a formal
// by name (022, then 0135) or a constant of that type (023) is written as
// is; else the expression (then 0136) with a conversion, TYPE ERROR 'FOR' if
// none. Returns the code to write after it (M12) or 0, the next item read.
//
unsigned Pass2::for_element()
{
    auto same_constant = [&] {
        emit_code_ = 023;
        emit_shifted(operand_);
        read();
        return 0u;
    };
    if (!code_) {
        lookup();
        unsigned after = 0;
        bool ok        = false;
        if (!kind_) {
            if (!type_pair(top(1)))
                ok = true;
            emit_code_ = ok ? 017 : emit_code_;
        } else {
            kind_ = (kind_ - 5) & 077777;
            if (!kind_ && !type_pair(top(1))) {
                ok         = true;
                after      = 0135;
                emit_code_ = 022;
            }
        }
        if (ok) {
            emit_shifted(desc_);
            read();
            return after;
        }
    } else {
        emit_code_ = (code_ - 024) & 077777;
        if (!emit_code_) {
            if (top(1) == 2)
                return same_constant();
        } else {
            emit_code_ = (emit_code_ + 1) & 077777;
            if (!emit_code_ && top(1) == 1)
                return same_constant();
        }
    }
    subtree(021513);
    switch (type_pair(top(1))) {
    case 0:
        return 0136;
    case 1:
        emit_code_ = 064;
        break;
    case 2:
        emit_code_ = 063;
        break;
    default:
        type_error(FOR_TEXT);
        return 0136;
    }
    emit_code();
    return 0136;
}

// *2356B: one element more.
unsigned Pass2::count_element()
{
    mem(ELEMENTS) = cyclic_add(mem(ELEMENTS), 1);
    return DISPATCH;
}

// Code 052, end of a FOR statement (read first): its block becomes BLOCK.
unsigned Pass2::for_end_item()
{
    push(mem(BLOCK));
    mem(BLOCK) = operand_;
    open_scope_at();
    emit_item();
    read();
    return AFTER_END;
}

// Code 033, DO: the control variable (its descriptor and type pushed) must be a number.
unsigned Pass2::do_item()
{
    emit_item();
    lookup();
    read();
    push(desc_);
    push(type_);
    mem(ELEMENTS) = 0;
    if (!NUMERIC[type_ & 7])
        type_error(FOR_TEXT);
    return DISPATCH;
}

// Code 056: a plain element.
unsigned Pass2::plain_item()
{
    read();
    for_element();
    emit_code_ = 056;
    emit_code();
    return count_element();
}

// Code 035: e WHILE b.
unsigned Pass2::while_item()
{
    parse_sub(021541);
    type_ = (type_ - 3) & 077777;
    if (type_)
        type_error(FOR_TEXT);
    for_element();
    emit_code_ = 035;
    emit_code();
    return count_element();
}

//
// Code 034: a STEP b UNTIL c, then the item STEP_KIND (0140 real, 034
// integer). An integer constant step b with an integer constant start a is
// 0137, with b interned without its exponent bits and a as 023.
//
unsigned Pass2::step_item()
{
    read();
    if (unsigned after = for_element())
        emit(after);
    mem(STEP_KIND) = STEP_KINDS[addr(top(1)) & 7];
    bool general   = top(1) != 2 || code_ != 024;
    if (general) {
        if (unsigned after = for_element())
            emit(after);
        for_element();
    } else {
        unsigned step = operand_;
        read();
        if (code_ != 024) {
            emit_code_ = 023;
            emit((Word(step) << 12) ^ 023);
            for_element();
        } else {
            emit_code_     = 0137;
            mem(STEP_KIND) = 0137;
            emit_code_     = addr(mem(FREE_SAVED));
            mem(BLOCK_END) = name(step) & MANTISSA;
            for (;;) {
                Word c = name(emit_code_);
                if (!c) {
                    name(emit_code_) = mem(BLOCK_END);
                    mem(FREE_CONST)  = cyclic_add(emit_code_, MINUS_ONE);
                    break;
                }
                if (c == mem(BLOCK_END))
                    break;
                emit_code_ = (emit_code_ - 1) & 077777;
            }
            emit((Word(emit_code_) << 12) ^ 023);
            emit(cyclic_add(mem(ITEM), MINUS_ONE));
            read();
        }
    }
    emit(mem(STEP_KIND));
    return count_element();
}

//
// Code 032, the control variable: a FOR with several elements gets bit 48
// in its block header. A simple variable (032) or a formal by name (0141,
// marked assigned) of a number type; else ILLEGAL USE.
//
unsigned Pass2::control_item()
{
    if (mem(ELEMENTS) != 1) {
        unsigned block = addr(mem(BLOCK));
        store(block, BIT48 | mem(block));
    }
    pop();
    desc_ = addr(pop());
    descriptor_types();
    emit_code_ = 032;
    bool bad   = false;
    if (kind_) {
        kind_ = (kind_ - 5) & 077777;
        if (kind_ || (mem(desc_) & NOT_ASSIGNABLE)) {
            illegal_use();
            bad = true;
        } else {
            mem(desc_) |= ASSIGNED;
            emit_code_ = 0141;
        }
    }
    if (!bad)
        emit_shifted(desc_);
    mem(REPORTED) = 0;
    read();
    return DISPATCH;
}

//
// *0344B: close the scope of BLOCK, write the item BLOCK<<12 ^ code_, BLOCK
// back from the stack; read on.
//
void Pass2::close_block()
{
    sp_ -= 2;
    close_scope();
    emit_code_ = code_;
    emit_shifted(mem(BLOCK));
    mem(BLOCK) = pop();
    read();
}

//
// Declarations.
//

//
// Code 041, a switch declaration with n+1 designational expressions: 0146
// and the item, then each element: a label (marked used, 0142) or an
// expression (0144); each must be of type 0, else TYPE ERROR SWITCH. Then
// 0143, on at *0330B.
//
unsigned Pass2::switch_item()
{
    emit_code_ = 0146;
    emit_code();
    emit_item();
    push(mem(DLIST_TOP));
    push(count_);
    mem(DLIST_TOP) = 0;
    read();
    for (;;) {
        if (code_) {
            subtree(021664);
            emit_code_ = 0144;
            emit_code();
        } else {
            lookup();
            if (kind_) {
                kind_ = (kind_ - 5) & 077777;
                if (kind_)
                    illegal_use();
            } else {
                store(desc_, mem(desc_) | BIT48);
            }
            store(desc_, mem(desc_) | BIT48);
            read();
            emit_code_ = 0142;
            emit_shifted(desc_);
        }
        if (type_) {
            mem(ARGS + 1) = 0;
            type_error(SWITCH_TEXT);
        }
        top(1) = cyclic_add(top(1), MINUS_ONE);
        if (top(1) & BIT48)
            break;
    }
    pop();
    emit_code_ = 0143;
    emit_code();
    return DECL_END;
}

//
// Code 0104, the end of the bounds of an array segment (read first): the
// block gets bit 16 and the item 0102 once; the bounds are parsed with the
// block's names hidden (BOUND_LOOP). Two constants, upper below 256, lower
// below 64, are kept in BOUNDS for the array record.
//
unsigned Pass2::bounds_item()
{
    emit_code_ = addr(mem(BLOCK));
    if (!(mem(emit_code_) & HAS_ARRAYS)) {
        mem(emit_code_) ^= HAS_ARRAYS;
        emit_code_ = 0102;
        emit_shifted(mem(BLOCK));
    }
    emit_item();
    read();
    mem(HIDDEN) = mem(BLOCK);
    emit_code_  = (code_ - 024) & 077777;
    if (!emit_code_ && small_constant(INTEGER | 256)) {
        mem(BOUNDS) = name(operand_);
        subtree(020213);
        emit_code_ = (code_ - 024) & 077777;
        if (!emit_code_ && small_constant(INTEGER | 64)) {
            mem(BOUNDS) |= (name(operand_) << 12) & WORD_MASK;
            return BOUND_LOOP;
        }
    }
    mem(BOUNDS) = 0;
    return BOUND_LOOP;
}

//
// Code 040, an array segment: the frame of the bound loop is dropped; with
// constant bounds an array record BOUNDS<<24 | item goes to ARRAYS_END.
//
unsigned Pass2::array_item()
{
    mem(HIDDEN) = 0;
    sp_ -= 2;
    emit_item();
    if (Word b = mem(BOUNDS)) {
        store(addr(mem(ARRAYS_END)), ((b << 24) & WORD_MASK) | mem(ITEM));
        mem(ARRAYS_END) = cyclic_add(mem(ARRAYS_END), 1);
    }
    read();
    return DISPATCH;
}

// Code 044, a syntax error: items are skipped up to a statement marker.
unsigned Pass2::syntax_error_item()
{
    do
        read();
    while (code_ != 051);
    code_ = 0;
    return SET_POSITION;
}

//
// *0330B, after a procedure or a switch declaration: DLIST_TOP back from
// the stack; 0145 and return, or for one declared in the outermost block
// its formals marked (*1617B).
//
unsigned Pass2::decl_end()
{
    desc_          = addr(mem(DLIST_TOP));
    mem(DLIST_TOP) = pop();
    if (outermost(desc_)) {
        desc_ = (desc_ - 1) & 077777;
        proc_params();
        return RETURN_SUB;
    }
    emit_code_ = 0145;
    emit_code();
    return RETURN_SUB;
}

//
// Code 042, a procedure declaration (read after its body): its scope is
// closed. A function (a type in bits 44-41 of its descriptor) whose result
// was never assigned: NO ASSIGNMENT TO.
//
unsigned Pass2::procedure_item()
{
    close_block();
    desc_ = addr(mem(DLIST_TOP));
    if ((mem(desc_ - 1) & FUNCTION) && !(mem(desc_) & RESULT_SET)) {
        operand_ = addr(mem(desc_ - 1));
        message_name(NO_ASSIGNMENT);
    }
    return decl_end();
}

//
// Code 062, the end of a procedure body (read first): DLIST_TOP pushed and
// set to it; 0146 unless declared in the outermost block; its scope is
// opened as a FOR's.
//
unsigned Pass2::body_end_item()
{
    push(mem(DLIST_TOP));
    mem(DLIST_TOP) = operand_;
    if (!outermost(operand_)) {
        emit_code_ = 0146;
        emit_code();
    }
    return FOR_END;
}

//
// *1324B: a string slice S[i] or S[i:j] (the marker 050 or 0250 pushed):
// S must be a string (017, or 022 by name), the indices integers.
//
unsigned Pass2::slice()
{
    lookup();
    if (desc_) {
        if ((mem(desc_) ^ STRING_DESC) & STRING_MASK) {
            illegal_use();
        } else {
            emit_code_ = kind_ ? 022 : 017;
            emit_shifted(desc_);
        }
    }
    parse_sub(020557);
    index_type();
    if (top(1) == SLICE_CODE) {
        subtree(020562);
        index_type();
    }
    type_ = 4;
    emit(pop());
    return RETURN_SUB;
}

//
// *1342B: an element of a string S[i] (from subscripted): one index, not a
// left part; otherwise an illegal use, and on as an element.
//
unsigned Pass2::string_element(unsigned kind)
{
    count_ = (count_ - 1) & 077777;
    if (count_) {
        illegal_use();
        return subscript_item(kind);
    }
    kind = (kind - 046) & 077777;
    if (!kind) {
        illegal_use();
        return subscript_item(kind);
    }
    emit_code_ = 0250;
    top(1)     = 0250;
    return slice();
}

//
// *0312B, at a statement marker: the position text "line/ s" from operand_
// and count_, then the marker is written.
//
unsigned Pass2::set_position()
{
    unsigned fill;
    mem(LINE_TEXT) = (itoa6(operand_, fill) << 16) & WORD_MASK;
    mem(STMT_TEXT) = (itoa6(count_, fill) << 32) & WORD_MASK;
    emit_code_     = fill;
    emit_item();
    read();
    return DISPATCH;
}

// Code 026, GOTO: the designational expression after it must be a label.
unsigned Pass2::goto_item()
{
    parse_sub(017647);
    if (type_ && !mem(REPORTED))
        type_error(GOTO_TEXT);
    return DISPATCH;
}

//
// Code 061, VALUE: each value formal loses bit 48 and is written with its
// item; a label, string or other must not be by value (ILLEGAL VALUE
// PARAMETER); 0155 at the end.
//
unsigned Pass2::value_item()
{
    emit_code_ = 0127;
    emit_code();
    do {
        desc_    = operand_;
        operand_ = addr(mem(desc_));
        descriptor_types();
        mem(desc_) &= ~BIT48;
        bool bad = false;
        switch (type_) {
        case 0:
            bad = true;
            break;
        case 1:
        case 2:
        case 3:
            bad = kind_;
            break;
        case 4:
        case 5:
            break;
        default:
            throw std::runtime_error("pass 2: type " + std::to_string(type_));
        }
        if (type_ == 1 && !kind_) {
            emit_code_ = 0154;
        } else {
            if (bad)
                message_name(BAD_VALUE);
            emit_code_ = code_;
        }
        emit_shifted(desc_);
        read();
    } while (code_ == 061);
    emit_code_ = 0155;
    emit_code();
    return DISPATCH;
}

//
// I/O statements.
//

// 20457: the last argument of an I/O statement, an integer; then 0121.
unsigned Pass2::io_last()
{
    loop(io_arg(020460), 020460);
    type_ = (type_ - 2) & 077777;
    if (type_)
        message(IO_ERROR);
    emit_code_ = 0121;
    emit_code();
    return RETURN_SUB;
}

// 20453: a string argument, then the last one.
unsigned Pass2::io_string()
{
    loop(io_arg(020454), 020454);
    type_ = (type_ - 4) & 077777;
    if (type_ && !mem(REPORTED))
        message(IO_ERROR);
    return io_last();
}

// Codes 064, 063: desc_ 0473, a string and an integer argument.
unsigned Pass2::io_write(unsigned code)
{
    desc_      = code;
    emit_code_ = 0473;
    emit_shifted(desc_);
    read();
    return io_string();
}

// Codes 100, 103, 070-072: desc_ 0273 and an integer argument.
unsigned Pass2::io_control(unsigned code)
{
    desc_      = code;
    emit_code_ = 0273;
    emit_shifted(desc_);
    read();
    return io_last();
}

//
// Code 065, an input statement: a string variable is written as 017 with
// its item; anything else must be a string (ILLEGAL INPUT OF) and becomes
// the left parts of an assignment.
//
unsigned Pass2::io_065()
{
    read();
    lookup();
    if (type_ != 4)
        message_name(BAD_INPUT);
    if (!code_) {
        kind_ = (kind_ - 1) & 077777;
        if (!kind_) {
            emit_code_ = 017;
            emit_shifted(desc_);
            emit(IO_ITEM_NAME);
            read();
            return RETURN_SUB;
        }
    }
    emit(IO_ITEM);
    lookup();
    push(1);
    push(type_);
    push(type_);
    return LEFT_PART;
}

// Code 074: an array argument as 017 with its item, else the expression.
unsigned Pass2::io_074()
{
    read();
    if (!code_) {
        lookup();
        kind_ = (kind_ - 1) & 077777;
        if (!kind_) {
            emit_code_ = 017;
            emit_shifted(desc_);
            emit(IO_ITEM_ARRAY);
            read();
            return RETURN_SUB;
        }
    }
    emit(IO_ITEM);
    return parse_read(RETURN_SUB);
}

// Code 101: 023 0473, a string argument, 0125, the last one.
unsigned Pass2::io_101()
{
    desc_      = 023;
    emit_code_ = 0473;
    emit_shifted(desc_);
    read();
    loop(io_arg(020526), 020526);
    type_ = (type_ - 4) & 077777;
    if (type_)
        message(IO_ERROR);
    emit(IO_101);
    emit_code_ = 0125;
    emit_code();
    emit_code_ = 0121;
    emit_code();
    return RETURN_SUB;
}

// Codes 066, 067: their item, then two arguments as 064, 063.
unsigned Pass2::io_two(Word item)
{
    emit(item);
    read();
    loop(io_arg(020453), 020453);
    return io_string();
}

//
// Calls. The arguments are read last to first; [1] counts them, [2] keeps
// the result type. formal_ walks the formals of an ALGOL procedure (0 for
// one without a description).
//

// *1424B: the end of a call: emit_code_ written, type_ = the result type.
unsigned Pass2::call_end()
{
    sp_--;
    type_ = addr(pop());
    emit_code();
    return RETURN_SUB;
}

// *1427B: the type of the formal formal_.
unsigned Pass2::formal_type()
{
    emit_code_ = (mem(formal_) >> 40) & 017;
    return FORMAL_TYPE_OF[emit_code_];
}

// *1421B: one argument less, the next formal; after the last, 0110.
unsigned Pass2::actual_count()
{
    if (formal_)
        formal_ = (formal_ + 1) & 077777;
    top(1) = cyclic_add(top(1), MINUS_ONE);
    if (!(top(1) & BIT48))
        return next_actual();
    emit_code_ = 0110;
    return call_end();
}

// *1420B: 0120 after an argument.
unsigned Pass2::actual_done()
{
    emit_code_ = 0120;
    emit_code();
    return actual_count();
}

//
// *1501B: NO ACTUAL-FORMAL CORRESPONDENCE BETWEEN the actual (its name, or
// for an expression its kind) AND the formal.
//
unsigned Pass2::mismatch()
{
    if (mem(REPORTED))
        return actual_count();
    mem(ARGS)      = saved_name(addr(mem(formal_)));
    mem(ARGS2)     = saved_name(addr(mem(desc_)));
    mem(ARGS + 1)  = 0;
    mem(ARGS2 + 1) = 0;
    if (kind_ & 010) {
        mem(ARGS2)     = ACTUAL_TEXT[kind_ - 010][0];
        mem(ARGS2 + 1) = ACTUAL_TEXT[kind_ - 010][1];
    }
    message(NO_CORRESPONDENCE);
    return actual_count();
}

// *1544B: the actual's type_ against the formal's.
unsigned Pass2::check_types()
{
    if (!type_pair(formal_type()))
        return actual_count();
    return mismatch();
}

//
// *1460B: an actual of kind_ against the formal formal_ (desc_: the actual's
// descriptor): the table CORRESPONDENCE (row by kind_, a 4-bit field by the
// formal's kind) says which marks they exchange and whether their
// parameters or dimensions must agree.
//
unsigned Pass2::correspond()
{
    emit_code_ = addr(pack_bits(mem(formal_), FORMAL_BITS) >> 42);
    emit_code_ = CORRESPONDENCE[kind_][FORMAL_CLASS[emit_code_] / 2];

    unsigned f = formal_, a = desc_;
    auto get     = [&](unsigned p) { return mem(p); };
    auto clear48 = [&](Word w) {
        store(f, w & ~BIT48);
        return check_types();
    };
    auto value = [&] {
        store(a, get(a) | BIT39);
        if (get(a) & PARAMS)
            return mismatch();
        return clear48(get(f));
    };
    switch (emit_code_) {
    case 0:
        return check_types();
    case 1: {
        Word diff = (get(a) ^ get(f)) & PARAMS;
        if (!diff)
            return check_types();
        if (get(f) & PARAMS) {
            if (get(a) & PARAMS)
                return mismatch();
            store(a, get(a) ^ diff);
            return check_types();
        }
        store(f, get(f) ^ diff);
        return check_types();
    }
    case 2:
        store(f, get(f) | BIT39);
        if (get(f) & PARAMS)
            return mismatch();
        return check_types();
    case 3:
        return clear48(get(f));
    case 4:
        store(a, get(a) | ASSIGNED);
        return clear48(get(f));
    case 5:
        store(f, get(f) | NOT_ASSIGNABLE);
        return check_types();
    case 6:
        return value();
    case 7:
        store(f, get(f) | NOT_ASSIGNABLE);
        return value();
    case 010: // past the table: the code after it
        return mismatch();
    }
    throw std::runtime_error("pass 2: correspondence " + std::to_string(emit_code_));
}

// *1455B: write a<<12 ^ emit_code_ for an argument; with a formal, correspondence.
unsigned Pass2::actual_emit(Word a)
{
    emit_shifted(a);
    read();
    if (!formal_)
        return actual_count();
    return correspond();
}

//
// *1631B: an expression argument becomes a thunk: 0116 with the counter at
// BLOCK_DLIST (one level down) and its own DLIST_TOP; formal_ and both pushed.
//
void Pass2::thunk()
{
    push(formal_);
    push(mem(DLIST_TOP));
    mem(DLIST_TOP) = 0;
    Word level     = mem(BLOCK_DLIST);
    push(level);
    mem(BLOCK_DLIST) = cyclic_add(level, LEVEL_DOWN);
    emit_code_       = 0116;
    emit(0116 | top(1));
}

// *1641B: the end of a thunk: 0117, the counter and DLIST_TOP back.
unsigned Pass2::thunk_end()
{
    emit_code_ = 0117;
    emit(0117 | pop());
    mem(DLIST_TOP) = pop();
    formal_        = addr(pop());
    if (!formal_)
        return actual_count();
    return correspond();
}

//
// *1617B: a procedure as an argument or declared in the outermost block:
// its word after the descriptor gets 0100000; its formals (that word's
// address down, count in bits 19-15) lose bit 48. Returns what A holds then.
//
Word Pass2::proc_params()
{
    unsigned info = (desc_ + 1) & 077777;
    unsigned p    = addr(mem(info));
    mem(info) |= HAS_ARRAYS;
    emit_code_ = (mem(desc_) >> 15) & 037;
    if (!emit_code_)
        return 0;
    do {
        p = (p - 1) & 077777;
        mem(p) &= ~BIT48;
    } while (--emit_code_);
    emit_code_ = 0113;
    return desc_;
}

//
// *1570B: an identifier as an argument (looked up): by its kind 0112
// (variable, marked used), 0115 (array), 0113 (procedure), 0114 (label,
// switch, formal by name).
//
unsigned Pass2::actual_ident()
{
    if (!desc_) {
        read();
        return actual_count();
    }
    switch (kind_) {
    case 0:
        emit_code_ = 0112;
        if (!type_) {
            emit_code_ = 0113;
            store(desc_, mem(desc_) | BIT48);
        }
        return actual_emit(desc_);
    case 1:
        emit_code_ = 0115;
        if (mem(desc_) & VALUE_FORMAL)
            emit_code_ = 0114;
        else if (!type_)
            emit_code_ = 0113;
        return actual_emit(desc_);
    case 2:
        emit_code_ = 0113;
        return actual_emit(proc_params());
    case 3:
        emit_code_ = 0113;
        return actual_emit(desc_);
    case 4:
        emit_code_ = 0113;
        if (!(mem(desc_) & BIT48))
            return actual_emit(desc_);
        return actual_emit(illegal_use());
    case 5:
    case 6:
    case 7:
        emit_code_ = 0114;
        return actual_emit(desc_);
    }
    throw std::runtime_error("pass 2: kind " + std::to_string(kind_));
}

// *1434B: an argument by how it ends.
unsigned Pass2::actual_kind()
{
    emit_code_   = CODES[code_].actual;
    auto by_type = [&](unsigned type) {
        type_      = type;
        kind_      = 010;
        emit_code_ = 0111;
        return actual_emit(operand_);
    };
    switch (emit_code_) {
    case 0:
        thunk();
        return THUNK_BODY;
    case 1:
        lookup();
        return actual_ident();
    case 2:
        return by_type(2);
    case 3:
        return by_type(1);
    case 4:
        operand_ = TRUE_SLOT;
        return by_type(3);
    case 5:
        operand_ = FALSE_SLOT;
        return by_type(3);
    case 6:
        thunk();
        return DESIGNATOR_THUNK;
    case 7:
        return by_type(5);
    case 010:
        type_      = 4;
        emit_code_ = 0156;
        kind_      = 012;
        desc_      = 0;
        return actual_emit(operand_);
    }
    throw std::runtime_error("pass 2: argument " + std::to_string(emit_code_));
}

//
// *1404B: the next argument. To a formal by value or without a formal it is
// classified (actual_kind); else it is an expression of the formal's type
// (064 when integer for real), then 0120.
//
unsigned Pass2::next_actual()
{
    if (!formal_ || (mem(formal_) & VALUE_FORMAL))
        return actual_kind();
    push(formal_);
    subtree(020633);
    formal_ = addr(pop());
    kind_   = 012;
    switch (type_pair(formal_type())) {
    case 0:
        return actual_done();
    case 1:
        emit_code_ = 064;
        emit_code();
        return actual_done();
    default:
        return mismatch();
    }
}

// *1402B: the call's item 020 with p, then the arguments.
unsigned Pass2::call_head()
{
    emit_code_ = 020;
    emit_shifted(desc_);
    read();
    return next_actual();
}

//
// Code 020, a call of p with n arguments: the number of parameters must
// match (a procedure without them gets n). STD_PROC is the standard
// procedure with an expression argument; non-ALGOL procedures go on at
// CODE_CALL.
//
unsigned Pass2::call_item()
{
    formal_ = 0;
    lookup();
    push(type_);
    push(count_);
    if (mem(REPORTED))
        return call_head();
    emit_code_ = (desc_ - STD_PROC) & 077777;
    if (!emit_code_) {
        emit_code_ = 057;
        emit_shifted(count_);
        return parse(ACTUAL_DONE);
    }
    switch (kind_) {
    case 2:
        formal_ = addr(raw_sub(addr(mem(desc_ + 1)), top(1)));
        break;
    case 4:
        if (mem(desc_) & BIT48)
            return CODE_CALL;
        break;
    case 6:
        break;
    case 0:
    case 1:
    case 3:
    case 5:
    case 7:
        illegal_use();
        return call_head();
    default:
        throw std::runtime_error("pass 2: kind " + std::to_string(kind_));
    }
    if (Word dims = (mem(desc_) >> 15) & 037) {
        if (dims != count_) {
            illegal_use();
            return call_head();
        }
    } else {
        mem(desc_) ^= (Word(count_) << 15) & WORD_MASK;
    }
    return call_head();
}

//
// Non-ALGOL calls. Arguments go through io_arg, which pushes the address to
// return to.
//

// *1722B: a label argument (type_ 0): LABEL AS ARGUMENT OF NON-ALGOL CALL.
unsigned Pass2::arg_label()
{
    if (!type_ && !mem(REPORTED))
        message(LABEL_ARG);
    return arg_return();
}

// *1716B: the item emit_code_ of an argument (074 for a string).
unsigned Pass2::arg_item()
{
    if (type_ == 4)
        emit_code_ = 074;
    emit_code();
    return ARG_LABEL;
}

// 21122: an argument of a non-ALGOL call or an I/O statement, by how it ends.
unsigned Pass2::io_arg(unsigned ret)
{
    push(ret);
    emit_code_ = CODES[code_].io;
    switch (emit_code_) {
    case 0:
        return parse_read(ARG_EXPRESSION);
    case 1:
        lookup();
        return arg_ident();
    case 2:
        emit_code_ = 066;
        emit_shifted(operand_);
        type_ = 4;
        read();
        return ARG_RETURN;
    default:
        push(ARG_DESIGNATOR);
        push(0);
        return subscripted(0103);
    }
}

// *1726B: one argument less; after the last, 0121.
unsigned Pass2::arg_count()
{
    top(1) = cyclic_add(top(1), MINUS_ONE);
    if (!(top(1) & BIT48))
        return io_arg(ARG_COUNT);
    emit_code_ = 0121;
    return call_end();
}

//
// *1740B: an identifier argument (looked up): 0122 (variable), 0123
// (array), 022 (formal by name) with its item; a string variable,
// procedures and others as expressions.
//
unsigned Pass2::arg_ident()
{
    auto item = [&] {
        emit_shifted(desc_);
        read();
        return ARG_LABEL;
    };
    if (mem(REPORTED))
        return item();
    if (type_ == STRING_TYPE)
        return parse_read(ARG_EXPRESSION);
    switch (kind_) {
    case 0:
        emit_code_ = 0122;
        return item();
    case 1:
        emit_code_ = 0123;
        return item();
    case 4:
        emit_code_ = 0122;
        if (!(mem(desc_) & BIT48))
            return parse_read(ARG_EXPRESSION);
        type_ = 1;
        return item();
    case 5:
        emit_code_ = 022;
        emit_shifted(desc_);
        read();
        return ARG_DESIGNATOR;
    case 2:
    case 3:
    case 6:
    case 7:
        return parse_read(ARG_EXPRESSION);
    }
    throw std::runtime_error("pass 2: kind " + std::to_string(kind_));
}

// The end of a standard procedure's arguments: type_ back, return.
unsigned Pass2::std_end()
{
    sp_--;
    type_ = addr(pop());
    return RETURN_SUB;
}

// *2101B (PRINT): one argument less.
unsigned Pass2::print_count()
{
    top(1) = cyclic_add(top(1), MINUS_ONE);
    if (!(top(1) & BIT48))
        return print_arg();
    return std_end();
}

// *2057B: a PRINT argument desc_ + type_ with 071; type 0: ILLEGAL ARGUMENT OF PRINT.
unsigned Pass2::print_item()
{
    desc_      = (desc_ + type_) & 077777;
    emit_code_ = 071;
    emit_shifted(desc_);
    if (!type_)
        message(BAD_PRINT);
    return print_count();
}

//
// *2052B: an argument of PRINT: an array (017 a, then 017+type), a
// procedure (057 or 016 by its type, then 072), else an expression (010
// + type).
//
unsigned Pass2::print_arg()
{
    auto expression = [&] {
        subtree(021330);
        desc_ = 010;
        return print_item();
    };
    if (code_)
        return expression();
    lookup();
    if (desc_) {
        if (kind_ == 1) {
            emit_code_ = 017;
            emit_shifted(desc_);
            desc_ = 017;
            read();
            return print_item();
        }
        if (kind_ != 2)
            return expression();
        emit_code_ = (((mem(desc_) >> 32) & 7) - 4) & 077777;
        desc_      = 057;
        if (emit_code_) {
            emit_code_--;
            desc_ = 016;
            if (emit_code_)
                illegal_use();
        }
        emit_code_ = 072;
        emit_shifted(desc_);
    }
    read();
    return print_count();
}

// *2117B (READ-like 3): one argument less.
unsigned Pass2::read_count()
{
    top(1) = cyclic_add(top(1), MINUS_ONE);
    if (!(top(1) & BIT48))
        return read_arg();
    return std_end();
}

//
// *2131B: an argument as the left parts of an assignment, in a frame
// returning to ret.
//
unsigned Pass2::as_left_part(unsigned ret)
{
    emit_code_ = ret;
    push(ret);
    push(mem(REPORTED));
    push(1);
    push(type_);
    push(type_);
    return LEFT_PART;
}

// *2107B: an argument of the third standard procedure.
unsigned Pass2::read_arg()
{
    auto general = [&] {
        if (!(raw_sub(mem(desc_) & STD_PROC_BITS, 0200000000000) & NEGATIVE)) {
            desc_      = 062;
            emit_code_ = 072;
            emit_shifted(desc_);
            read();
            return read_count();
        }
        desc_      = (051 + type_) & 077777;
        emit_code_ = 072;
        emit_shifted(desc_);
        return as_left_part(READ_COUNT);
    };
    lookup();
    if (code_)
        return general();
    if (!desc_) {
        read();
        return read_count();
    }
    if (!type_ || kind_ != 1)
        return general();
    emit_code_ = 017;
    emit_shifted(desc_);
    desc_      = (072 + type_) & 077777;
    emit_code_ = 071;
    emit_shifted(desc_);
    read();
    return read_count();
}

//
// *2015B: a standard procedure (its number): 2 PRINT, 6 LENGTH, 3 and 7 by
// arguments, 1 a function of one number argument (071 with desc_ - 05760).
//
unsigned Pass2::std_proc(Word number)
{
    emit_code_ = number;
    if (number == 2 || number == 6 || number == 3 || number == 7)
        emit_code_ = 0;
    switch (number) {
    case 2:
        read();
        return print_arg();
    case 6:
        sp_ -= 2;
        parse_sub(021262);
        type_ = (type_ - 4) & 077777;
        if (type_)
            message(BAD_LENGTH);
        emit_code_ = 071;
        desc_      = 0100;
        type_      = 2;
        emit_shifted(desc_);
        return RETURN_SUB;
    case 3:
        read();
        return read_arg();
    case 7:
        count_ = (count_ - 2) & 077777;
        if (count_) {
            illegal_use();
            return call_head();
        }
        parse_sub(021364);
        lookup();
        return as_left_part(STD_END);
    case 1:
        break;
    default:
        emit_code_ = (emit_code_ - 1) & 077777;
        illegal_use();
        return CODE_CALL_ARGS;
    }
    emit_code_ = 0;
    count_     = (count_ - 1) & 077777;
    if (count_) {
        illegal_use();
        return CODE_CALL_ARGS;
    }
    push(desc_);
    parse_sub(021251);
    desc_ = addr(pop());
    if (!NUMERIC[type_ & 7]) {
        operand_ = addr(mem(desc_));
        illegal_use();
    }
    desc_      = (desc_ - 05760) & 077777;
    emit_code_ = 071;
    emit_shifted(desc_);
    return std_end();
}

//
// 21110: a call of a non-ALGOL procedure: standard ones by their number in
// bits 39-37; else the item 0126 (065 for a typed one) p<<12 | n<<7, then
// each argument.
//
unsigned Pass2::code_call()
{
    if (Word bits = pack_bits(mem(desc_), STD_PROC_BITS))
        return std_proc(bits >> 45);
    return CODE_CALL_ARGS;
}

// *1667B: the item of a non-ALGOL call, then its arguments.
unsigned Pass2::code_call_args()
{
    emit_code_ = type_ ? 065 : 0126;
    emit((Word(desc_) << 12) ^ (Word(count_) << 7) ^ emit_code_);
    read();
    return io_arg(ARG_COUNT);
}

void Pass2::register_handlers()
{
    auto &h = handlers_;

    // The framework.
    h[NEXT_ITEM] = [this] { // *0217B: the statement loop
        mem(HIDDEN)   = 0;
        mem(REPORTED) = 0;
        return parse_read(AFTER_END);
    };
    h[AFTER_END] = [this] { // *0220B
        mem(REPORTED) = 0;
        return parse_read(AFTER_END);
    };
    h[DISPATCH]    = [this] { return dispatch(); };
    h[RETURN_SUB]  = [this] { return return_sub(); };
    h[NEXT_RETURN] = [this] { // *0402B
        read();
        return RETURN_SUB;
    };
    h[022026] = [this]() -> unsigned { // IF, STRING, EOP: the original jumps to 01103
        throw std::runtime_error("pass 2: unexpected item code " + std::to_string(code_));
    };
    h[022027] = [this] { return finish(); };

    // Blocks and declarations.
    h[017511]       = [this] { return end_item(); };
    h[017523]       = [this] { return begin_item(); };
    h[017533]       = [this] { return syntax_error_item(); };
    h[SET_POSITION] = [this] { return set_position(); };
    h[017543]       = [this] { // code 055, the start of a FOR statement
        close_block();
        return DISPATCH;
    };
    h[017544]     = [this] { return procedure_item(); };
    h[DECL_END]   = [this] { return decl_end(); };
    h[017574]     = [this] { return body_end_item(); };
    h[FOR_END]    = [this] { return for_end_item(); };
    h[021637]     = [this] { return switch_item(); };
    h[020162]     = [this] { return bounds_item(); };
    h[BOUND_LOOP] = [this] { return parse_read(BOUND_DONE); };
    h[BOUND_DONE] = [this] { // a bound must be an integer
        index_type();
        return BOUND_LOOP;
    };
    h[020200] = [this] { return array_item(); };
    h[021206] = [this] { // codes 060, 031, 053, 054: the item as is
        emit_item();
        read();
        return DISPATCH;
    };
    h[021210] = [this] { return value_item(); };

    // Expressions.
    h[017605] = [this] { return identifier(); };
    h[017646] = [this] { return goto_item(); };
    h[017765] = [this] { // TRUE, FALSE: constants of their slots
        operand_ = TRUE_SLOT;
        return constant(3);
    };
    h[017766] = [this] {
        operand_ = FALSE_SLOT;
        return constant(3);
    };
    h[017770]         = [this] { return constant(1); }; // real
    h[017771]         = [this] { return constant(5); }; // octal
    h[017772]         = [this] { return constant(2); }; // integer
    h[017775]         = [this] { return string_item(); };
    h[020106]         = [this] { return arith_op(0); };
    h[020152]         = [this] { return arith_op(1); };
    h[020153]         = [this] { return arith_op(2); };
    h[020154]         = [this] { return divide_item(); };
    h[ARITH_LEFT]     = [this] { return arith_left(); };
    h[020002]         = [this] { return subscripted(044); }; // code 021, an array element
    h[NEXT_SUB]       = [this] { return next_subscript(); };
    h[INDEX_DONE]     = [this] { return index_done(); };
    h[SUBSCRIPT_DONE] = [this] { return subscript_done(); };
    h[020221]         = [this] { return power_item(); };
    h[020325]         = [this] { return bool_item(); };
    h[020350]         = [this] { return not_item(); };
    h[020354]         = [this] { return relation_item(); };
    h[020424]         = [this] { return uplus_item(); };
    h[020430]         = [this] { return uminus_item(); };
    h[020546]         = [this] { // code 050: a slice S[i:j]
        push(SLICE_CODE);
        return slice();
    };
    h[021366] = [this] { return cond_expr_item(); };
    h[021461] = [this] { return then_item(); };

    // Assignments.
    h[017675]     = [this] { return assign_item(); };
    h[LEFT_PART]  = [this] { return left_part(); };
    h[AFTER_LEFT] = [this] { return after_left(); };
    h[NEXT_LEFT]  = [this] { return next_left(); };
    h[LEFT_COUNT] = [this] { return left_count(); };

    // FOR statements.
    h[021524] = [this] { return do_item(); };
    h[021534] = [this] { return plain_item(); };
    h[021540] = [this] { return while_item(); };
    h[021546] = [this] { return step_item(); };
    h[021617] = [this] { return control_item(); };

    // I/O statements.
    h[020445] = [this] { return io_write(051); };
    h[020446] = [this] { return io_write(060); };
    h[020447] = [this] { return io_control(025); };
    h[020450] = [this] { return io_control(026); };
    h[020466] = [this] { return io_065(); };
    h[020511] = [this] { return io_074(); };
    h[020521] = [this] { // code 073
        emit(IO_073);
        read();
        return RETURN_SUB;
    };
    h[020523] = [this] { return io_101(); };
    h[020534] = [this] { return io_control(033); };
    h[020535] = [this] { return io_control(034); };
    h[020536] = [this] { return io_control(035); };
    h[020541] = [this] { return io_two(IO_TWO_0101); };
    h[020542] = [this] { return io_two(IO_TWO_0102); };

    // Calls.
    h[020571]      = [this] { return call_item(); };
    h[ACTUAL_DONE] = [this] { return actual_done(); };
    h[THUNK_BODY]  = [this] { // *1637B: the expression of a thunk
        subtree(021063);
        desc_ = 0;
        kind_ = 012;
        return thunk_end();
    };
    h[DESIGNATOR_THUNK] = [this] { // *1646B: an element or a switch designator as an argument
        push(THUNK_DESIGNATOR);
        push(0);
        return subscripted(0103);
    };
    h[THUNK_DESIGNATOR] = [this] { // *1651B: after it
        kind_ = 011;
        return thunk_end();
    };
    h[CODE_CALL]      = [this] { return code_call(); };
    h[CODE_CALL_ARGS] = [this] { return code_call_args(); };
    h[ARG_DESIGNATOR] = [this] { // *1711B: after an element or a switch designator
        emit_code_ = 0124;
        return arg_item();
    };
    h[ARG_EXPRESSION] = [this] { // *1715B: after an expression argument
        emit_code_ = 0125;
        return arg_item();
    };
    h[ARG_LABEL]   = [this] { return arg_label(); };
    h[ARG_RETURN]  = [this] { return arg_return(); };
    h[ARG_COUNT]   = [this] { return arg_count(); };
    h[STD_END]     = [this] { return std_end(); };
    h[PRINT_COUNT] = [this] { return print_count(); };
    h[READ_COUNT]  = [this] { return read_count(); };
}

Pass2::Pass2(Context &ctx, const Stream &input)
    : ctx_(ctx), in_(input), pos_(input.size()), flgex_(ctx.flgex)
{
    operand_ = ctx.regs[3] & 077777;
    desc_    = ctx.regs[4] & 077777;
    type_    = ctx.regs[5] & 077777;
    kind_    = ctx.regs[6] & 077777;
    register_handlers();
}

Stream Pass2::run()
{
    loop(start(), DRIVER);
    ctx_.flgex = unsigned(flgex_);
    return Stream(out_.rbegin(), out_.rend());
}

} // namespace

Stream pass2(Context &ctx, const Stream &input)
{
    return Pass2(ctx, input).run();
}
