//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PASS1_H
#define ALGOL_PASS1_H

#include <stdexcept>
#include <string>
#include <vector>

#include "arith.h"
#include "card.h"
#include "passes.h"

//
// Pass 1 (G13666, G14504): reads the cards, lexes, checks the syntax and
// writes the first stream. The lexer and the parser share their state in the
// workspace cells 02000…, as the original does; see doc/passes.md.
//
class Pass1 {
public:
    explicit Pass1(Context &ctx);

    // Whole pass: the stream of pass 1.
    Stream run();

    //
    // Lexer (AG15414). Leaves the symbol word in 02000 and its code in M4:
    // a delimiter code, or 0 for an identifier (slot << 12), 024/023/0105
    // for a constant (slot << 12 | code), 025 for a string.
    //
    void lex();
    unsigned symbol() const { return m4_; }
    Word symbol_word() const { return ws_[D_SYM]; }

    // A syntax error found by the lexer or the parser (AG17015).
    struct SyntaxError {
        unsigned number;
    };

    // No more cards before 'EOP' (HET 'EOP' !).
    struct NoEop {};

    // The compilation ends here (AG01716).
    struct Finish {};

    // Workspace cells (AD02000+n).
    static constexpr unsigned D_SYM      = 02000; // symbol word
    static constexpr unsigned D_CONST    = 02003; // next free constant slot
    static constexpr unsigned D_CHAR     = 02004; // current character
    static constexpr unsigned D_AFTEREND = 02005; // skipping the text after END
    static constexpr unsigned CARD       = 02010; // the card, 14 words
    static constexpr unsigned D_WORD     = 02026; // current card word
    static constexpr unsigned D_KEYWORD  = 02027; // a quote started the name
    static constexpr unsigned D_BLANK    = 02030; // character to skip
    static constexpr unsigned D_BLANKS   = 02031; // word to skip
    static constexpr unsigned D_NAME     = 02032; // name being read, ISO
    static constexpr unsigned D_WARNED   = 02033; // WARNING SYMBOL AFTER END printed
    static constexpr unsigned D_POINT    = 02034; // number: '.' seen
    static constexpr unsigned D_EXP      = 02035; // number: exponent seen
    static constexpr unsigned D_LAST     = 02036; // number: last character
    static constexpr unsigned D_LOST     = 02037; // number: digits beyond precision
    static constexpr unsigned D_VALUE    = 02040; // number: value of the digits
    static constexpr unsigned D_NEXT     = 02041; // number: next value
    static constexpr unsigned D_MANT     = 02042; // number: mantissa before the exponent
    static constexpr unsigned D_EXPNEG   = 02043; // number: negative exponent
    static constexpr unsigned D_FRAC     = 02044; // number: starts with '.'
    static constexpr unsigned D_POWER    = 02045; // number: scaled value
    static constexpr unsigned D_KEY      = 02046; // constant being interned
    static constexpr unsigned D_SEMI     = 02047; // ',' after '.,' means ';'
    static constexpr unsigned D_STRINGS  = 02050; // string pool end << 12
    static constexpr unsigned D_LINE     = 02060; // line counter
    static constexpr unsigned D_PRINTED  = 02061; // last line printed
    static constexpr unsigned D_LIST     = 02121; // listing on
    static constexpr unsigned D_POOL     = 02002; // string pool cursor (G27220)
    static constexpr unsigned NAMES      = 02144; // name/constant table
    static constexpr unsigned D_RESPC    = 02106; // resume after an error: address
    static constexpr unsigned D_RESSP    = 02107; // … and stack depth
    static constexpr unsigned D_MARKLINE = 02122; // line of the last statement marker
    static constexpr unsigned D_MARKN    = 02123; // its count << 7
    static constexpr unsigned D_RENAME   = 02137; // name of the first label, TEXT
    static constexpr unsigned CARDS      = 01741; // card count, integer

private:
    Context &ctx_;
    Workspace &ws_;
    std::vector<Word> &strings_; // the string pool on drum
    std::vector<Card> cards_;
    size_t next_card_{};

    // Index registers of the original where they carry state.
    unsigned m4_{};        // symbol code
    unsigned m5_{};        // card word, -11…0
    unsigned m6_{};        // character in the word, -6…-1; 0: past the end
    unsigned rau_{ 2 };    // mode of the arithmetic unit
    unsigned m3_{ 05753 }; // D-list pointer, set by AG13666

    Word cells_[022]{}; // 16111-16132: cells of the 'COMMON' reader in the code

    Word emit_item_{}; // 15144
    Stream out_;

    // Where the parser goes on after a syntax error: a label in a frame.
    struct Checkpoint {
        unsigned frame{};
        unsigned label{};
    };
    struct Resume;
    Checkpoint checkpoint_{};
    unsigned frames_{};

    Word D0() const { return ws_[D_SYM]; }
    template <class Body>
    void frame(Body body);
    Checkpoint recover(unsigned n);
    [[noreturn]] static void syntax_error(unsigned n);
    [[noreturn]] void skip_to_eop();
    [[noreturn]] void eop_inside();
    void emit(Word a);
    void stmark();
    void setres(unsigned owner, unsigned label);
    void dpush(Word a);
    unsigned find_formal(Word start);
    void print_line(const std::string &line);
    void print_error(unsigned n);

    // The parser (doc/passes.md).
    void program();
    void program_head(unsigned self, bool resumed);
    void statement(unsigned caller, unsigned cont);
    void statement_body(unsigned self, Word &saved);
    void assignment();
    void compound(unsigned self, unsigned at, Word &saved);
    void if_statement(unsigned self, unsigned at);
    void for_statement(unsigned self, unsigned at, Word &saved);
    void for_head();
    void for_end(Word saved);
    void block(unsigned self, unsigned at, Word &saved);
    void declarations(unsigned self, Word &saved);
    void procedure_declaration(unsigned self, Word type);
    void array_declaration(Word type, Word &saved);
    void variables(Word type, Word &saved);
    void switch_declaration(Word type);
    void procedure(Word type);
    void formals(Word start, Word &count);
    void io_list(Word first);
    void io_statement(Word item);
    void io_format(Word item);
    void io_print(Word item, Word first);
    void io_three(Word item, Word first);
    void io_simple(Word item);
    void io_element();
    void expression(unsigned prio);
    void conditional(unsigned prio);
    void operators(unsigned prio);
    void unary(unsigned prio, unsigned p, Word op);
    void designator(unsigned prio, Word id);

    unsigned nextch(bool advance = true);
    void read_card();
    void print_card();
    unsigned intern(Word value, unsigned slot);
    void store_string_word();
    void pool_put(Word w);
    [[noreturn]] void error(unsigned n);
    void common();
    [[noreturn]] void common_error(unsigned n);
    unsigned common_slot(Word name);
    Word &cell(unsigned addr);
    void common_pack(Word &w, unsigned c);
    static unsigned unicode_operator(unsigned c);
};

#endif
