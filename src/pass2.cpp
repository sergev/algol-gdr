#include "pass2.h"

#include "translated.h"

unsigned pass2_code(Machine &m, unsigned pc);

namespace {

// Cells of the workspace.
constexpr unsigned READ_CURSOR  = 02000;
constexpr unsigned WRITE_CURSOR = 02001;
constexpr unsigned FREE_CONST   = 02003; // the lowest constant slot
constexpr unsigned DLIST_TOP    = 02004; // M3 at the start
constexpr unsigned ARGS         = 02007; // data of the messages: the name, 0, the position
constexpr unsigned POSITION     = 02011; // 6 words: "PROGR/END" before any marker
constexpr unsigned LINE_TEXT    = 02015; // of POSITION: the line number
constexpr unsigned STMT_TEXT    = 02016; // the statement on the line
constexpr unsigned BLOCK_DLIST  = 02022; // M3<<12
constexpr unsigned FREE_SAVED   = 02025;
constexpr unsigned DLIST_TOP2   = 02026;
constexpr unsigned HIDDEN       = 02005; // a block whose names are not visible here
constexpr unsigned BLOCK_END    = 02017; // the end of the block being scanned
constexpr unsigned REPORTED     = 02027; // a message in this subtree
constexpr unsigned BLOCK        = 02030; // the innermost open block
constexpr unsigned ARRAYS_END   = 02032; // end of the array records at 04000
constexpr unsigned NAMES        = 02144; // slot i at NAMES + i
constexpr unsigned ARRAYS       = 04000;
constexpr unsigned DLIST_END    = 06000;
constexpr unsigned COMMON       = 027245; // 3 words per element: name, D3<<36 | slot<<24, size
constexpr unsigned FLGEX        = 00702;

// Routines and data of the translated pass.
constexpr unsigned READ_BACK    = 027143; // the previous item, in A
constexpr unsigned PRINT        = 01644;  // text first..last on the stack, indent in A
constexpr unsigned FORMAT       = 027733; // text first on the stack, last in A
constexpr unsigned OPEN_SCOPE   = 017455; // *0232B: through M12
constexpr unsigned EMIT_M14     = 017370; // *0145B: item M14, through M13
constexpr unsigned MAIN_LOOP    = 017352; // *0127B, with M13 = *0217B
constexpr unsigned NEXT_ITEM    = 017442; // *0217B
constexpr unsigned SAVE_NAMES   = 017436; // *70 info: page 02000 to drum 26 zone 0
constexpr unsigned SWAP_OUT     = 0214;   // M7+: page 02000 to drum 26 zone 1
constexpr unsigned NAMES_IN     = 0215;   // the names from drum 26 zone 0
constexpr unsigned SWAP_IN      = 0216;   // page 02000 from drum 26 zone 1
constexpr unsigned ITOA         = 030001; // A = six characters of the number in A
constexpr unsigned EMIT_MARKER  = 017363; // *0140B: item 02006, through M13
constexpr unsigned AFTER_STMT   = 017450; // *0225B
constexpr unsigned AFTER_END    = 017443; // *0220B
constexpr unsigned EMIT_SHIFTED = 017366; // *0143B: item A<<12 ^ M14, through M13
constexpr unsigned TYPES        = 021720; // per index: type in bits 7-5, kind in 4-1
constexpr unsigned UNDECLARED   = 021676; // formats
constexpr unsigned ILLEGAL_USE  = 021704;
constexpr unsigned TWICE        = 021712;
constexpr unsigned TYPE_ERROR   = 017655; // format: TYPE ERROR IN CONNECTION WITH ...
constexpr unsigned GOTO_TEXT    = 022141; // its argument
constexpr unsigned TYPE_PAIRS   = 017667; // 6 rows by type: 3-bit fields, the shift in the exponent
constexpr unsigned READ         = 017352; // *0127B: the previous item into M1-M3, through M13
constexpr unsigned PARSE        = 017445; // *0222B: a subtree, through M12
constexpr unsigned DISPATCH     = 017450; // *0225B: the handler of the item read
constexpr unsigned RETURN_SUB   = 017451; // *0226B: return from a subtree
constexpr unsigned EMIT_M4      = 017365; // *0142B: item M4<<12 ^ M14, through M13
constexpr unsigned NEXT_RETURN  = 017625; // *0402B: read the next item, return from the subtree
constexpr unsigned PARSE_READ   = 017446; // *0223B: the subtree of the item read, through M12
constexpr unsigned EMIT_A       = 017364; // *0141B: item A, through M13
constexpr unsigned OPS          = 020142; // + - * /: real code <<24, integer code <<30, text
constexpr unsigned STRING_PLUS  = 022137; // the item of + on strings
constexpr unsigned OPS_TEXT     = 020146; // '+', '-', '*', '/'
constexpr unsigned INDEX_TEXT   = 022121; // INDEX
constexpr unsigned SUBSCRIPT    = 020003; // *0560B
constexpr unsigned NEXT_SUB     = 020020; // *0575B
constexpr unsigned INDEX_DONE   = 020041; // *0616B
constexpr unsigned ARITH_LEFT   = 020110; // *0665B
constexpr unsigned POWER_TEXT   = 022065; // '**'
constexpr unsigned BOOL_TEXT    = 020352; // BOOLEAN OP., 2 words
constexpr unsigned REL_TEXT     = 020422; // RELATION, 2 words
constexpr unsigned REL_MIXED    = 020400; // by relation code: the code for an integer and a real
constexpr unsigned NUMERIC      = 020317; // by type: nonzero for real and integer
constexpr unsigned ONE          = 01632;  // the slot of the constant 1
constexpr unsigned ASSIGN_TEXT  = 022110; // ':='
constexpr unsigned LEFT_PART    = 017702; // *0457B
constexpr unsigned AFTER_LEFT   = 017720; // *0475B
constexpr unsigned NEXT_LEFT    = 017736; // *0513B
constexpr unsigned LEFT_COUNT   = 017737; // *0514B
constexpr unsigned BAD_LEFT     = 017757; // format: ILLEGAL LEFT PART OR INPUT ELEMENT
constexpr unsigned NOT_DECLARED = 017311; // text, name at +010
constexpr unsigned WRONG_ORDER  = 017322; // text, name at +010
constexpr unsigned INCORRECT    = 022051; // text up to 022056
constexpr unsigned SCRATCH      = 017333; // *0110B
constexpr unsigned POSITION_SRC = 021670;

constexpr Word HEADER         = 0'4000'0000'0000'0000; // D-list: a block header
constexpr Word DESCRIPTOR     = 0'2000'0000'0000'0000;
constexpr Word BIT41          = 0'0020'0000'0000'0000;
constexpr Word BIT48          = 0'4000'0000'0000'0000;
constexpr Word BIT39          = 0'0000'4000'0000'0000;
constexpr Word PARAMS         = 0'0000'0000'0370'0000; // number of parameters
constexpr Word VARIABLE       = 020000;                // TYPES: the item can be a left part
constexpr Word ASSIGNED       = 0'0004'0000'0000'0000; // a formal by name assigned to
constexpr Word NOT_ASSIGNABLE = 0'0010'0000'0000'0000;
constexpr Word RESULT_SET     = 0200000; // in a function's word after it: assigned
constexpr Word IN_COMMON_CLR  = 0'7770'0777'7777'7777;
constexpr Word IN_COMMON      = 0'0001'0000'0000'0000;
constexpr Word DIMENSIONS     = 0'0000'0000'0070'0000;
constexpr Word COMMON_SIMPLE  = 0'0000'0000'0010'0000;
constexpr Word RECORD_CLR     = 0'7777'7400'0377'7777; // descriptor without bits 33-21: the
                                                       // shadowed binding, then the array record

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

// A message of the 'COMMON' check: the text with the element's name, through
// 01644, saving M8 and M13 as the original does.
void common_message(Machine &m, unsigned text, Word name)
{
    m.mem[text + 010] = name;
    m.mem[FLGEX]      = cyclic_add(m.mem[FLGEX], 1);
    m.acc             = m.mem[FLGEX];
    m.push(m.acc);
    m.push(m.M[8]);
    m.push(m.M[13]);
    m.push(text);
    m.set_m(14, 5);
    m.acc = text + 010;
    m.push(m.acc);
    m.acc = 5;
    m.call(PRINT, 13, 017304);
    m.sti(0, 13);
    m.sti(0, 8);
}

//
// *0205B, after a message: the first one in a subtree sets FLGEX* and
// REPORTED. Returns through M8.
//
unsigned after_message(Machine &m)
{
    m.set_m(4, 0);
    m.set_m(5, 0);
    m.acc = m.mem[REPORTED];
    m.rmr = m.acc;
    if (!m.acc) {
        m.acc           = 1;
        m.mem[FLGEX]    = 1;
        m.mem[REPORTED] = 1;
    }
    m.set_logical();
    return m.M[8];
}

//
// *0203B: the message of the format in M13, with the data at ARGS.
//
unsigned message(Machine &m)
{
    m.push(m.acc);
    m.acc = m.M[13];
    m.set_m(14, ARGS);
    m.push(m.acc);
    m.acc = ARGS;
    m.call(FORMAT, 13, 017430);
    return after_message(m);
}

//
// *0175B: the message of the format in M13 about the name of slot M3, read
// from the names saved on drum 26 zone 0.
//
unsigned message_name(Machine &m)
{
    m.extracode(070, 7, SWAP_OUT);
    m.extracode(070, 7, NAMES_IN);
    m.acc = m.mem[NAMES + m.M[3]];
    m.push(m.acc);
    m.extracode(070, 7, SWAP_IN);
    m.xta(Machine::SP, 0);
    m.mem[ARGS]     = m.acc;
    m.acc           = 0;
    m.mem[ARGS + 1] = 0;
    return message(m);
}

//
// *0211B: ILLEGAL USE OF IDENTIFIER of slot M3, unless reported in this
// subtree. Returns through M8.
//
unsigned illegal_use(Machine &m)
{
    m.set_m(13, ILLEGAL_USE);
    m.acc = m.mem[REPORTED];
    m.rmr = m.acc;
    m.set_logical();
    if (m.acc)
        return m.M[8];
    return message_name(m);
}

//
// *0146B: is the binding M4 inside the block whose header is at A, between
// the header and its end? A = bit 41 if so, else 0; M14 = the header.
// Returns through M13.
//
unsigned inside(Machine &m)
{
    unsigned block = m.acc & 077777;
    m.set_m(14, block);
    unsigned end = m.mem[block] & 077777;
    m.acc        = (m.M[4] > block && m.M[4] < end) ? BIT41 : 0;
    m.rmr        = 0;
    m.rau        = 003;
    m.set_logical();
    return m.M[13];
}

// The kind index of a descriptor: bits 46, 45, 38, 37, 36.
unsigned kind_index(Word d)
{
    return ((d >> 41) & 030) | ((d >> 35) & 07);
}

//
// *0154B: the identifier of slot M3. M4 = its descriptor, M5 = its type,
// M6 = its kind (TYPES). An undeclared one is reported once (its slot gets
// 1); one declared inside the HIDDEN block is an illegal use. Returns through
// M13, kept in M8.
//
unsigned lookup(Machine &m)
{
    m.set_m(8, m.M[13]);
    m.acc = m.mem[NAMES + m.M[3]];
    m.set_logical();
    m.set_m(4, unsigned(m.acc));
    if (!m.M[4]) {
        m.acc                 = 1;
        m.mem[NAMES + m.M[3]] = 1;
        m.set_m(13, UNDECLARED);
        return message_name(m);
    }
    m.rmr = m.acc;
    m.acc ^= 1;
    if (!m.acc)
        return after_message(m);
    m.acc = m.mem[HIDDEN];
    m.rmr = m.acc;
    if (m.acc) {
        m.set_m(13, 017404);
        inside(m);
        m.rmr = m.acc;
        if (m.acc)
            return illegal_use(m);
    }
    Word d = m.mem[m.M[4]];
    m.set_m(14, unsigned(d >> 40) & 017);
    m.set_m(5, unsigned(m.mem[TYPES + m.M[14]] >> 4) & 7);
    m.set_m(14, kind_index(d));
    m.acc = m.mem[TYPES + m.M[14]] & 017;
    m.set_m(6, unsigned(m.acc));
    m.rmr = 0;
    m.set_logical();
    return m.M[8];
}

//
// *0232B: open the scope of the block whose header is at M11, in A. Each
// declaration up to the block's end is bound to its name slot and keeps the
// binding it shadows in bits 33-21; nested blocks are skipped. TWICE
// DECLARED when the shadowed binding is inside the block at M3 (after the
// first message M3 is the slot: the original does so). Returns through M12.
//
unsigned open_scope(Machine &m)
{
    m.acc &= 07777;
    m.mem[BLOCK_END] = m.acc;
    for (;;) {
        m.set_m(11, m.M[11] + 1);
        for (;;) {
            if (m.M[11] == m.mem[BLOCK_END]) {
                m.acc = 0;
                m.rmr = 0;
                m.set_logical();
                return m.M[12];
            }
            Word d = m.mem[m.M[11]];
            if (d & DESCRIPTOR)
                break;
            m.set_m(11, unsigned(d));
        }
        Word d = m.mem[m.M[11]];
        m.set_m(6, unsigned(d));
        Word prev = m.mem[NAMES + m.M[6]];
        m.set_m(4, unsigned(prev));
        m.mem[m.M[11]]        = (d & RECORD_CLR) | ((prev << 20) & WORD_MASK);
        m.mem[NAMES + m.M[6]] = m.M[11];
        m.acc                 = m.M[3];
        m.set_m(13, 017470);
        inside(m);
        m.rmr = m.acc;
        if (m.acc) {
            m.set_m(3, m.M[6]);
            m.set_m(8, 017456);
            m.set_m(13, TWICE);
            message_name(m);
        }
    }
}

// *0231B: open the scope of the block at M3.
unsigned open_scope_at(Machine &m)
{
    m.set_m(11, m.M[3]);
    m.acc = m.mem[m.M[11]];
    m.set_logical();
    return open_scope(m);
}

//
// *0252B: close the scope of the block at BLOCK: each declaration's slot gets
// back the binding it shadowed. Returns through M12.
//
unsigned close_scope(Machine &m)
{
    m.acc = m.mem[BLOCK];
    m.set_m(11, unsigned(m.acc));
    m.mem[BLOCK_END] = m.mem[m.M[11]] & 07777;
    for (;;) {
        m.set_m(11, m.M[11] + 1);
        for (;;) {
            if (m.M[11] == m.mem[BLOCK_END]) {
                m.acc = 0;
                m.rmr = 0;
                m.set_logical();
                return m.M[12];
            }
            Word d = m.mem[m.M[11]];
            if (d & DESCRIPTOR)
                break;
            m.set_m(11, unsigned(d));
        }
        Word d = m.mem[m.M[11]];
        m.set_m(6, unsigned(d));
        m.acc                 = (d >> 20) & 07777;
        m.mem[NAMES + m.M[6]] = m.acc;
    }
}

//
// *0266B, END (the stream is read backwards, so it opens a scope): the block
// at M3 becomes BLOCK, the old one pushed. A block header (bit 48) is written
// out with the old DLIST_TOP, which is pushed and replaced by M3; else 0 is
// pushed. Goes on at the main loop.
//
unsigned end_item(Machine &m)
{
    m.push(m.mem[BLOCK]);
    m.acc        = m.M[3];
    m.mem[BLOCK] = m.acc;
    m.acc        = m.mem[m.M[3]];
    m.rau        = 010;
    m.rmr        = m.acc;
    if (m.acc & BIT48) {
        m.push(m.mem[DLIST_TOP]);
        m.acc            = m.M[3];
        m.mem[DLIST_TOP] = m.acc;
        m.acc            = m.mem[(m.M[Machine::SP] - 1) & 077777];
        m.set_logical();
        m.set_m(14, m.M[1]);
        m.call(EMIT_SHIFTED, 13, 017515);
    } else {
        m.acc = 0;
        m.set_logical();
        m.push(0);
    }
    m.set_m(12, 017516);
    open_scope_at(m);
    m.set_m(13, AFTER_END);
    return MAIN_LOOP;
}

//
// *0300B, BEGIN: closes the scope. For a block header, the item is written
// with BLOCK and DLIST_TOP restored first. BLOCK gets the outer block back.
//
unsigned begin_item(Machine &m)
{
    m.set_m(Machine::SP, m.M[Machine::SP] - 2);
    m.xta(Machine::SP, 0);
    m.rmr = m.acc;
    if (m.acc) {
        m.mem[DLIST_TOP] = m.acc;
        m.acc            = m.mem[BLOCK];
        m.set_m(14, m.M[1]);
        m.call(EMIT_SHIFTED, 13, 017525);
    }
    m.set_m(12, 017526);
    close_scope(m);
    m.xta(Machine::SP, 0);
    m.mem[BLOCK] = m.acc;
    m.set_m(13, AFTER_STMT);
    return MAIN_LOOP;
}

// *0402B: read the next item and return from the subtree.
unsigned next_and_return(Machine &m)
{
    m.set_m(13, RETURN_SUB);
    return READ;
}

//
// *0377B: write the item A<<12 ^ M14. An operand alone as a statement (the
// subtree called from the statement loop) is an illegal use.
//
unsigned emit_operand(Machine &m)
{
    m.call(EMIT_SHIFTED, 13, 017623);
    m.rmr = m.mem[(m.M[Machine::SP] - 2) & 077777];
    m.acc = m.rmr ^ AFTER_END;
    m.rmr = m.acc;
    m.set_logical();
    if (!m.acc)
        return illegal_use(m);
    return next_and_return(m);
}

//
// *0416B: a procedure without parameters used as an operand: with parameters
// it is an illegal use (returns through M8); else bit 39 of its descriptor
// is flipped unless bit 48 is set. Returns through M13.
//
unsigned proc_operand(Machine &m)
{
    m.acc = m.mem[m.M[4]] & PARAMS;
    m.rmr = 0;
    m.set_logical();
    if (m.acc)
        return illegal_use(m);
    m.acc = m.mem[m.M[4]];
    m.rau = 013;
    m.rmr = m.acc;
    if (m.acc & BIT48)
        return m.M[13];
    m.acc ^= BIT39;
    m.set_logical();
    m.mem[m.M[4]] = m.acc;
    return m.M[13];
}

//
// Code 000, an identifier: looked up, then by its kind (M6) the item for its
// value (017), a call (021, 022, 0107), a label (024: bit 48 of the
// descriptor marks it used outside the procedure at DLIST_TOP); arrays and
// procedures with parameters are illegal here.
//
unsigned identifier(Machine &m)
{
    m.set_m(13, 017606);
    lookup(m);
    if (!m.M[4])
        return next_and_return(m);
    m.set_m(8, NEXT_RETURN);
    bool label = false;
    switch (m.M[6]) {
    case 0:
    case 5:
        if (m.M[5]) {
            m.set_m(14, m.M[6] ? 022 : 017);
            m.acc = m.M[4];
            return emit_operand(m);
        }
        label = true;
        break;
    case 1:
    case 2:
        return illegal_use(m);
    case 3:
    case 4:
    case 6:
    case 7:
        if (m.M[6] == 4 || m.M[6] == 6) {
            unsigned ret = m.M[6] == 4 ? 017634 : 017617;
            m.set_m(13, ret);
            unsigned pc = proc_operand(m);
            if (pc != ret)
                return pc;
        }
        if (m.M[6] <= 4) {
            m.set_m(14, 021);
            m.acc = m.mem[m.M[4]];
            m.rau = 013;
            m.rmr = m.acc;
            if (m.acc & BIT48)
                m.set_m(14, 0107);
        } else {
            m.set_m(14, 022);
        }
        m.set_m(13, NEXT_RETURN);
        return EMIT_M4;
    default:
        return 017610 + m.M[6];
    }
    if (label) {
        m.acc = m.mem[DLIST_TOP];
        m.set_m(13, 017627);
        inside(m);
        m.rmr = m.acc;
        if (!m.acc) {
            m.acc         = m.mem[m.M[4]] | BIT48;
            m.mem[m.M[4]] = m.acc;
        }
        m.set_m(14, 024);
        m.acc = m.M[4];
    }
    return emit_operand(m);
}

// *0550B: a constant of type M5 in slot M3: item 023.
unsigned constant(Machine &m)
{
    m.set_m(14, 023);
    m.acc = m.M[3];
    m.set_logical();
    return emit_operand(m);
}

// Codes 076, 077: TRUE, FALSE are the constants of slots 01625, 01624.
unsigned true_item(Machine &m)
{
    m.set_m(3, 01625);
    m.set_m(5, 3);
    return constant(m);
}

unsigned false_item(Machine &m)
{
    m.set_m(3, 01624);
    m.set_m(5, 3);
    return constant(m);
}

// Codes 023, 024, 0105: real, integer, octal constants.
unsigned real_item(Machine &m)
{
    m.set_m(5, 1);
    return constant(m);
}

unsigned integer_item(Machine &m)
{
    m.set_m(5, 2);
    return constant(m);
}

unsigned octal_item(Machine &m)
{
    m.set_m(5, 5);
    return constant(m);
}

// Code 025: a string, item 070.
unsigned string_item(Machine &m)
{
    m.set_m(5, 4);
    m.set_m(14, 070);
    m.acc = m.M[3];
    m.set_logical();
    return emit_operand(m);
}

//
// *0430B: TYPE ERROR IN CONNECTION WITH the text at ARGS, unless reported in
// this subtree. Returns through M8.
//
unsigned type_error_args(Machine &m)
{
    m.set_m(13, TYPE_ERROR);
    m.acc = m.mem[REPORTED];
    m.rmr = m.acc;
    m.set_logical();
    if (m.acc)
        return m.M[8];
    return message(m);
}

// *0427B: the same with the text in A; FLGEX* gets it too.
unsigned type_error(Machine &m)
{
    m.mem[ARGS]  = m.acc;
    m.mem[FLGEX] = m.acc;
    return type_error_args(m);
}

//
// *0440B: how types M5 and A combine (TYPE_PAIRS): M14 = 0 the same, 1 or 2
// an integer and a real, 3 an error. Returns through M13.
//
unsigned type_pair(Machine &m)
{
    m.set_m(14, unsigned(m.acc));
    m.acc = m.mem[(TYPE_PAIRS + m.M[5]) & 077777];
    m.asx(14, TYPE_PAIRS);
    m.acc &= 7;
    m.rmr = 0;
    m.set_m(14, unsigned(m.acc));
    return m.M[13];
}

//
// *0665B: the left operand of + - * / (the right one's type pushed, over the
// operator's word). Both of type 0 or not alike is a TYPE ERROR; with an
// integer and a real (/ on two integers too) the left one gets 064 and the
// result is real; strings only add. The item: the operator's code for the
// type.
//
unsigned arith_left(Machine &m)
{
    m.push(m.M[5]);
    m.call(PARSE_READ, 12, 020112);
    m.xta(Machine::SP, 0);
    m.set_m(13, 020113);
    type_pair(m);
    unsigned left = m.M[5];
    auto op_code  = [&](int shift, unsigned type) {
        m.xta(Machine::SP, 0);
        m.set_m(5, type);
        m.acc = (m.acc >> shift) & 077;
        m.rmr = 0;
        m.set_logical();
        m.set_m(13, RETURN_SUB);
        return EMIT_A;
    };
    auto error = [&] {
        m.wtc(Machine::SP, 0);
        m.xta(0, 0);
        m.set_m(8, RETURN_SUB);
        return type_error(m);
    };
    switch (m.M[14]) {
    case 0:
        if (!left)
            return error();
        if (left == 1)
            break;
        m.set_m(5, left - 2);
        if (left == 2)
            return op_code(30, 2);
        m.set_m(5, left - 4);
        if (left != 4 || m.mem[(m.M[Machine::SP] - 1) & 077777] != m.mem[OPS])
            return error();
        m.acc = m.mem[STRING_PLUS];
        m.set_m(13, RETURN_SUB);
        m.set_m(5, 4);
        m.set_m(Machine::SP, m.M[Machine::SP] - 1);
        return EMIT_A;
    case 1:
        m.acc = m.mem[(m.M[Machine::SP] - 1) & 077777] ^ m.mem[OPS + 3];
        if (!m.acc) {
            m.acc = m.M[5] ^ 2;
            if (!m.acc) {
                m.set_m(14, 064);
                m.call(EMIT_M14, 13, 020126);
            }
        }
        break;
    case 2:
        break;
    case 3:
        return error();
    default:
        return 020114 + m.M[14];
    }
    return op_code(24, 1);
}

// Codes 013-015: + - *.
unsigned arith_op(Machine &m, unsigned op)
{
    m.acc = m.mem[op];
    m.push(m.acc);
    m.call(PARSE, 12, 020110);
    return arith_left(m);
}

unsigned plus_item(Machine &m)
{
    return arith_op(m, OPS);
}

unsigned minus_item(Machine &m)
{
    return arith_op(m, OPS + 1);
}

unsigned times_item(Machine &m)
{
    return arith_op(m, OPS + 2);
}

// Code 016, /: an integer right operand gets 064.
unsigned divide_item(Machine &m)
{
    m.acc = m.mem[OPS + 3];
    m.push(m.acc);
    m.call(PARSE, 12, 020156);
    m.acc = m.M[5] ^ 2;
    m.set_logical();
    if (m.acc)
        return arith_left(m);
    m.set_m(14, 064);
    m.set_m(5, 1);
    m.call(EMIT_M14, 13, 020110);
    return arith_left(m);
}

//
// *0644B: a subscript of type M5: a real one gets 063 (to integer), an
// integer one is fine, else TYPE ERROR INDEX (through M8 = M13). Returns
// through M13.
//
unsigned index_type(Machine &m)
{
    switch (m.M[5]) {
    case 1:
        m.acc = 063;
        m.call(EMIT_A, 13, m.M[13]);
        return m.M[13];
    case 2:
        return m.M[13];
    case 0:
    case 3:
    case 4:
    case 5:
        m.set_m(8, m.M[13]);
        m.acc = m.mem[INDEX_TEXT];
        m.set_logical();
        return type_error(m);
    default:
        return 020070 + m.M[5];
    }
}

//
// *0656B: A = the dimensions of the array M4; another number of subscripts
// (at [M15-1]) is an illegal use, through M8 = M13. Returns through M13.
//
unsigned subscript_count(Machine &m)
{
    m.rmr = m.acc;
    m.acc ^= m.mem[(m.M[Machine::SP] - 1) & 077777];
    m.set_logical();
    if (!m.acc)
        return m.M[13];
    m.set_m(8, m.M[13]);
    return illegal_use(m);
}

// *0654B: the dimensions in bits 19-15; a formal array without them gets them.
unsigned array_dims(Machine &m)
{
    m.acc = (m.mem[m.M[4]] >> 15) & 037;
    m.rmr = 0;
    m.set_logical();
    if (m.acc)
        return subscript_count(m);
    m.acc         = (m.mem[(m.M[Machine::SP] - 1) & 077777] << 15) & WORD_MASK;
    m.acc         = m.mem[m.M[4]] ^ m.acc;
    m.mem[m.M[4]] = m.acc;
    return m.M[13];
}

//
// *0560B: an array element or a switch designator: slot M3 with M2
// subscripts (M12: 044, 046, 025 for a switch). The item a<<12 | n<<7 | M12
// goes out first, then each subscript (NEXT_SUB).
//
unsigned subscripted(Machine &m)
{
    m.push(m.M[2]);
    m.set_m(13, 020005);
    lookup(m);
    if (m.M[4]) {
        m.set_m(8, 020012);
        m.set_m(6, m.M[6] - 1);
        if (m.M[6])
            return 020565;
        if (m.M[5]) {
            m.set_m(13, 020012);
            array_dims(m);
        } else {
            m.set_m(12, 025);
        }
    }
    m.push(m.M[5]);
    Word count = m.mem[(m.M[Machine::SP] - 2) & 077777];
    m.acc      = (Word(m.M[4]) << 12) | ((count << 7) & WORD_MASK) | m.M[12];
    m.rmr      = 0;
    m.set_logical();
    m.call(EMIT_A, 13, 020017);
    m.call(READ, 13, NEXT_SUB);
    return NEXT_SUB;
}

// Code 021: an array element.
unsigned element_item(Machine &m)
{
    m.set_m(12, 044);
    return subscripted(m);
}

// *0554B: a switch designator in a left part: as if called from *0514B.
unsigned left_designator(Machine &m)
{
    m.set_m(12, 046);
    m.set_m(13, 017737);
    m.push(m.M[13]);
    m.acc = 0;
    m.push(0);
    return subscripted(m);
}

//
// *0612B: the left operand of i+c or i-c, c pushed as c<<12 (030 with c
// comes out at INDEX_DONE).
//
unsigned index_offset(Machine &m, Word c)
{
    m.acc = (c << 12) & WORD_MASK;
    m.push(m.acc);
    m.set_m(12, INDEX_DONE);
    return PARSE;
}

//
// A general subscript i+e or i-e: as the operator's handler would, in a
// frame returning to INDEX_DONE with 0 pushed.
//
unsigned index_general(Machine &m, unsigned op)
{
    m.push(0);
    m.push(INDEX_DONE);
    m.push(0);
    m.acc = m.mem[op];
    m.push(m.acc);
    m.set_m(13, INDEX_DONE);
    m.set_m(14, op);
    m.set_m(12, ARITH_LEFT);
    return PARSE_READ;
}

// A small integer constant in slot M3: below the limit at M7+k.
bool small_constant(Machine &m, unsigned limit)
{
    m.acc = m.mem[NAMES + m.M[3]];
    m.arith(7, limit, 005);
    return !m.uza();
}

//
// *0575B: the next subscript (read backwards), by how it ends: a constant
// below 2048 is an item 045 c; i+c, i-c with c below 2048, 64 are 030 c
// after i; anything else is the expression, then 030 0.
//
unsigned next_subscript(Machine &m)
{
    m.acc = m.mem[TYPES + m.M[1]];
    m.set_m(14, unsigned(m.acc >> 7) & 3);
    m.acc = m.M[14];
    m.rmr = 0;
    m.set_logical();
    switch (m.M[14]) {
    case 1:
        if (small_constant(m, 02720)) {
            m.acc = m.mem[NAMES + m.M[3]] & 07777;
            m.set_logical();
            m.set_m(14, 045);
            m.call(EMIT_SHIFTED, 13, 020032);
            m.set_m(13, 020044);
            return READ;
        }
        [[fallthrough]];
    case 0:
        m.acc = 0;
        m.push(0);
        m.set_m(12, INDEX_DONE);
        return PARSE_READ;
    case 2:
        m.call(READ, 13, 020033);
        m.acc = m.M[1] ^ 024;
        if (m.acc || !small_constant(m, 02720))
            return index_general(m, OPS);
        m.acc = m.mem[NAMES + m.M[3]] & 07777;
        return index_offset(m, m.acc);
    case 3:
        m.call(READ, 13, 020053);
        m.acc = m.M[1] ^ 024;
        if (m.acc || !small_constant(m, 02713))
            return index_general(m, OPS + 1);
        m.acc = m.mem[NAMES + m.M[3]];
        m.rau = 003;
        m.arith(7, 02731, 006);
        m.acc &= 07777;
        return index_offset(m, m.acc);
    }
    return 0;
}

// *0621B: one subscript less; after the last, M5 back and return.
unsigned subscript_done(Machine &m)
{
    unsigned at = (m.M[Machine::SP] - 2) & 077777;
    m.acc       = m.mem[at];
    m.rau       = 003;
    m.arith(7, 02646, 005);
    m.mem[at] = m.acc;
    m.rau     = 007;
    if (m.acc)
        return NEXT_SUB;
    m.xta(Machine::SP, 0);
    m.sti(0, 5);
    return RETURN_SUB;
}

// *0616B: after a subscript: its type, then 030 with the offset pushed.
unsigned index_done(Machine &m)
{
    m.set_m(13, 020042);
    unsigned pc = index_type(m);
    if (pc != 020042)
        return pc;
    m.set_m(14, 030);
    m.acc = 030;
    m.acc ^= m.mem[(m.M[Machine::SP] - 1) & 077777];
    m.set_m(Machine::SP, m.M[Machine::SP] - 1);
    m.call(EMIT_A, 13, 020044);
    return subscript_done(m);
}

//
// Code 017, **: unary + and - before the exponent are dropped and counted.
// An integer constant exponent 1..63: the item c<<12 ^ 0152 (real base),
// 0153 (integer), negative as 1 / x**c; on an octal base 0352 with 64+c or
// 64-c. Otherwise by the types: 0147 real**real, 0150 real**int, 0151
// int**int constant, int**int as 0150 after 064, 0350, 0347 for octal.
//
unsigned power_item(Machine &m)
{
    auto at = [&](unsigned k) -> Word & { return m.mem[(m.M[Machine::SP] - k) & 077777]; };
    m.acc   = 0;
    m.push(0); // [M15-2]: the constant exponent
    m.push(0); // [M15-1]: its sign
    for (;;) {
        m.call(READ, 13, 020224);
        m.set_m(14, m.M[1] - 024);
        if (m.M[1] == 046)
            continue;
        if (m.M[1] != 047)
            break;
        m.acc = at(1) ^ 1;
        at(1) = m.acc;
    }
    auto finish = [&] {
        m.set_m(Machine::SP, m.M[Machine::SP] - 2);
        m.set_m(13, RETURN_SUB);
        return EMIT_M14;
    };
    auto error = [&] {
        m.acc = m.mem[POWER_TEXT];
        m.set_m(8, 020254);
        type_error(m);
        return finish();
    };
    if (m.M[1] == 024) {
        m.acc = m.mem[NAMES + m.M[3]];
        at(2) = m.acc;
        m.set_m(14, unsigned(m.acc));
        bool small = false;
        if (m.M[14]) {
            m.arith(7, 02713, 005);
            small = !m.uza();
        }
        if (small) {
            m.call(PARSE, 12, 020270);
            m.set_m(14, m.M[5] == 2 ? 0153 : 0152);
            if (m.M[5] == 5) {
                m.rau         = 003;
                m.acc         = at(1);
                bool negative = m.acc;
                m.acc         = at(2);
                m.arith(7, 02713, negative ? 006 : 004);
                m.set_m(14, 0352);
                m.call(EMIT_SHIFTED, 13, 020300);
                m.set_m(Machine::SP, m.M[Machine::SP] - 2);
                return RETURN_SUB;
            }
            if (m.M[5] != 1 && m.M[5] != 2)
                return error();
            m.acc = at(1);
            if (m.acc) {
                bool integer = m.M[5] == 2;
                m.set_m(5, m.M[5] - 2);
                if (integer) {
                    m.set_m(14, 064);
                    m.call(EMIT_M14, 13, 020306);
                }
                m.set_m(5, 1);
                m.set_m(4, ONE);
                m.set_m(14, 023);
                m.call(EMIT_M4, 13, 020310);
                m.set_m(14, 064);
                m.call(EMIT_M14, 13, 020311);
                m.set_m(14, 016);
                m.call(EMIT_M14, 13, 020312);
                m.set_m(14, 0152);
            }
            m.acc = at(2);
            m.call(EMIT_SHIFTED, 13, 020314);
            m.set_m(Machine::SP, m.M[Machine::SP] - 2);
            return RETURN_SUB;
        }
    }
    m.call(PARSE_READ, 12, 020231);
    m.push(m.M[5]);
    m.acc = at(2);
    if (m.acc) {
        m.set_m(5, m.M[5] - 1);
        m.set_m(14, m.M[5] ? 026 : 047);
        m.call(EMIT_M14, 13, 020236);
    }
    m.call(PARSE_READ, 12, 020237);
    m.acc = (Word(m.M[5]) << 3) | at(1);
    m.set_m(Machine::SP, m.M[Machine::SP] - 1);
    m.set_m(13, unsigned(m.acc));
    switch (m.acc) {
    case 011:
        m.set_m(14, 0147);
        break;
    case 012:
        m.set_m(14, 0150);
        break;
    case 052:
        m.set_m(14, 0350);
        break;
    case 051:
        m.set_m(14, 0347);
        break;
    case 022:
        m.set_m(14, 0151);
        m.acc = at(2);
        if (!m.acc) {
            m.set_m(14, 064);
            m.call(EMIT_M14, 13, 020253);
            m.set_m(14, 0150);
            m.set_m(5, 1);
        }
        break;
    case 021:
        m.set_m(14, 064);
        m.call(EMIT_M14, 13, 020262);
        m.set_m(14, 0147);
        m.set_m(5, 1);
        break;
    default:
        return error();
    }
    return finish();
}

// *1121B: TYPE ERROR IN CONNECTION WITH BOOLEAN OP. (the code popped).
unsigned bool_error(Machine &m)
{
    m.set_m(8, RETURN_SUB);
    m.acc = m.mem[BOOL_TEXT];
    m.stx(0, ARGS);
    m.mem[ARGS + 1] = m.mem[BOOL_TEXT + 1];
    m.acc           = m.mem[ARGS + 1];
    return type_error_args(m);
}

//
// *1110B: a boolean operator (its code pushed) on operands of type M5:
// boolean as is; octal OR as is, AND and NOT as 0200 + the code; else TYPE
// ERROR.
//
unsigned bool_result(Machine &m)
{
    m.set_m(13, RETURN_SUB);
    m.set_m(14, m.M[5]);
    m.xta(Machine::SP, 0);
    if (m.M[5] == 3)
        return EMIT_A;
    if (m.M[5] == 5) {
        unsigned code = m.acc & 077777;
        if (code == 2)
            return EMIT_A;
        m.acc |= 0200;
        if (code == 3 || code == 4)
            return EMIT_A;
    }
    m.push(m.acc);
    return bool_error(m);
}

// Codes 001-003, 043: IMPL OR AND EQUIV.
unsigned bool_item(Machine &m)
{
    m.push(m.M[1]);
    m.call(PARSE, 12, 020327);
    m.push(m.M[5]);
    m.call(PARSE_READ, 12, 020331);
    m.xta(Machine::SP, 0);
    m.set_m(13, 020332);
    type_pair(m);
    if (m.M[14])
        return bool_error(m);
    return bool_result(m);
}

// Code 004: NOT.
unsigned not_item(Machine &m)
{
    m.push(m.M[1]);
    m.call(PARSE, 12, 020333);
    return bool_result(m);
}

//
// Codes 005-012: relations. Operands of one type, real, integer or string;
// octal ones only for = and ≠; an integer and a real: = ≠ as 0105, 0106.
// Strings: the item (code-4)<<12 ^ 0157. The result is boolean.
//
unsigned relation_item(Machine &m)
{
    m.push(m.M[1]);
    m.call(PARSE, 12, 020356);
    m.push(m.M[5]);
    bool ok = false;
    switch (m.M[5]) {
    case 1:
    case 2:
    case 4:
        ok = true;
        break;
    case 0:
    case 3:
        break;
    case 5: {
        unsigned code = m.mem[(m.M[Machine::SP] - 2) & 077777] & 077777;
        m.set_m(14, code);
        ok = code == 011 || code == 012;
        break;
    }
    default:
        return 020360 + m.M[5];
    }
    bool error = !ok;
    m.call(PARSE_READ, 12, ok ? 020372 : 020413);
    if (ok) {
        m.xta(Machine::SP, 0);
        m.set_m(13, 020373);
        type_pair(m);
        switch (m.M[14]) {
        case 0:
            m.xta(Machine::SP, 0);
            break;
        case 1:
        case 2:
            m.wtc(Machine::SP, 0);
            m.xta(0, REL_MIXED);
            break;
        case 3:
            m.xta(Machine::SP, 0);
            error = true;
            break;
        default:
            return 020374 + m.M[14];
        }
    } else {
        m.set_m(Machine::SP, m.M[Machine::SP] - 2);
    }
    if (error) {
        m.mem[ARGS + 1] = m.mem[REL_TEXT + 1];
        m.acc           = m.mem[REL_TEXT];
        m.set_m(8, 020402);
        type_error(m);
    }
    m.set_m(5, m.M[5] - 4);
    if (!m.M[5]) {
        m.arx(7, 02706);
        m.set_m(14, 0157);
        m.set_m(5, 3);
        m.set_m(13, RETURN_SUB);
        return EMIT_SHIFTED;
    }
    m.set_m(5, 3);
    m.set_m(13, RETURN_SUB);
    return EMIT_A;
}

// Code 046: unary +, on a number only.
unsigned uplus_item(Machine &m)
{
    m.call(PARSE, 12, 020425);
    m.acc = m.mem[NUMERIC + m.M[5]];
    m.set_logical();
    if (m.acc)
        return RETURN_SUB;
    m.acc = m.mem[OPS_TEXT];
    m.set_m(8, RETURN_SUB);
    return type_error(m);
}

// Code 047: unary -, on a number: 047 real, 026 integer.
unsigned uminus_item(Machine &m)
{
    m.call(PARSE, 12, 020431);
    m.acc = m.mem[NUMERIC + m.M[5]];
    m.set_logical();
    if (!m.acc) {
        m.acc = m.mem[OPS_TEXT + 1];
        m.set_m(8, RETURN_SUB);
        return type_error(m);
    }
    m.set_m(14, 047);
    switch (m.M[5]) {
    case 1:
        break;
    case 2:
        m.set_m(14, 026);
        break;
    default:
        return 020434 + m.M[5];
    }
    m.set_m(13, RETURN_SUB);
    return EMIT_M14;
}

// *0513B: read the next left part.
unsigned next_left(Machine &m)
{
    m.call(READ, 13, LEFT_COUNT);
    return LEFT_COUNT;
}

//
// *0514B: one left part less (the count is one less than their number);
// after the last, return from the statement.
//
unsigned left_count(Machine &m)
{
    unsigned at = (m.M[Machine::SP] - 3) & 077777;
    m.acc       = m.mem[at];
    m.arx(7, 02717);
    m.mem[at] = m.acc;
    if (m.uza()) {
        m.set_m(Machine::SP, m.M[Machine::SP] - 5);
        return m.mem[m.M[Machine::SP]] & 077777;
    }
    m.acc                                  = 0;
    m.mem[(m.M[Machine::SP] - 1) & 077777] = 0;
    m.set_logical();
    return LEFT_PART;
}

// *0512B: write the left part M4 with the code M14.
unsigned emit_left(Machine &m)
{
    m.call(EMIT_M4, 13, NEXT_LEFT);
    return next_left(m);
}

//
// *0475B: after its type check, a left part by its kind: a variable (0100),
// a formal by name (0101, marked assigned; illegal if not assignable), the
// name of a function inside its body (0100 with its word after it, which is
// marked); an element or a switch designator goes to *0554B.
//
unsigned after_left(Machine &m)
{
    if (m.M[1])
        return 017777;
    if (!m.M[4])
        return next_left(m);
    switch (m.M[6]) {
    case 0:
        m.set_m(14, 0100);
        return emit_left(m);
    case 2:
    case 3:
        m.acc                        = m.mem[(m.M[4] + 1) & 077777] | RESULT_SET;
        m.mem[(m.M[4] + 1) & 077777] = m.acc;
        m.rau                        = 003;
        m.set_m(8, NEXT_LEFT);
        m.acc = m.M[4];
        m.arith(0, BLOCK, 005);
        if (m.uza())
            return illegal_use(m);
        m.acc = m.mem[(m.M[4] + 1) & 077777] & 077777;
        m.arith(0, BLOCK, 006);
        if (m.uza())
            return illegal_use(m);
        m.set_m(4, m.M[4] + 1);
        m.set_m(14, 0100);
        return emit_left(m);
    case 5:
        m.acc         = m.mem[m.M[4]] | ASSIGNED;
        m.mem[m.M[4]] = m.acc;
        m.acc &= NOT_ASSIGNABLE;
        m.set_logical();
        m.set_m(8, NEXT_LEFT);
        if (m.acc)
            return illegal_use(m);
        m.set_m(14, 0101);
        return emit_left(m);
    case 1:
    case 4:
    case 6:
    case 7:
        m.set_m(8, NEXT_LEFT);
        return illegal_use(m);
    default:
        return 017722 + m.M[6];
    }
}

//
// *0457B: a left part, against the type at [M15-2] (the right side's, then
// the previous left part's): 063 or 064 when integer and real, TYPE ERROR
// ':=' when [M15-1] (the right side's type, 0 after the first left part) is
// 0, ILLEGAL USE when not alike.
//
unsigned left_part(Machine &m)
{
    auto at = [&](unsigned k) -> Word & { return m.mem[(m.M[Machine::SP] - k) & 077777]; };
    m.acc   = m.mem[TYPES + m.M[1]] & VARIABLE;
    m.rmr   = 0;
    m.set_logical();
    if (!m.acc) {
        m.call(PARSE_READ, 12, 017755);
        m.set_m(13, BAD_LEFT);
        m.set_m(8, LEFT_COUNT);
        return message(m);
    }
    m.set_m(13, 017705);
    lookup(m);
    if (!m.M[4])
        return after_left(m);
    m.acc = at(2);
    m.set_m(13, 017707);
    type_pair(m);
    switch (m.M[14]) {
    case 0:
        return after_left(m);
    case 1:
        m.set_m(14, 063);
        break;
    case 2:
        m.set_m(14, 064);
        break;
    case 3:
        m.set_m(8, AFTER_LEFT);
        return illegal_use(m);
    default:
        return 017707 + m.M[14];
    }
    m.acc = m.M[14];
    m.call(EMIT_A, 13, 017714);
    m.acc = m.M[5];
    at(2) = m.acc;
    m.set_m(8, AFTER_LEFT);
    m.acc = at(1);
    if (m.acc)
        return after_left(m);
    m.acc = m.mem[ASSIGN_TEXT];
    return type_error(m);
}

// Code 022, := with n+1 left parts: the right side first.
unsigned assign_item(Machine &m)
{
    m.push(m.M[2]);
    m.call(PARSE, 12, 017677);
    m.push(m.M[5]);
    m.push(m.M[5]);
    m.acc = m.mem[ASSIGN_TEXT];
    m.set_m(8, LEFT_PART);
    if (!m.M[5])
        return type_error(m);
    return left_part(m);
}

// Code 026, GOTO: the designational expression after it must be a label.
unsigned goto_item(Machine &m)
{
    m.call(PARSE, 12, 017647);
    if (!m.M[5])
        return DISPATCH;
    m.acc = m.mem[REPORTED];
    m.rmr = m.acc;
    m.set_logical();
    if (m.acc)
        return DISPATCH;
    m.set_m(8, DISPATCH);
    m.acc = m.mem[GOTO_TEXT];
    return type_error(m);
}

//
// *0312B, at a statement marker: the position text "line/ s" from M3 and M2,
// then the marker is written. Goes on at the main loop.
//
unsigned set_position(Machine &m)
{
    m.acc = m.M[3];
    m.call(ITOA, 13, 017536);
    m.asn(0, 64 - 16);
    m.mem[LINE_TEXT] = m.acc;
    m.acc            = m.M[2];
    m.call(ITOA, 13, 017540);
    m.asn(0, 64 - 32);
    m.mem[STMT_TEXT] = m.acc;
    m.call(EMIT_MARKER, 13, 017542);
    m.set_m(13, AFTER_STMT);
    return MAIN_LOOP;
}

//
// G17223: start of pass 2. Read the stream from its end, check the 'COMMON'
// elements against their declarations and mark them in the D-list, save the
// names to drum 26 zone 0, clear the identifier slots, open the outer scope
// and emit EOP. Goes on at the main loop.
//
unsigned start(Machine &m)
{
    m.set_m(5, 0);
    m.set_m(7, 017223);
    m.push(m.M[13]); // the return into the driver
    m.mem[READ_CURSOR] = cyclic_add(m.mem[WRITE_CURSOR], 0200000);
    m.call(READ_BACK, 13, 017227);
    m.mem[WRITE_CURSOR] = m.mem[022113];
    m.mem[DLIST_TOP2]   = m.M[3];
    m.mem[DLIST_TOP]    = m.M[3];
    m.mem[BLOCK_DLIST]  = Word(m.M[3]) << 12;

    // Each element: its slot holds its name, and the declaration at D3 is its own.
    for (m.set_m(8, 0); m.mem[COMMON + m.M[8] + 1]; m.set_m(8, m.M[8] + 3)) {
        Word name = m.mem[COMMON + m.M[8]];
        Word info = m.mem[COMMON + m.M[8] + 1];
        if (!slot_of(info))
            continue; // the block
        m.set_m(13, slot_of(info));
        if (m.mem[NAMES + m.M[13]] != name) {
            common_message(m, NOT_DECLARED, name);
            continue;
        }
        m.set_m(13, unsigned(info >> 36));
        m.mem[SCRATCH] = m.mem[m.M[13]] & 07777;
        if (m.mem[SCRATCH] != slot_of(info))
            common_message(m, WRONG_ORDER, name);
    }

    // Mark the declarations of the elements.
    for (m.set_m(8, DLIST_END - 1); m.mem[m.M[8]]; m.set_m(8, m.M[8] - 1)) {
        Word &d = m.mem[m.M[8]];
        if (d & HEADER)
            continue;
        m.mem[SCRATCH] = d & 077777;
        for (m.set_m(13, 0); m.mem[COMMON + 1 + m.M[13]]; m.set_m(13, m.M[13] + 3)) {
            if (slot_of(m.mem[COMMON + 1 + m.M[13]]) != m.mem[SCRATCH])
                continue;
            d = (d & IN_COMMON_CLR) | IN_COMMON;
            if (!(d & DIMENSIONS))
                d |= COMMON_SIMPLE;
            break;
        }
    }
    m.acc = 0;

    m.mem[FREE_SAVED] = m.mem[FREE_CONST];
    m.set_m(14, ARRAYS);
    m.mem[ARRAYS_END] = ARRAYS;
    m.extracode(070, 0, SAVE_NAMES);
    for (m.set_m(14, unsigned(m.mem[FREE_CONST])); m.M[14];) {
        m.set_m(14, m.M[14] - 1);
        m.mem[NAMES + m.M[14]] = 0;
    }
    m.acc = 0;
    m.set_m(11, m.M[3]);
    m.set_m(14, DLIST_END);
    m.acc         = DLIST_END;
    m.mem[m.M[3]] = DLIST_END;
    m.call(OPEN_SCOPE, 12, 017345);
    for (unsigned i = 0; i < 6; i++)
        m.mem[POSITION + i] = m.mem[POSITION_SRC + i];
    m.set_m(12, 0);
    m.set_m(14, 075);
    m.call(EMIT_M14, 13, 017351);
    m.set_m(13, NEXT_ITEM);
    return MAIN_LOOP;
}

//
// *2604B, at the first item: link the descriptors of each array record to
// it, print INCORRECT ALGOL PROGRAM after errors, return into the driver.
//
unsigned finish(Machine &m)
{
    m.set_m(Machine::SP, m.M[Machine::SP] - 2);
    for (m.set_m(8, ARRAYS); m.M[8] != m.mem[ARRAYS_END]; m.set_m(8, m.M[8] + 1)) {
        Word rec = m.mem[m.M[8]];
        m.set_m(2, unsigned(rec >> 7) & 037);
        m.set_m(4, unsigned(rec >> 12) & 07777);
        Word link = Word(m.M[8]) << 20;
        do {
            m.mem[m.M[4]] = (m.mem[m.M[4]] & RECORD_CLR) | link | HEADER;
            m.set_m(2, m.M[2] - 1);
            m.set_m(4, m.M[4] + 1);
        } while (m.M[2]);
    }
    m.acc = m.mem[FLGEX];
    if (m.acc) {
        m.push(m.acc);
        m.push(INCORRECT);
        m.set_m(14, INCORRECT + 5);
        m.acc = INCORRECT + 5;
        m.call(FORMAT, 13, 022050);
    }
    m.set_m(Machine::SP, m.M[Machine::SP] - 1);
    return m.mem[m.M[Machine::SP]] & 077777;
}

} // namespace

Stream pass2(Context &ctx, const Stream &input)
{
    TranslatedPass pass(ctx, pass2_code);
    pass.m.natives[017223] = start;
    pass.m.natives[022027] = finish;
    pass.m.natives[017420] = message_name;
    pass.m.natives[017426] = message;
    pass.m.natives[017430] = after_message;
    pass.m.natives[017535] = set_position;
    pass.m.natives[017371] = inside;
    pass.m.natives[017377] = lookup;
    pass.m.natives[017434] = illegal_use;
    pass.m.natives[017454] = open_scope_at;
    pass.m.natives[017455] = open_scope;
    pass.m.natives[017475] = close_scope;
    pass.m.natives[017511] = end_item;
    pass.m.natives[017523] = begin_item;
    pass.m.natives[017605] = identifier;
    pass.m.natives[017646] = goto_item;
    pass.m.natives[017652] = type_error;
    pass.m.natives[017653] = type_error_args;
    pass.m.natives[017663] = type_pair;
    pass.m.natives[017765] = true_item;
    pass.m.natives[017766] = false_item;
    pass.m.natives[017770] = real_item;
    pass.m.natives[017771] = octal_item;
    pass.m.natives[017772] = integer_item;
    pass.m.natives[017775] = string_item;
    pass.m.natives[020106] = plus_item;
    pass.m.natives[020152] = minus_item;
    pass.m.natives[020153] = times_item;
    pass.m.natives[020154] = divide_item;
    pass.m.natives[017777] = left_designator;
    pass.m.natives[020002] = element_item;
    pass.m.natives[020003] = subscripted;
    pass.m.natives[020020] = next_subscript;
    pass.m.natives[020041] = index_done;
    pass.m.natives[020044] = subscript_done;
    pass.m.natives[020067] = index_type;
    pass.m.natives[020101] = subscript_count;
    pass.m.natives[020110] = arith_left;
    pass.m.natives[020221] = power_item;
    pass.m.natives[020325] = bool_item;
    pass.m.natives[020350] = not_item;
    pass.m.natives[020354] = relation_item;
    pass.m.natives[020424] = uplus_item;
    pass.m.natives[020430] = uminus_item;
    pass.m.natives[017675] = assign_item;
    pass.m.natives[017702] = left_part;
    pass.m.natives[017720] = after_left;
    pass.m.natives[017736] = next_left;
    pass.m.natives[017737] = left_count;
    // Pass 1 writes forward through 27126 from the cursor at 15403.
    pass.write_stream(input, 027126, pass.m.mem[015403]);
    return pass.run(017223, 01627);
}
