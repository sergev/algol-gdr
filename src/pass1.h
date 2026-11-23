//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_PASS1_H
#define ALGOL_PASS1_H

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "arith.h"
#include "card.h"
#include "passes.h"

//
// Pass 1 (G13666, G14504): reads the cards, lexes, checks the syntax and
// writes the first stream. The lexer and the parser share their state in
// members named after the cells 02000… of the original; see doc/passes.md.
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
    Word symbol_word() const { return sym_; }

    // A syntax error found by the lexer or the parser (AG17015).
    struct SyntaxError {
        unsigned number;
    };

    // No more cards before 'EOP' (HET 'EOP' !).
    struct NoEop {};

    // The compilation ends here (AG01716).
    struct Finish {};

private:
    Context &ctx_;
    Tables &tables_;
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

    // Cells of the original at 02000… (D-list and name table in tables_).
    Word sym_{ 051 };              // 02000: symbol word
    Word cursor_{};                // 02001: stream write cursor
    Word pool_{};                  // 02002: string pool cursor (G27220)
    Word char_{ 01634 << 12 };     // 02004: current character
    Word afterend_{};              // 02005: skipping the text after END
    Card card_{};                  // 02010: the card, 14 words
    Word word_{};                  // 02026: current card word
    Word keyword_{};               // 02027: a quote started the name
    Word blank_{ 040 };            // 02030: character to skip
    Word blanks_{};                // 02031: word to skip
    Word name_{};                  // 02032: name being read, ISO
    Word warned_{};                // 02033: WARNING SYMBOL AFTER END printed
    Word point_{};                 // 02034: number: '.' seen
    Word exp_{};                   // 02035: number: exponent seen
    Word last_{};                  // 02036: number: last character
    Word lost_{};                  // 02037: number: digits beyond precision
    Word value_{};                 // 02040: number: value of the digits
    Word next_{};                  // 02041: number: next value
    Word mant_{};                  // 02042: number: mantissa before the exponent
    Word expneg_{};                // 02043: number: negative exponent
    Word frac_{};                  // 02044: number: starts with '.'
    Word power_{};                 // 02045: number: scaled value
    Word key_{};                   // 02046: constant being interned
    Word semi_{};                  // 02047: ',' after '.,' means ';'
    Word pool_end_{ 01634 << 12 }; // 02050: string pool end << 12
    Word io_item_{};               // 02052: item of an I/O statement
    Word head_kind_{};             // 02054: 'ALGOL' or 'CODE' before the program
    Word head_desc_{};             // 02055: descriptor bits of a declaration before the program
    Word head_slot_{};             // 02056: its name slot
    Word line_{};                  // 02060: line counter
    Word printed_{};               // 02061: last line printed
    Word io_done_{};               // 02117: the I/O element wrote the item
    Word list_{};                  // 02121: listing on
    Word markline_{};              // 02122: line of the last statement marker
    Word markn_{};                 // 02123: its count << 7
    Word label_name_{};            // 02136: name of the first label
    Word rename_{};                // 02137: … in TEXT
    Word card_count_{};            // 01741: card count, integer
    std::array<Word, 0400> pool_page_{}; // 04000-04377: the string pool page
    Stream out_;

    // Where the parser goes on after a syntax error: a label in a frame.
    struct Checkpoint {
        unsigned frame{};
        unsigned label{};
    };
    struct Resume;
    Checkpoint checkpoint_{};
    unsigned frames_{};

    Word D0() const { return sym_; }
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
