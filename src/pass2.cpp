#include "pass2.h"

#include "translated.h"

unsigned pass2_code(Machine &m, unsigned pc);

namespace {

// Cells of the workspace.
constexpr unsigned READ_CURSOR  = 02000;
constexpr unsigned WRITE_CURSOR = 02001;
constexpr unsigned FREE_CONST   = 02003; // the lowest constant slot
constexpr unsigned DLIST_TOP    = 02004; // M3 at the start
constexpr unsigned POSITION =
    02011; // 6 words from 21670: "PROGR/END", the position before any marker
constexpr unsigned BLOCK_DLIST = 02022; // M3<<12
constexpr unsigned FREE_SAVED  = 02025;
constexpr unsigned DLIST_TOP2  = 02026;
constexpr unsigned ARRAYS_END  = 02032; // end of the array records at 04000
constexpr unsigned NAMES       = 02144; // slot i at NAMES + i
constexpr unsigned ARRAYS      = 04000;
constexpr unsigned DLIST_END   = 06000;
constexpr unsigned COMMON      = 027245; // 3 words per element: name, D3<<36 | slot<<24, size
constexpr unsigned FLGEX       = 00702;

// Routines and data of the translated pass.
constexpr unsigned READ_BACK    = 027143; // the previous item, in A
constexpr unsigned PRINT        = 01644;  // text first..last on the stack, indent in A
constexpr unsigned FORMAT       = 027733; // text first on the stack, last in A
constexpr unsigned OPEN_SCOPE   = 017455; // *0232B: through M12
constexpr unsigned EMIT_M14     = 017370; // *0145B: item M14, through M13
constexpr unsigned MAIN_LOOP    = 017352; // *0127B, with M13 = *0217B
constexpr unsigned NEXT_ITEM    = 017442; // *0217B
constexpr unsigned SAVE_NAMES   = 017436; // *70 info: page 02000 to drum 26 zone 0
constexpr unsigned NOT_DECLARED = 017311; // text, name at +010
constexpr unsigned WRONG_ORDER  = 017322; // text, name at +010
constexpr unsigned INCORRECT    = 022051; // text up to 022056
constexpr unsigned SCRATCH      = 017333; // *0110B
constexpr unsigned POSITION_SRC = 021670;

constexpr Word BLOCK         = 0'4000'0000'0000'0000; // D-list: a block header
constexpr Word IN_COMMON_CLR = 0'7770'0777'7777'7777;
constexpr Word IN_COMMON     = 0'0001'0000'0000'0000;
constexpr Word DIMENSIONS    = 0'0000'0000'0070'0000;
constexpr Word COMMON_SIMPLE = 0'0000'0000'0010'0000;
constexpr Word RECORD_CLR    = 0'7777'7400'0377'7777; // descriptor without its record field

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
        if (d & BLOCK)
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
            m.mem[m.M[4]] = (m.mem[m.M[4]] & RECORD_CLR) | link | BLOCK;
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
    // Pass 1 writes forward through 27126 from the cursor at 15403.
    pass.write_stream(input, 027126, pass.m.mem[015403]);
    return pass.run(017223, 01627);
}
