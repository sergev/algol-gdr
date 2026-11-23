//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
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

namespace {

constexpr Word MASK24  = 077777777;
constexpr Word MASK48  = 07777777777777777;
constexpr unsigned R15 = 077777;

// Words of pass 1 (its constants at 15255…). D-list words: a descriptor and
// its type, kind and flags.
constexpr Word HEADER     = 0'4000'0000'0000'0000; // a block header
constexpr Word DESCRIPTOR = 0'2000'0000'0000'0000;
constexpr Word BIT46      = 0'1000'0000'0000'0000; // a formal by name; the end of a FOR
constexpr Word BIT45      = 0'0400'0000'0000'0000; // a procedure's head
constexpr Word STRING     = 0'0200'0000'0000'0000;
constexpr Word BOOLEAN    = 0'0100'0000'0000'0000;
constexpr Word BITS       = 0'0060'0000'0000'0000;
constexpr Word INTEGER    = 0'0040'0000'0000'0000;
constexpr Word REAL       = 0'0020'0000'0000'0000;
constexpr Word LABEL      = 0'0010'0000'0000'0000;
constexpr Word BY_VALUE   = 0'0004'0000'0000'0000; // a VALUE formal, a simple variable
constexpr Word PROCEDURE  = 0'0002'0000'0000'0000;
constexpr Word ARRAY      = 0'0001'0000'0000'0000;
constexpr Word SIMPLE     = 0'0000'4000'0000'0000; // no parameters, no subscripts
constexpr Word SPECIFIED  = 0'0373'4000'0000'0000; // a formal's specification bits
constexpr Word ALGOL_PROC = 0'0402'0000'0000'0000; // 'ALGOL': procedures before the program
constexpr Word CODE_PROC  = 0'4402'0000'0000'0000; // 'CODE'

// Items of the preset constants (slots 01626…).
constexpr Word CONST_52 = 01626 << 12 | 024;
constexpr Word CONST_51 = 01627 << 12 | 024;
constexpr Word CONST_50 = 01630 << 12 | 024;
constexpr Word CONST_1  = 01632 << 12 | 024;

constexpr Word ONE       = 1;
constexpr Word COUNT     = 0200;                  // one in a count at bits 12-8
constexpr Word TOO_MANY  = 0'7777'7777'7777'0000; // a count of 32 or more
constexpr Word NOT_BLANK = 0'2765'7537'2765'7537; // a word of blanks and zeros has none

// Initial cursors: the stream (02001), the string pool (02002).
constexpr Word STREAM_START = 0'0000'0302'6020'6001;
constexpr Word POOL_START   = 0'4420'2767'4000'4000;

// Attributes of a symbol code (15146…): where the parser goes with it.
struct SymbolAttr {
    unsigned code;
    unsigned head;        // before the program: 'ALGOL', 'CODE', a type, …
    unsigned statement;   // the first symbol of a statement
    bool block;           // a declaration: the compound statement is a block
    unsigned for_element; // after a FOR list element: DO, ',', STEP, WHILE
    unsigned declaration; // in declarations and specifications
    unsigned priority;    // a binary operator
    unsigned expression;  // the first symbol of an expression
};

// clang-format off
const SymbolAttr SYMBOL_ATTRS[] = {
    { 0000, 8, 4, 0, 0, 0, 0, 5 },
    { 0001, 0, 0, 0, 0, 0, 2, 0 },
    { 0002, 0, 0, 0, 0, 0, 3, 0 },
    { 0003, 0, 0, 0, 0, 0, 4, 0 },
    { 0004, 0, 0, 0, 0, 0, 0, 3 },
    { 0005, 0, 0, 0, 0, 0, 6, 0 },
    { 0006, 0, 0, 0, 0, 0, 6, 0 },
    { 0007, 0, 0, 0, 0, 0, 6, 0 },
    { 0010, 0, 0, 0, 0, 0, 6, 0 },
    { 0011, 0, 0, 0, 0, 0, 6, 0 },
    { 0012, 0, 0, 0, 0, 0, 6, 0 },
    { 0013, 0, 0, 0, 0, 0, 7, 4 },
    { 0014, 0, 0, 0, 0, 0, 7, 4 },
    { 0015, 0, 0, 0, 0, 0, 8, 0 },
    { 0016, 0, 0, 0, 0, 0, 8, 0 },
    { 0017, 0, 0, 0, 0, 0, 9, 0 },
    { 0020, 0, 0, 0, 0, 0, 0, 2 },
    { 0023, 0, 0, 0, 0, 0, 0, 6 },
    { 0024, 0, 0, 0, 0, 0, 0, 6 },
    { 0025, 0, 0, 0, 0, 0, 0, 6 },
    { 0026, 0, 3, 0, 0, 0, 0, 0 },
    { 0027, 0, 2, 0, 0, 0, 0, 1 },
    { 0031, 0, 15, 0, 0, 0, 0, 0 },
    { 0032, 0, 5, 0, 0, 0, 0, 0 },
    { 0033, 0, 0, 0, 1, 0, 0, 0 },
    { 0034, 0, 0, 0, 3, 0, 0, 0 },
    { 0035, 0, 0, 0, 2, 0, 0, 0 },
    { 0036, 9, 1, 0, 0, 0, 0, 0 },
    { 0037, 0, 15, 0, 0, 0, 0, 0 },
    { 0040, 0, 0, 1, 0, 7, 0, 0 },
    { 0041, 0, 0, 1, 0, 5, 0, 0 },
    { 0042, 7, 0, 1, 0, 4, 0, 0 },
    { 0043, 0, 0, 0, 0, 0, 1, 0 },
    { 0047, 0, 0, 0, 1, 0, 0, 0 },
    { 0051, 0, 15, 0, 0, 0, 0, 0 },
    { 0054, 6, 0, 1, 0, 3, 0, 0 },
    { 0055, 5, 0, 1, 0, 2, 0, 0 },
    { 0056, 4, 0, 1, 0, 1, 0, 0 },
    { 0057, 3, 0, 1, 0, 8, 0, 0 },
    { 0060, 0, 0, 0, 0, 6, 0, 0 },
    { 0062, 0, 6, 0, 0, 0, 0, 0 },
    { 0063, 0, 10, 0, 0, 0, 0, 0 },
    { 0064, 0, 11, 0, 0, 0, 0, 0 },
    { 0065, 0, 7, 0, 0, 0, 0, 0 },
    { 0066, 0, 8, 0, 0, 0, 0, 0 },
    { 0067, 0, 9, 0, 0, 0, 0, 0 },
    { 0070, 0, 12, 0, 0, 0, 0, 0 },
    { 0071, 0, 13, 0, 0, 0, 0, 0 },
    { 0072, 0, 14, 0, 0, 0, 0, 0 },
    { 0073, 1, 0, 0, 0, 0, 0, 0 },
    { 0074, 2, 0, 0, 0, 0, 0, 0 },
    { 0075, 0, 15, 0, 0, 0, 0, 0 },
    { 0076, 0, 0, 0, 0, 0, 0, 6 },
    { 0077, 0, 0, 0, 0, 0, 0, 6 },
    { 0101, 10, 0, 1, 0, 9, 0, 0 },
    { 0105, 0, 0, 0, 0, 0, 0, 6 },
};
// clang-format on

const SymbolAttr &attr(unsigned code)
{
    static const auto table = [] {
        std::array<SymbolAttr, 0400> t{};
        for (const auto &a : SYMBOL_ATTRS)
            t[a.code] = a;
        return t;
    }();
    return table[code & 0377];
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
            out_.push_back(Item{ uint32_t(CONST_1 & MASK24) });
            a = emit_item_ | 0221;
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
        a = arx(ws_[D_MARKN], COUNT);
    } else {
        ws_[D_MARKLINE] = ws_[D_LINE];
        a               = COUNT;
    }
    ws_[D_MARKN] = a;
    emit(((ws_[D_MARKLINE] << 12) & MASK48) | ws_[D_MARKN] | 051);
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
    ws_[02001]  = STREAM_START;
    ws_[D_POOL] = POOL_START;
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
        ws_[D_POOL] = pool | 0377;
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
        ws_[02055] = DESCRIPTOR;
        Word type  = 0;
        switch (attr(m4_).head) {
        case 1: // 'ALGOL' :
        case 2: // 'CODE' :
            ws_[02054] = attr(m4_).head == 1 ? ALGOL_PROC : CODE_PROC;
            lex();
            m4_ = (m4_ - 050) & R15;
            if (m4_)
                bad();
            lex();
            continue;
        case 3:
            type = (DESCRIPTOR | STRING);
            break;
        case 4:
            type = (DESCRIPTOR | REAL);
            break;
        case 5:
            type = (DESCRIPTOR | INTEGER);
            break;
        case 6:
            type = (DESCRIPTOR | BOOLEAN);
            break;
        case 012:
            type = (DESCRIPTOR | BITS);
            break;
        case 7: // 'PROCEDURE'
            procedure(ws_[02055] | (DESCRIPTOR | PROCEDURE));
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
                emit((Word(m3_) << 12) | 060);
                if (!ws_[D_RENAME]) {
                    ws_[02136]    = ws_[NAMES + ws_[02056]];
                    ws_[D_RENAME] = apx(ws_[02136], NOT_BLANK);
                }
                dpush(ws_[02056] | (DESCRIPTOR | LABEL | SIMPLE));
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
            procedure(ws_[02055] | (DESCRIPTOR | PROCEDURE));
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
        switch (attr(m4_).statement) {
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
                emit((Word(m3_) << 12) | 060);
                dpush((a >> 12) | (DESCRIPTOR | LABEL | SIMPLE));
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
            io_statement(065);
            return;
        case 7:
            io_format(074);
            return;
        case 010:
            io_three(065, 066);
            return;
        case 011:
            io_three(074, 067);
            return;
        case 012: // 'PRINT'
            io_print(074, CONST_51);
            return;
        case 013:
            io_print(074, CONST_52);
            return;
        case 014:
            io_simple(070);
            return;
        case 015:
            io_simple(071);
            return;
        case 016:
            io_simple(072);
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
    emit(left | 022);
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
    if (at == BEGIN_BODY && attr(m4_).block) { // *0172B
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
        emit(030);
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
        emit(031);
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
    dpush(arx(saved, ONE) | BIT46);
    emit((Word(arx(m3_, ONE)) << 12) | 052);
    stmark();
}

// AG14655: the head of a FOR statement, up to DO.
void Pass1::for_head()
{
    lex();
    if (m4_)
        syntax_error(043);
    emit(055);
    Word var = D0();
    emit(D0() | 032);
    lex();
    if (D0() != 022)
        syntax_error(044);
    for (;;) { // *0162B: a FOR list element
        lex();
        expression(0);
        unsigned code = 0;
        switch (attr(m4_).for_element) {
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
        emit(033 | var);
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
        emit(036);
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
        emit((Word(arx(m3_, ONE)) << 12) | 037);
        stmark();
        return;
    }
}

// AG14262: the declarations of a block, up to its first statement.
void Pass1::declarations(unsigned self, Word &saved)
{
    for (;;) {
        Word type = 0;
        switch (attr(m4_).declaration) {
        case 0: // the statements
            return;
        case 1:
            type = (DESCRIPTOR | REAL);
            break;
        case 2:
            type = (DESCRIPTOR | INTEGER);
            break;
        case 3:
            type = (DESCRIPTOR | BOOLEAN);
            break;
        case 010:
            type = (DESCRIPTOR | STRING);
            break;
        case 011:
            type = (DESCRIPTOR | BITS);
            break;
        case 4: // 'PROCEDURE'
            procedure_declaration(self, (DESCRIPTOR | PROCEDURE));
            continue;
        case 5: // 'SWITCH'
            switch_declaration((DESCRIPTOR | LABEL | ARRAY));
            continue;
        case 6:
            syntax_error(050);
        case 7: // 'ARRAY'
            array_declaration((DESCRIPTOR | REAL | ARRAY), saved);
            continue;
        default:
            throw std::logic_error("pass 1: declaration dispatch");
        }
        // *0427B: a simple declaration
        lex();
        if (!m4_) {
            variables(type, saved);
        } else if (D0() == 042) {
            procedure_declaration(self, type | PROCEDURE);
        } else {
            if (D0() != 040)
                syntax_error(057);
            array_declaration(type | ARRAY, saved);
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
    saved |= HEADER;
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
        Word closer = arx(D0(), 025);
        if (D0() != 021 && D0() != 020)
            syntax_error(061);
        if (count & TOO_MANY)
            syntax_error(062);
        emit((Word(arx(m3_, ONE)) << 12) | count | 040);
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
        emit(dims | 0104);
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
// [1:1] (its block's header gets bit 48); the others get (BY_VALUE | SIMPLE).
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
            saved |= HEADER;
            dpush((D0() >> 12) | type);
            Word count = arx(0, COUNT);
            emit((Word(arx(m3_, ONE)) << 12) | count | 040);
            Word dims = COUNT;
            emit(CONST_1);
            emit(CONST_1);
            emit(dims | 0104);
            unsigned p = (m3_ + 1) & R15;
            ws_[p]     = ((dims << 8) & MASK48) | ws_[p];
        } else { // *0560B
            type |= (BY_VALUE | SIMPLE);
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
    emit(((start << 12) & MASK48) | count | 041);
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
            head  = start | BIT45;
            name  = (D0() >> 12) | type;
            emit(042);
            lex();
            if (D0() == 051) { // *0145B: no parameters
                name |= SIMPLE;
                count = 0;
                lex();
            } else {
                formals(start, count);
            }
            statement(self, PROC_END);
        }
        emit((Word(m3_) << 12) | 062); // L14642
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
        dpush((D0() >> 12) | (DESCRIPTOR | BIT46));
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
            if (ws_[f] & BY_VALUE)
                syntax_error(0116);
            ws_[f] = (ws_[f] & ~BIT46) | BY_VALUE;
            emit((Word(f) << 12) | 061);
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
        switch (attr(m4_).declaration) {
        case 0: // *0133B
            if (specified ^ count)
                syntax_error(0124);
            return;
        case 1:
        case 2:
        case 3:
        case 010:
        case 011: { // *0075B: a type, then maybe ARRAY or PROCEDURE
            static const Word types[] = { 0, REAL, INTEGER, BOOLEAN, 0, 0, 0, 0, STRING, BITS };
            spec                      = types[attr(m4_).declaration];
            lex();
            if (!m4_) {
                spec = (HEADER | SIMPLE) | spec;
                read = false;
            } else if (D0() == 040) {
                spec = ARRAY | spec;
            } else {
                if (D0() != 042)
                    syntax_error(0120);
                spec = PROCEDURE | spec;
            }
            break;
        }
        case 4:
            spec = PROCEDURE;
            break;
        case 5:
            spec = ARRAY;
            break;
        case 6:
            spec = (LABEL | SIMPLE);
            break;
        case 7:
            spec = (REAL | ARRAY);
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
            if (ws_[f] & SPECIFIED)
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
    if (attr(m4_).statement != 017) {
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
        emit(CONST_50);
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
            io_list(0100);
            return;
        }
        lex();
        io_list(0101);
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
        io_list(0103);
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
    io_list(063);
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
    io_list(063);
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
        if (attr(m4_).priority)
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
    switch (attr(m4_).expression) {
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
        unary(prio, 5, 4);
        return;
    case 4: // '+', '-'
        unary(prio, 7, arx(D0(), 033));
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
    emit(045);
}

// *0312B: binary operators of priority prio or more, after an operand.
void Pass1::operators(unsigned prio)
{
    for (;;) {
        unsigned p = attr(m4_).priority;
        if (!p)
            return;
        rau_ = 026;
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
        closer = 045;
    } else if (D0() == 021) {
        closer = 046;
        lex();
        expression(0);
        count = arx(count, COUNT); // L15063
        if (D0() == 050) {         // *0370B: a slice
            lex();
            expression(0);
            emit(id | 050);
            if (D0() != 046)
                syntax_error(033);
            lex();
            operators(prio);
            return;
        }
        if (D0() != 047) {
            if (D0() != 046)
                syntax_error(033);
            emit(count | id | 021);
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
    Word a = (closer | count | id) - 025;
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

namespace error {

// The texts of the syntax errors (pieces at 17106…) and the text of each
// error number (6 bits per error at 17166).
constexpr const char *OVERFLOW           = " OVERFLOW";
constexpr const char *STATEMENT          = "STATEMENT";
constexpr const char *DECLARATION        = "DECLARATION";
constexpr const char *STRING             = "STRING ";
constexpr const char *SYMBOL             = "SYMBOL";
constexpr const char *NUMBER             = "NUMBER";
constexpr const char *EXPRESSION         = "EXPRESSION";
constexpr const char *FOR_STATEMENT      = "FOR STATEMENT";
constexpr const char *IF_STATEMENT       = "IF STATEMENT";
constexpr const char *STRING_DECLARATION = "STRING DECLARATION";
constexpr const char *ARRAY_DECLARATION  = "ARRAY DECLARATION";
constexpr const char *SWITCH_DECLARATION = "SWITCH DECLARATION";
constexpr const char *PROCEDURE_HEAD     = "PROCEDURE HEAD";
constexpr const char *IO_STATEMENT       = "I/O STATEMENT";
constexpr const char *PROGRAM_HEAD       = "PROGRAM HEAD";
constexpr const char *PROGRAM_END        = "PROGRAM END";
constexpr const char *ILIST_OVERFLOW     = "ILIST OVERFLOW";
constexpr const char *CLIST_OVERFLOW     = "CLIST OVERFLOW";
constexpr const char *INTEGER_OVERFLOW   = "INTEGER OVERFLOW";
constexpr const char *REAL_OVERFLOW      = "REAL OVERFLOW";
constexpr const char *SLIST_OVERFLOW     = "SLIST OVERFLOW";
constexpr const char *DLIST_OVERFLOW     = "DLIST OVERFLOW";

// clang-format off
const std::array<const char *, 0150> TEXT = {
    OVERFLOW,           SYMBOL,             SYMBOL,             ILIST_OVERFLOW,     // 000
    SYMBOL,             SYMBOL,             SYMBOL,             CLIST_OVERFLOW,     // 004
    STRING,             NUMBER,             INTEGER_OVERFLOW,   REAL_OVERFLOW,      // 010
    SLIST_OVERFLOW,     DLIST_OVERFLOW,     NUMBER,             NUMBER,             // 014
    OVERFLOW,           OVERFLOW,           OVERFLOW,           EXPRESSION,         // 020
    EXPRESSION,         EXPRESSION,         EXPRESSION,         EXPRESSION,         // 024
    EXPRESSION,         EXPRESSION,         EXPRESSION,         EXPRESSION,         // 030
    OVERFLOW,           OVERFLOW,           STATEMENT,          IF_STATEMENT,       // 034
    IF_STATEMENT,       IF_STATEMENT,       STATEMENT,          FOR_STATEMENT,      // 040
    FOR_STATEMENT,      FOR_STATEMENT,      FOR_STATEMENT,      FOR_STATEMENT,      // 044
    DECLARATION,        STRING_DECLARATION, STRING_DECLARATION, STRING_DECLARATION, // 050
    STRING_DECLARATION, STRING_DECLARATION, STATEMENT,          DECLARATION,        // 054
    ARRAY_DECLARATION,  ARRAY_DECLARATION,  ARRAY_DECLARATION,  ARRAY_DECLARATION,  // 060
    ARRAY_DECLARATION,  ARRAY_DECLARATION,  DECLARATION,        DECLARATION,        // 064
    DECLARATION,        SWITCH_DECLARATION, SWITCH_DECLARATION, SWITCH_DECLARATION, // 070
    SWITCH_DECLARATION, OVERFLOW,           OVERFLOW,           OVERFLOW,           // 074
    OVERFLOW,           OVERFLOW,           OVERFLOW,           OVERFLOW,           // 100
    OVERFLOW,           OVERFLOW,           PROCEDURE_HEAD,     PROCEDURE_HEAD,     // 104
    PROCEDURE_HEAD,     PROCEDURE_HEAD,     PROCEDURE_HEAD,     PROCEDURE_HEAD,     // 110
    PROCEDURE_HEAD,     PROCEDURE_HEAD,     PROCEDURE_HEAD,     PROCEDURE_HEAD,     // 114
    PROCEDURE_HEAD,     PROCEDURE_HEAD,     PROCEDURE_HEAD,     PROCEDURE_HEAD,     // 120
    PROCEDURE_HEAD,     OVERFLOW,           OVERFLOW,           OVERFLOW,           // 124
    OVERFLOW,           OVERFLOW,           IO_STATEMENT,       IO_STATEMENT,       // 130
    IO_STATEMENT,       IO_STATEMENT,       IO_STATEMENT,       PROGRAM_HEAD,       // 134
    OVERFLOW,           OVERFLOW,           OVERFLOW,           PROGRAM_END,        // 140
    OVERFLOW,           OVERFLOW,           OVERFLOW,           OVERFLOW,           // 144
};
// clang-format on

} // namespace error

// The line of a syntax error: ERROR nn and its text.
void Pass1::print_error(unsigned n)
{
    if (!ws_[D_LIST] && ws_[D_LINE] != ws_[D_PRINTED]) {
        print_card();
        ws_[D_PRINTED] = ws_[D_LINE];
    }
    char num[16];
    std::snprintf(num, sizeof(num), "ERROR %02u    ", n);
    print_line(num + std::string(error::TEXT.at(n)));
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
