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
    std::vector<Card> cards_;
    size_t next_card_{};
    unsigned pool_sectors_{}; // sectors of the string pool written to drum

    // Index registers of the original where they carry state.
    unsigned m4_{};     // symbol code
    unsigned m5_{};     // card word, -11…0
    unsigned m6_{};     // character in the word, -6…-1; 0: past the end
    unsigned rau_{ 2 }; // mode of the arithmetic unit
    unsigned m3_{};     // D-list pointer

    std::vector<Word> stk_; // the stack of M17
    Word emit_item_{};      // 15144
    Stream out_;

    static constexpr unsigned RET_DRIVER = 0;

    void parse();
    void emit(Word a);
    void stmark();
    void setres(unsigned addr);
    void dpush(Word a);
    unsigned find_formal();
    void print_line(const std::string &line);
    void print_error(unsigned n);

    unsigned nextch(bool advance = true);
    void read_card();
    void print_card();
    unsigned intern(Word value, unsigned slot);
    void store_string_word();
    void pool_put(Word w);
    [[noreturn]] void error(unsigned n);
    [[noreturn]] static void common();
    static Word chclass(unsigned c);
};

#endif
