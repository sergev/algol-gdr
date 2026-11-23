//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_LEXER_H
#define ALGOL_LEXER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "diag.h"

class Diagnostics;

//
// Symbols of the language.
//
enum class Sym {
    // Values.
    IDENT,
    INTEGER_CONST,
    REAL_CONST,
    BITS_CONST, // an octal constant 'ddd'
    STRING_CONST,
    COMMON, // _common /block/ elements; — a whole declaration

    // Operators.
    PLUS,
    MINUS,
    TIMES,
    SLASH,
    INTDIV, // ÷ _div
    POWER,
    LT,
    LE,
    EQ,
    NE,
    GE,
    GT,
    NOT,
    AND,
    OR,
    IMPL,
    EQUIV,

    // Punctuation.
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    COMMA,
    COLON,
    SEMICOLON,
    ASSIGN,

    // Keywords.
    GOTO,
    IF,
    THEN,
    ELSE,
    FOR,
    DO,
    STEP,
    UNTIL,
    WHILE,
    BEGIN,
    END,
    OWN,
    BOOLEAN,
    INTEGER,
    REAL,
    STRING,
    BITS,
    ARRAY,
    SWITCH,
    PROCEDURE,
    LABEL,
    VALUE,
    TRUE,
    FALSE,
    ALGOL,
    CODE,
    READ,
    PRINT,
    PUNCH,
    WRITE,
    DECODE,
    ENCODE,
    REWIND,
    BACKSPACE,
    ENDFILE,
    EOP,

    ERROR,       // a lexical error, reported
    END_OF_FILE, // no more lines
};

// Text of a symbol, for messages: '_begin', ':=', identifier, …
const char *to_string(Sym s);

struct CommonBlock {
    struct Element {
        std::string name;
        std::vector<int> dims; // empty for a variable
        int size{ 1 };         // words
    };
    std::string name;
    std::vector<Element> elements;
};

struct Token {
    Sym sym{ Sym::END_OF_FILE };
    Pos line{};       // and column
    std::string text; // identifier as written, or the characters of a string (UTF-8)
    int64_t ival{};   // INTEGER_CONST
    double rval{};    // REAL_CONST
    uint64_t bits{};  // BITS_CONST
    std::shared_ptr<CommonBlock> common;
};

//
// The lexer: symbols from the UTF-8 lines of the source ('′' is a quote).
// Keywords are '_' and a word, in any case: _begin; a blank ends them.
// Elsewhere outside strings blanks and line ends are ignored, also inside
// names and numbers. Names keep their spelling and case. Lexical errors are reported
// and give an ERROR symbol; it goes on after them.
//
class Lexer {
public:
    // on_line is called with the number and text of each line read.
    Lexer(const std::string &source, Diagnostics &diag,
          std::function<void(int, const std::string &)> on_line = {});

    Token next();

    int line() const { return line_; }
    bool publication() const { return publication_; }

private:
    struct EndOfInput {};

    // While set, the end of the input reads as the character 0.
    struct SoftEof {
        explicit SoftEof(bool &flag) : flag_(flag) { flag_ = true; }
        ~SoftEof() { flag_ = false; }
        SoftEof(const SoftEof &)            = delete;
        SoftEof &operator=(const SoftEof &) = delete;
        bool &flag_;
    };

    std::vector<std::string> lines_;
    Diagnostics &diag_;
    std::function<void(int, const std::string &)> on_line_;
    size_t next_line_{};
    std::u32string text_; // the current line and '\n'
    size_t col_{};
    char32_t ch_{};
    bool skip_blanks_{ true };
    int line_{};
    Pos start_{}; // of the symbol being read
    bool after_end_{};
    bool warned_{};
    bool semicolon_{}; // a number ended with '.,'
    bool eof_{};
    bool soft_eof_{};    // reading a name or number: SoftEof
    bool publication_{}; // keywords underlined: b̲e̲g̲i̲n̲, strings in `…'
    int string_line_{};  // where an open string began
    Sym last_{ Sym::SEMICOLON };

    char32_t fetch();
    char32_t advance();
    void step() { col_++; }
    std::string word(bool raw);

    Token make(Sym s) const;
    Token error(const std::string &message);
    Token scan();
    Token name();
    Token keyword_symbol();
    Token keyword_token(const std::string &kw, Pos start);
    bool underlined() const;
    std::string underlined_word();
    Token quoted_string(char32_t close);
    Token rejected_string(bool double_quoted);
    Token octal();
    Token number(bool point); // point: after a leading '.'
    Token after_end();
    Token common();
    void comment();
};

#endif
