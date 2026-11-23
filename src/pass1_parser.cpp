//
// Parser of pass 1 (G13666 and the parser part of G14504): recursive descent
// over the symbols of the lexer, writing the stream and the D-list.
//
// After a syntax error the original resumes at a checkpoint (02106, 02107):
// an address in a routine still running, with the stack cut to its frame. A
// statement sets it to the point after itself in its caller, and so does its
// end; blocks, the declarations and the program set their own. Here the
// routines that own checkpoints run as frames (frame()): the error unwinds
// to the frame of the checkpoint, which goes on at that point.
//
#include <cstdio>
#include <ostream>
#include <stdexcept>
#include <string>

#include "pass1.h"
#include "tables.h"

namespace {

constexpr Word MASK24  = 077777777;
constexpr Word MASK48  = 07777777777777777;
constexpr unsigned R15 = 077777;

// Constants of pass 1 (AE16472 + offset, in the table at 15146…).
Word K(unsigned addr)
{
    return tables::symattr.at(addr);
}

// Attributes of a symbol code: the dispatch fields of the parser.
Word attr(unsigned code)
{
    return code < tables::symattr.words.size() ? tables::symattr.words[code] : 0;
}

Word arx(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & MASK48;
    return s;
}

// APX: gather the bits of a selected by mask into the high bits.
Word apx(Word a, Word mask)
{
    Word r = 0;
    for (int i = 0; i < 48; i++) {
        if ((mask >> i) & 1) {
            r >>= 1;
            r |= ((a >> i) & 1) << 47;
        }
    }
    return r;
}

const Word ONE      = K(015274); // 1
const Word COUNT    = K(015262); // one in a count at bits 12-8
const Word TOO_MANY = K(015404); // a count of 32 or more

// Checkpoints: the address of the original where the parser goes on.
enum : unsigned {
    START          = 0,
    HEAD_NEXT      = 013760, // the program head: after an external name
    AFTER_PROGRAM  = 013732,
    SKIP_TO_EOP    = 014011,
    BEGIN_BODY     = 014060, // after 'BEGIN'
    NEXT_STATEMENT = 014062, // a compound statement: the next statement
    AFTER_COMPOUND = 014063, // … after one
    AFTER_THEN     = 014105, // IF: after the THEN branch
    AFTER_ELSE     = 014111,
    AFTER_THEN_FOR = 014114, // after a THEN branch that is a FOR statement
    AFTER_BODY     = 014050, // FOR: after its body
    AFTER_BLOCK_ST = 014301, // a block: after a statement
    BLOCK_ERROR    = 014265, // … after an error there
    DECLARATIONS   = 014504, // a block: the declarations
    PROC_END       = 014642, // a procedure: after its body
    THIRTY_ERRORS  = 017103, // 30 errors: skip to 'EOP'
};

} // namespace

// The parser state saved in a checkpoint is the frame's own.
struct Pass1::Resume {
    Checkpoint at;
};

//
// Run a frame from START: a syntax error is reported (AG17015) and resumes at
// the checkpoint, here when it is this frame's (body runs from its label),
// else in the frame that owns it.
//
template <class Body>
void Pass1::frame(Body body)
{
    const unsigned self = ++frames_;
    unsigned at         = START;
    for (;;) {
        try {
            body(self, at);
            return;
        } catch (const SyntaxError &e) {
            Checkpoint c = recover(e.number);
            if (c.frame != self)
                throw Resume{ c };
            at = c.label;
        } catch (const Resume &r) {
            if (r.at.frame != self)
                throw;
            at = r.at.label;
        }
    }
}

// AG15120: write an item of the stream. An identifier of a 'COMMON' block
// becomes a subscripted variable with the constant 1.
void Pass1::emit(Word a)
{
    emit_item_ = a;
    if ((a & 07777) == 0) {
        Word slot = (a >> 12) & 07777;
        for (unsigned k = 3;; k += 3) {
            if (ws_[027246 + k] == 0)
                break;
            if (((ws_[027247 + k] & 077777) ^ 1) != 0)
                continue;
            if (((ws_[027246 + k] >> 24) & 07777) != slot)
                continue;
            out_.push_back(Item{ uint32_t(K(015357) & MASK24) });
            a = emit_item_ | K(015304);
            break;
        }
    }
    out_.push_back(Item{ uint32_t(a & MASK24) });
}

// A syntax error found by the parser (the lexer's error() also clears 02000).
void Pass1::syntax_error(unsigned n)
{
    throw SyntaxError{ n };
}

// AG15104: a statement marker, line << 12 | n << 7 | 051.
void Pass1::stmark()
{
    Word a;
    if (ws_[D_LINE] == ws_[D_MARKLINE]) {
        a = arx(ws_[D_MARKN], K(015262));
    } else {
        ws_[D_MARKLINE] = ws_[D_LINE];
        a               = K(015262);
    }
    ws_[D_MARKN] = a;
    emit(((ws_[D_MARKLINE] << 12) & MASK48) | ws_[D_MARKN] | K(015361));
}

// AG15102: the checkpoint, then a marker.
void Pass1::setres(unsigned owner, unsigned label)
{
    checkpoint_ = { owner, label };
    stmark();
}

// AG15114: push a word onto the D-list, which grows down from 05753.
void Pass1::dpush(Word a)
{
    ws_[m3_] = a;
    m3_      = (m3_ - 1) & R15;
    if (((m3_ + 073401) & R15) == 0)
        throw SyntaxError{ 015 };
}

void Pass1::print_line(const std::string &line)
{
    if (ctx_.listing)
        ctx_.listing->print(line);
}

//
// AG17015: report the syntax error n, skip a symbol (unless at ';' or END)
// and write 044; the checkpoint to go on at. At 'EOP' the compilation ends;
// after 30 errors the rest is skipped to 'EOP'.
//
Pass1::Checkpoint Pass1::recover(unsigned n)
{
    for (;;) {
        try {
            if (++ctx_.flgex == 30) {
                checkpoint_.label = THIRTY_ERRORS;
                print_line("30 ERRORS , COMPILATION INTERRUPTED");
                skip_to_eop();
            }
            rau_ = 6;
            print_error(n);
            if (D0() != 051 && D0() != 037) {
                if (D0() == 075)
                    eop_inside();
                if (ws_[D_SEMI]) {
                    ws_[D_SEMI] = 0;
                    lex();
                    ws_[D_SYM] = 051;
                    m4_        = 051;
                } else {
                    ws_[D_WARNED]   = 051;
                    ws_[D_AFTEREND] = 051;
                    ws_[D_KEYWORD]  = 0;
                    lex();
                }
            }
            emit(044);
            ws_[D_WARNED] = 0;
            if (checkpoint_.label == THIRTY_ERRORS)
                skip_to_eop();
            return checkpoint_;
        } catch (const SyntaxError &e) {
            n = e.number;
        }
    }
}

// *2372B: after 30 errors, the symbols up to 'EOP'.
void Pass1::skip_to_eop()
{
    do
        lex();
    while (D0() != 075);
    throw Finish{};
}

// AG17066: 'EOP' inside the program.
void Pass1::eop_inside()
{
    print_line("↑EOP↑ BEFORE END OF PROGRAM");
    ctx_.flgex = 1;
    throw Finish{};
}

//
// AG13666: the start of the pass, the program head, the program, and the
// symbols after it up to 'EOP'.
//
void Pass1::program()
{
    if (ctx_.options.list)
        blank_line(ctx_);
    ws_[D_LIST] = ctx_.options.list ? 1 : 0;
    m5_         = 0;
    m6_         = 0;
    m3_         = 05753;
    ws_[02001]  = K(015403);
    ws_[D_POOL] = K(015371);
    frame([&](unsigned self, unsigned at) {
        switch (at) {
        case START:
            checkpoint_ = { self, HEAD_NEXT };
            emit(0102);
            stmark();
            lex();
            ws_[02054] = 0;
            [[fallthrough]];
        case HEAD_NEXT:
            program_head(self, at == HEAD_NEXT);
            [[fallthrough]];
        case AFTER_PROGRAM: // *0044B
            checkpoint_ = { self, SKIP_TO_EOP };
            if (D0() != 075) {
                stmark();
                syntax_error(0143);
            }
            break;
        case SKIP_TO_EOP: // *0122B
            while (D0() != 075)
                lex();
            break;
        default:
            throw std::logic_error("pass 1: no checkpoint " + std::to_string(at));
        }
        // *0050B: the last sector of the string pool.
        Word pool   = ws_[D_POOL];
        ws_[D_POOL] = pool | K(015413);
        pool_put(ws_[D_POOL]);
        ws_[D_POOL] = pool;
    });
}

//
// L13711: the program head, up to the program: 'ALGOL': and 'CODE': lists of
// external names (a type before one, ',' between them, ';' after a list),
// labels, then 'BEGIN' or a 'PROCEDURE'. In a list (cell 02054: the kind)
// each name goes to the D-list with its type (02055) and slot (02056).
// resumed: at HEAD_NEXT, after a name.
//
void Pass1::program_head(unsigned self, bool resumed)
{
    // *0125B: an error in the program head.
    auto bad = [&] {
        if (D0() == 075)
            eop_inside();
        if (D0() == 037)
            lex();
        syntax_error(0137);
    };
    // A name of the list (L13755): to the D-list, then ',' and the next, or
    // ';'. entry: a name was read (else at L13760, after one).
    auto names = [&](bool entry) {
        if (entry && !ws_[02054])
            bad();
        for (;;) {
            if (entry)
                dpush(ws_[02054] | ws_[02055] | ws_[02056]);
            entry = true;
            m4_   = (m4_ - 051) & R15; // L13760
            if (!m4_) {
                lex();
                return;
            }
            m4_ = (m4_ + 2) & R15;
            if (m4_)
                bad();
            lex();
            if (m4_)
                bad();
            ws_[02056] = D0() >> 12;
            lex();
        }
    };
    if (resumed)
        names(false);
    for (;;) { // L13711
        ws_[02055] = K(015316);
        Word type  = 0;
        switch ((attr(m4_) & 017000000) >> 18) {
        case 1: // 'ALGOL' :
        case 2: // 'CODE' :
            ws_[02054] = ((attr(m4_) & 017000000) >> 18) == 1 ? K(015275) : K(015360);
            lex();
            m4_ = (m4_ - 050) & R15;
            if (m4_)
                bad();
            lex();
            continue;
        case 3:
            type = K(015331);
            break;
        case 4:
            type = K(015324);
            break;
        case 5:
            type = K(015326);
            break;
        case 6:
            type = K(015330);
            break;
        case 012:
            type = K(015327);
            break;
        case 7: // 'PROCEDURE'
            procedure(ws_[02055] | K(015317));
            return;
        case 010: // labels before the program, or a name of a list
            for (;;) {
                ws_[02056] = D0() >> 12;
                lex();
                if (D0() == 047 || D0() == 051)
                    break;
                if (D0() != 050)
                    bad();
                lex();
                emit((Word(m3_) << 12) | K(015256));
                if (!ws_[D_RENAME]) {
                    ws_[02136]    = ws_[NAMES + ws_[02056]];
                    ws_[D_RENAME] = apx(ws_[02136], K(015407));
                }
                dpush(ws_[02056] | K(015351));
                if (m4_) {
                    if (D0() != 036)
                        bad();
                    statement(self, AFTER_PROGRAM);
                    return;
                }
            }
            names(true);
            continue;
        case 011: // 'BEGIN'
            statement(self, AFTER_PROGRAM);
            return;
        default:
            bad();
        }
        // L13750: a type before an external name, or before 'PROCEDURE'.
        ws_[02055] = type;
        lex();
        if (D0() == 042) {
            procedure(ws_[02055] | K(015317));
            return;
        }
        if (m4_)
            bad();
        ws_[02056] = D0() >> 12;
        lex();
        names(true);
    }
}

//
// AG14020: a statement. Its checkpoint is the point after it in the caller
// (frame caller, label cont), at the start and at the end.
//
void Pass1::statement(unsigned caller, unsigned cont)
{
    setres(caller, cont);
    Word saved = 0; // a FOR: the D-list pointer; a block: its header word
    frame([&](unsigned self, unsigned at) {
        switch (at) {
        case START:
            statement_body(self, saved);
            break;
        case BEGIN_BODY:
        case NEXT_STATEMENT:
        case AFTER_COMPOUND:
            compound(self, at, saved);
            break;
        case AFTER_THEN:
        case AFTER_ELSE:
        case AFTER_THEN_FOR:
            if_statement(self, at);
            break;
        case AFTER_BODY:
            for_statement(self, at, saved);
            break;
        case AFTER_BLOCK_ST:
        case BLOCK_ERROR:
        case DECLARATIONS:
            block(self, at, saved);
            break;
        default:
            throw std::logic_error("pass 1: no checkpoint " + std::to_string(at));
        }
    });
    checkpoint_ = { caller, cont }; // *0200B
}

// *0135B: a statement by its first symbol; labels before it.
void Pass1::statement_body(unsigned self, Word &saved)
{
    for (;;) {
        switch ((attr(m4_) & 03600) >> 7) {
        case 0:
            syntax_error(042);
        case 1: // 'BEGIN'
            compound(self, START, saved);
            return;
        case 2: // 'IF'
            lex();
            if_statement(self, START);
            return;
        case 3: // 'GOTO'
            lex();
            expression(0);
            emit(026);
            return;
        case 4: { // *0233B: an identifier: a label, an assignment or a call
            Word a = D0();
            lex();
            if (D0() == 050) {
                lex();
                emit((Word(m3_) << 12) | K(015256));
                dpush((a >> 12) | K(015351));
                continue;
            }
            designator(011, a);
            assignment();
            return;
        }
        case 5: // 'FOR'
            for_statement(self, START, saved);
            return;
        case 6:
            io_statement(K(015335));
            return;
        case 7:
            io_format(K(015373));
            return;
        case 010:
            io_three(K(015335), K(015341));
            return;
        case 011:
            io_three(K(015373), K(015345));
            return;
        case 012: // 'PRINT'
            io_print(K(015373), K(015312));
            return;
        case 013:
            io_print(K(015373), K(015270));
            return;
        case 014:
            io_simple(K(015350));
            return;
        case 015:
            io_simple(K(015362));
            return;
        case 016:
            io_simple(K(015367));
            return;
        default: // an empty statement
            return;
        }
    }
}

// *0250B: after a designator at the start of a statement: ':=' and the left
// parts, then the expression; else it was a call.
void Pass1::assignment()
{
    if (D0() != 022)
        return;
    Word left = COUNT;
    for (;;) { // *0252B
        lex();
        if (m4_) {
            expression(0);
            break;
        }
        expression(0);
        if (D0() != 022)
            break;
        left = arx(left, COUNT);
    }
    emit(left | K(015306));
}

//
// *0170B: 'BEGIN': a compound statement, or a block when declarations
// follow. at: where it goes on.
//
void Pass1::compound(unsigned self, unsigned at, Word &saved)
{
    if (at == START) {
        setres(self, BEGIN_BODY);
        lex();
        at = BEGIN_BODY;
    }
    if (at == BEGIN_BODY && (attr(m4_) & 04000)) { // *0172B
        block(self, START, saved);
        return;
    }
    if (at != AFTER_COMPOUND)
        statement(self, AFTER_COMPOUND);
    for (;;) { // L14063
        if (D0() == 051) {
            lex();
            statement(self, AFTER_COMPOUND);
            continue;
        }
        if (D0() != 037) { // *0202B
            checkpoint_ = { self, NEXT_STATEMENT };
            syntax_error(036);
        }
        lex();
        return;
    }
}

//
// *0206B: 'IF' b 'THEN' s ['ELSE' s]: 030 after the condition, 031 before
// the ELSE branch, then 054, or 053 without it. at: where it goes on.
//
void Pass1::if_statement(unsigned self, unsigned at)
{
    if (at == START) {
        expression(0);
        if (D0() != 030)
            syntax_error(037);
        lex();
        emit(K(015346));
        if (D0() == 027)
            syntax_error(040);
        if (D0() == 032) {
            statement(self, AFTER_THEN_FOR);
            at = AFTER_THEN_FOR;
        } else {
            statement(self, AFTER_THEN);
            at = AFTER_THEN;
        }
    }
    switch (at) {
    case AFTER_THEN_FOR:
        if (D0() == 031)
            syntax_error(041);
        emit(053);
        return;
    case AFTER_THEN:
        if (D0() != 031) {
            emit(053);
            return;
        }
        lex();
        emit(K(015363));
        statement(self, AFTER_ELSE);
        [[fallthrough]];
    default: // AFTER_ELSE
        emit(054);
        return;
    }
}

// *0160B: 'FOR': its head, its body, its end. at: where it goes on.
void Pass1::for_statement(unsigned self, unsigned at, Word &saved)
{
    if (at == START) {
        saved = m3_;
        for_head();
        statement(self, AFTER_BODY);
    }
    for_end(saved);
}

// AG14050: the end of a FOR statement: its D-list word and 052.
void Pass1::for_end(Word saved)
{
    dpush(arx(saved, ONE) | K(015305));
    emit((Word(arx(m3_, ONE)) << 12) | K(015366));
    stmark();
}

// AG14655: the head of a FOR statement, up to DO.
void Pass1::for_head()
{
    lex();
    if (m4_)
        syntax_error(043);
    emit(K(015401));
    Word var = D0();
    emit(D0() | K(015365));
    lex();
    if (D0() != 022)
        syntax_error(044);
    for (;;) { // *0162B: a FOR list element
        lex();
        expression(0);
        unsigned code = 0;
        switch ((attr(m4_) & 030000) >> 12) {
        case 0:
            syntax_error(045);
        case 1:
            code = 056;
            break;
        case 2: // 'WHILE'
            lex();
            expression(0);
            code = 035;
            break;
        default: // 'STEP'
            lex();
            expression(0);
            if (D0() != 052)
                syntax_error(046);
            lex();
            expression(0);
            code = 034;
            break;
        }
        emit(code);
        if (D0() == 047)
            continue;
        if (D0() != 033)
            syntax_error(047);
        lex();
        emit(K(015370) | var);
        return;
    }
}

//
// *0371B: a block with declarations: 036, the declarations, the statements,
// its D-list header (saved: m3_ at its start, then bit 48 when it has
// arrays), 037. at: where it goes on.
//
void Pass1::block(unsigned self, unsigned at, Word &saved)
{
    switch (at) {
    case START:
        saved = m3_;
        emit(K(015406));
        setres(self, DECLARATIONS);
        declarations(self, saved);
        statement(self, AFTER_BLOCK_ST);
        break;
    case DECLARATIONS: // AG14504
        setres(self, DECLARATIONS);
        if (D0() == 051)
            lex();
        declarations(self, saved);
        statement(self, AFTER_BLOCK_ST);
        break;
    case BLOCK_ERROR:
        statement(self, AFTER_BLOCK_ST);
        break;
    }
    for (;;) { // *0413B: after a statement
        if (D0() == 051) {
            lex();
            statement(self, AFTER_BLOCK_ST);
            continue;
        }
        if (D0() != 037) { // *0425B
            setres(self, BLOCK_ERROR);
            syntax_error(056);
        }
        lex();
        dpush(arx(saved, ONE));
        emit((Word(arx(m3_, ONE)) << 12) | K(015412));
        stmark();
        return;
    }
}

// AG14262: the declarations of a block, up to its first statement.
void Pass1::declarations(unsigned self, Word &saved)
{
    for (;;) {
        Word type = 0;
        switch ((attr(m4_) & 0740000) >> 14) {
        case 0: // the statements
            return;
        case 1:
            type = K(015324);
            break;
        case 2:
            type = K(015326);
            break;
        case 3:
            type = K(015330);
            break;
        case 010:
            type = K(015331);
            break;
        case 011:
            type = K(015327);
            break;
        case 4: // 'PROCEDURE'
            procedure_declaration(self, K(015317));
            continue;
        case 5: // 'SWITCH'
            switch_declaration(K(015374));
            continue;
        case 6:
            syntax_error(050);
        case 7: // 'ARRAY'
            array_declaration(K(015375), saved);
            continue;
        default:
            throw std::logic_error("pass 1: declaration dispatch");
        }
        // *0427B: a simple declaration
        lex();
        if (!m4_) {
            variables(type, saved);
        } else if (D0() == 042) {
            procedure_declaration(self, type | K(015260));
        } else {
            if (D0() != 040)
                syntax_error(057);
            array_declaration(type | K(015352), saved);
        }
    }
}

// *0514B: a procedure declaration; the declarations go on after ';'.
void Pass1::procedure_declaration(unsigned self, Word type)
{
    procedure(type);
    setres(self, DECLARATIONS);
    if (D0() != 051)
        syntax_error(066);
    lex();
}

//
// *0435B: an array declaration: segments of names with their bounds. The
// D-list words of a segment get the number of dimensions (bits 12-8 << 8).
// The block's header word gets bit 48.
//
void Pass1::array_declaration(Word type, Word &saved)
{
    saved |= K(015353);
    for (;;) { // *0437B: an array segment
        Word count = 0;
        lex();
        for (;;) { // *0441B
            if (m4_)
                syntax_error(060);
            dpush((D0() >> 12) | type);
            count = arx(count, COUNT);
            lex();
            if (D0() == 047) {
                lex();
                continue;
            }
            break;
        }
        Word closer = arx(D0(), K(015333));
        if (D0() != 021 && D0() != 020)
            syntax_error(061);
        if (count & TOO_MANY)
            syntax_error(062);
        emit((Word(arx(m3_, ONE)) << 12) | count | K(015255));
        Word dims = COUNT;
        for (;;) { // *0462B: bounds
            lex();
            expression(0);
            if (D0() != 050)
                syntax_error(063);
            lex();
            expression(0);
            if (D0() != 047)
                break;
            dims = arx(dims, COUNT);
        }
        if (closer != D0())
            syntax_error(064);
        emit(dims | K(015320));
        unsigned n = (count >> 7) & R15;
        unsigned p = m3_;
        do { // *0502B
            p      = (p + 1) & R15;
            ws_[p] = ((dims << 8) & MASK48) | ws_[p];
            n      = (n - 1) & R15;
        } while (n);
        lex();
        if (D0() == 047)
            continue;
        if (D0() != 051)
            syntax_error(065);
        lex(); // *0411B
        return;
    }
}

//
// *0522B: simple variables. A variable of a 'COMMON' block is an array
// [1:1] (its block's header gets bit 48); the others get K(015321).
//
void Pass1::variables(Word type, Word &saved)
{
    for (;;) {
        Word slot      = D0() >> 12;
        bool in_common = false;
        for (unsigned k = 0;; k = (k + 3) & R15) {
            Word a = ws_[(027246 + k) & R15];
            if (!a)
                break;
            if ((((a >> 24) & 07777) ^ slot) == 0) {
                in_common = true;
                break;
            }
        }
        if (in_common) { // *0532B
            saved |= K(015353);
            dpush((D0() >> 12) | type);
            Word count = arx(0, COUNT);
            emit((Word(arx(m3_, ONE)) << 12) | count | K(015255));
            Word dims = COUNT;
            emit(K(015357));
            emit(K(015357));
            emit(dims | K(015320));
            unsigned p = (m3_ + 1) & R15;
            ws_[p]     = ((dims << 8) & MASK48) | ws_[p];
        } else { // *0560B
            type |= K(015321);
            dpush((D0() >> 12) | type);
        }
        lex(); // *0563B
        if (D0() == 051) {
            lex();
            return;
        }
        if (D0() != 047)
            syntax_error(067);
        lex();
        if (m4_)
            syntax_error(070);
    }
}

// *0572B: 'SWITCH' s := designational expressions: 041 with s and their number.
void Pass1::switch_declaration(Word type)
{
    lex();
    if (m4_)
        syntax_error(071);
    Word start = m3_;
    Word count = 0;
    dpush((D0() >> 12) | type);
    lex();
    if (D0() != 022)
        syntax_error(072);
    for (;;) { // *0602B
        lex();
        expression(0);
        count = arx(count, COUNT);
        if (D0() != 047)
            break;
    }
    if (D0() != 051)
        syntax_error(073);
    if (count & TOO_MANY)
        syntax_error(074);
    emit(((start << 12) & MASK48) | count | K(015277));
    lex(); // *0411B
}

//
// AG14510: a procedure heading and its body. type: its D-list word (name
// slot ORed in). The formals go to the D-list after its 042, then the
// VALUE list (061 each), the specifications (each formal exactly once),
// the body; then 062, its D-list words and the number of formals.
//
void Pass1::procedure(Word type)
{
    Word start = 0, head = 0, name = 0, count = 0;
    frame([&](unsigned self, unsigned at) {
        if (at == START) {
            lex();
            if (m4_)
                syntax_error(0106);
            start = arx(m3_, ONE);
            head  = start | K(015301);
            name  = (D0() >> 12) | type;
            emit(K(015307));
            lex();
            if (D0() == 051) { // *0145B: no parameters
                name |= K(015323);
                count = 0;
                lex();
            } else {
                formals(start, count);
            }
            statement(self, PROC_END);
        }
        emit((Word(m3_) << 12) | K(015311)); // L14642
        dpush(head);
        dpush(((count << 8) & MASK48) | name);
    });
}

// The formals of a procedure heading: names, VALUE, specifications.
void Pass1::formals(Word start, Word &count)
{
    if (D0() != 020)
        syntax_error(0107);
    lex();
    count = 0;
    for (;;) { // *0022B
        if (m4_)
            syntax_error(0110);
        count = arx(count, COUNT);
        dpush((D0() >> 12) | K(015336));
        lex();
        if (D0() != 047)
            break;
        lex();
    }
    if (D0() != 045)
        syntax_error(0111);
    lex();
    if (count & TOO_MANY)
        syntax_error(0112);
    if (D0() != 051)
        syntax_error(0113);
    lex();
    if (D0() == 061) { // *0042B: 'VALUE'
        lex();
        for (;;) {
            if (m4_)
                syntax_error(0114);
            unsigned f = find_formal(start);
            if (ws_[f] & K(015261))
                syntax_error(0116);
            ws_[f] = (ws_[f] & K(015376)) | K(015261);
            emit((Word(f) << 12) | K(015302));
            lex();
            if (D0() != 047)
                break;
            lex();
        }
        if (D0() != 051)
            syntax_error(0117);
        lex();
    }
    Word specified = 0;
    for (;;) { // *0060B: the specifications
        Word spec = 0;
        bool read = true;
        switch ((attr(m4_) & 0740000) >> 14) {
        case 0: // *0133B
            if (specified ^ count)
                syntax_error(0124);
            return;
        case 1:
        case 2:
        case 3:
        case 010:
        case 011: { // *0075B: a type, then maybe ARRAY or PROCEDURE
            static const unsigned types[] = {
                0, 015263, 015264, 015266, 0, 0, 0, 0, 015267, 015265
            };
            spec = K(types[(attr(m4_) & 0740000) >> 14]);
            lex();
            if (!m4_) {
                spec = K(015372) | spec;
                read = false;
            } else if (D0() == 040) {
                spec = K(015352) | spec;
            } else {
                if (D0() != 042)
                    syntax_error(0120);
                spec = K(015260) | spec;
            }
            break;
        }
        case 4:
            spec = K(015260);
            break;
        case 5:
            spec = K(015352);
            break;
        case 6:
            spec = K(015322);
            break;
        case 7:
            spec = K(015355);
            break;
        default:
            throw std::logic_error("pass 1: specification dispatch");
        }
        if (read) // *0107B
            lex();
        for (;;) { // *0110B
            if (m4_)
                syntax_error(0121);
            specified  = arx(specified, COUNT);
            unsigned f = find_formal(start);
            if (ws_[f] & K(015377))
                syntax_error(0122);
            ws_[f] |= spec;
            lex();
            if (D0() == 051) {
                lex();
                break;
            }
            if (D0() != 047)
                syntax_error(0123);
            lex();
        }
    }
}

//
// I/O statements. Their elements (AG14716) leave 02117 nonzero when they
// wrote the statement's item (02052) themselves.
//

// *0302B: the first item, then the elements of the list (unless the next
// symbol ends the statement), the item after each, and 073.
void Pass1::io_list(Word first)
{
    emit(first);
    if ((attr(m4_) & 03600) != 03600) {
        for (;;) { // *0306B
            io_element();
            if (!ws_[02117])
                emit(ws_[02052]);
            if (D0() != 047)
                break;
            lex();
        }
    }
    emit(073);
}

// *0266B: (e [, e]) or (e) or e, then the list.
void Pass1::io_statement(Word item)
{
    ws_[02052] = item;
    lex();
    if (D0() != 020) { // *0320B
        emit(K(015332));
        expression(0);
        m4_ = (m4_ - 047) & R15;
        if (!m4_) {
            lex();
        } else {
            m4_ = (m4_ + 047) & R15;
        }
        io_list(064);
        return;
    }
    lex();
    expression(0);
    if (D0() == 045) { // *0314B
        lex();
        if (D0() != 047) {
            io_list(K(015257));
            return;
        }
        lex();
        io_list(K(015300));
        return;
    }
    if (D0() != 047)
        syntax_error(0132);
    lex();
    expression(0);
    m4_ = (m4_ - 045) & R15;
    if (m4_)
        syntax_error(0132);
    lex();
    io_list(064);
}

// *0325B: (e) or (e, e), then the list.
void Pass1::io_format(Word item)
{
    ws_[02052] = item;
    lex();
    m4_ = (m4_ - 020) & R15;
    if (m4_)
        syntax_error(0133);
    lex();
    expression(0);
    m4_ = (m4_ - 045) & R15;
    if (!m4_) { // *0340B
        lex();
        io_list(K(015315));
        return;
    }
    m4_ = (m4_ - 2) & R15;
    if (m4_)
        syntax_error(0133);
    lex();
    expression(0);
    m4_ = (m4_ - 045) & R15;
    if (m4_)
        syntax_error(0133);
    lex();
    io_list(K(015314));
}

// *0342B: PRINT and the like: e, then the list.
void Pass1::io_print(Word item, Word first)
{
    ws_[02052] = item;
    lex();
    emit(first); // *0344B
    expression(0);
    m4_ = (m4_ - 047) & R15;
    if (!m4_)
        lex();
    else
        m4_ = (m4_ + 047) & R15;
    io_list(K(015314));
}

// *0352B: (e, e, e), then the list after the item first.
void Pass1::io_three(Word item, Word first)
{
    ws_[02052] = item;
    lex(); // *0353B
    unsigned sep = 020;
    for (int k = 0; k < 3; k++) {
        m4_ = (m4_ - sep) & R15; // *0364B
        if (m4_)
            syntax_error(0135);
        lex();
        expression(0);
        sep = 047;
    }
    m4_ = (m4_ - 045) & R15;
    if (m4_)
        syntax_error(0135);
    lex();
    io_list(first);
}

// *0263B: a statement of one expression, then its item.
void Pass1::io_simple(Word item)
{
    lex();
    expression(0);
    emit(item);
}

//
// AG14716: an element of an I/O list: an expression, a FOR list element
// (FOR head, element, then the end of a FOR), or a parenthesized list of
// elements (which may go on as an expression).
//
void Pass1::io_element()
{
    // *0232B: after an element inside parentheses: ',' and the next, or ')'.
    auto rest = [&] {
        for (;;) {
            if (D0() == 047) {
                lex();
                io_element();
                if (!ws_[02117])
                    emit(ws_[02052]);
                continue;
            }
            if (D0() != 045)
                syntax_error(0136);
            ws_[02117] = ws_[02052];
            lex();
            return;
        }
    };
    if (D0() == 020) { // *0220B
        lex();
        io_element();
        if (ws_[02117]) {
            rest();
            return;
        }
        if (D0() != 045) {
            emit(ws_[02052]);
            rest();
            return;
        }
        lex();
        if (attr(m4_) & 0170)
            operators(0);
        ws_[02117] = 0;
        return;
    }
    if (D0() == 032) { // *0244B: FOR in an I/O list
        Word saved = m3_;
        for_head();
        io_element();
        if (!ws_[02117]) {
            ws_[02117] = ws_[02052];
            emit(ws_[02117]);
        }
        ws_[02054] = saved;
        for_end(saved);
        return;
    }
    expression(0);
    ws_[02117] = 0;
}

//
// AG14762: an expression in a context of priority prio: operands, unary and
// binary operators by priority (their items after the operands), IF
// expressions (prio 0 only, or after one), designators.
//
void Pass1::expression(unsigned prio)
{
    switch (attr(m4_) & 07) {
    case 0:
        syntax_error(024);
    case 1: // 'IF'
        if (prio)
            syntax_error(026);
        conditional(prio);
        return;
    case 2: // '('
        lex();
        expression(0);
        if (D0() != 045)
            syntax_error(025);
        lex();
        operators(prio);
        return;
    case 3: // 'NOT'
        unary(prio, 5, K(015325));
        return;
    case 4: // '+', '-'
        unary(prio, 7, arx(D0(), K(015370)));
        return;
    case 5: { // identifier
        Word id = D0();
        lex();
        designator(prio, id);
        return;
    }
    case 6: // constant
        emit(D0());
        lex();
        operators(prio);
        return;
    default:
        conditional(prio);
        return;
    }
}

// *0271B: 'IF' b 'THEN' e 'ELSE' e, then 045.
void Pass1::conditional(unsigned prio)
{
    lex();
    expression(prio);
    if (D0() != 030)
        syntax_error(027);
    lex();
    expression(1);
    if (D0() != 031)
        syntax_error(030);
    lex();
    expression(0);
    emit(K(015334));
}

// *0312B: binary operators of priority prio or more, after an operand.
void Pass1::operators(unsigned prio)
{
    for (;;) {
        Word a = attr(m4_) & 0170;
        if (!a)
            return;
        unsigned p = (a >> 3) & R15;
        rau_       = 026;
        if (int(p) - int(prio & R15) < 0)
            return;
        Word op = D0();
        lex(); // *0320B
        expression(p + 1);
        emit(op);
    }
}

// *0326B: a unary operator of priority p, item op.
void Pass1::unary(unsigned prio, unsigned p, Word op)
{
    rau_ = 026;
    if (int(p) - int(prio & R15) < 0)
        syntax_error(031);
    lex();
    expression(p + 1);
    emit(op);
    operators(prio);
}

//
// AG15037: after an identifier: a call f(…), an element a[…], a slice
// s[i:j], or the identifier alone; then the operators.
//
void Pass1::designator(unsigned prio, Word id)
{
    Word closer = 0;
    Word count  = 0;
    if (D0() == 020) {
        closer = K(015334);
    } else if (D0() == 021) {
        closer = K(015340);
        lex();
        expression(0);
        count = arx(count, COUNT); // L15063
        if (D0() == 050) {         // *0370B: a slice
            lex();
            expression(0);
            emit(id | K(015347));
            if (D0() != 046)
                syntax_error(033);
            lex();
            operators(prio);
            return;
        }
        if (D0() != 047) {
            if (D0() != 046)
                syntax_error(033);
            emit(count | id | K(015303));
            lex();
            operators(prio);
            return;
        }
    } else {
        emit(id);
        operators(prio);
        return;
    }
    for (;;) { // *0337B: arguments or subscripts
        lex();
        expression(0);
        count = arx(count, COUNT);
        if (D0() != 047)
            break;
    }
    if (D0() != closer)
        syntax_error(032);
    if (count & TOO_MANY)
        syntax_error(023);
    Word a = (closer | count | id) - K(015333);
    rau_   = 2;
    emit(a);
    lex();
    operators(prio);
}

// *0124B: the formal parameter named by the symbol; its D-list address,
// searched down from the heading's start.
unsigned Pass1::find_formal(Word start)
{
    ws_[D_SYM] = ws_[D_SYM] >> 12;
    unsigned f = start & R15;
    for (;;) {
        f      = (f - 1) & R15;
        Word w = ws_[f];
        if (!w)
            throw SyntaxError{ 0115 };
        if (((w & 07777) ^ ws_[D_SYM]) == 0)
            return f;
    }
}

// The line of a syntax error: ERROR nn and the class of the error (17166).
void Pass1::print_error(unsigned n)
{
    if (!ws_[D_LIST] && ws_[D_LINE] != ws_[D_PRINTED]) {
        print_card();
        ws_[D_PRINTED] = ws_[D_LINE];
    }
    unsigned cls = (tables::errclass.at(017166 + (n >> 3)) >> (6 * (n & 7))) & 077;
    std::string text;
    // A string of the pieces at 17121: ISO bytes, 0200+k inserts piece k.
    auto piece = [&](auto &&self, unsigned addr) -> void {
        for (;; addr++) {
            Word w = tables::syntext.at(addr);
            for (int i = 5; i >= 0; i--) {
                unsigned c = (w >> (8 * i)) & 0377;
                if (c == 0)
                    return;
                if (c & 0200)
                    self(self, 017121 + (c & 0177));
                else
                    text += char(c);
            }
        }
    };
    piece(piece, 017121 + cls);
    char num[16];
    std::snprintf(num, sizeof(num), "ERROR %02u    ", n);
    print_line(num + text);
}

Stream Pass1::run()
{
    try {
        program();
    } catch (const NoEop &) {
        print_line("HET ′EOP′ !");
        ctx_.flgex    = 1;
        ctx_.finished = true;
    } catch (const Finish &) {
        ctx_.finished = true;
    }
    // Registers pass 2 takes over: M3 is the D-list pointer.
    ctx_.regs[3] = m3_;
    ctx_.regs[4] = m4_;
    ctx_.regs[5] = m5_;
    ctx_.regs[6] = m6_;
    return out_;
}
