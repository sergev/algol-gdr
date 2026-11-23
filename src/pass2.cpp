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
constexpr unsigned NOT_DECLARED = 017311; // text, name at +010
constexpr unsigned WRONG_ORDER  = 017322; // text, name at +010
constexpr unsigned INCORRECT    = 022051; // text up to 022056
constexpr unsigned SCRATCH      = 017333; // *0110B
constexpr unsigned POSITION_SRC = 021670;

constexpr Word HEADER        = 0'4000'0000'0000'0000; // D-list: a block header
constexpr Word DESCRIPTOR    = 0'2000'0000'0000'0000;
constexpr Word BIT41         = 0'0020'0000'0000'0000;
constexpr Word BIT48         = 0'4000'0000'0000'0000;
constexpr Word IN_COMMON_CLR = 0'7770'0777'7777'7777;
constexpr Word IN_COMMON     = 0'0001'0000'0000'0000;
constexpr Word DIMENSIONS    = 0'0000'0000'0070'0000;
constexpr Word COMMON_SIMPLE = 0'0000'0000'0010'0000;
constexpr Word RECORD_CLR    = 0'7777'7400'0377'7777; // descriptor without bits 33-21: the
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
    // Pass 1 writes forward through 27126 from the cursor at 15403.
    pass.write_stream(input, 027126, pass.m.mem[015403]);
    return pass.run(017223, 01627);
}
