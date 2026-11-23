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
    COMMON, // 'common' /block/ elements; — a whole declaration

    // Operators.
    PLUS,
    MINUS,
    TIMES,
    SLASH,
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

// Text of a symbol, for messages: 'begin', ':=', identifier, …
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
    int line{};
    std::string text; // identifier as written, or the characters of a string (UTF-8)
    int64_t ival{};   // INTEGER_CONST
    double rval{};    // REAL_CONST
    uint64_t bits{};  // BITS_CONST
    std::shared_ptr<CommonBlock> common;
};

//
// The lexer: symbols from the UTF-8 lines of the source ('′' is a quote).
// Blanks and line ends are ignored outside strings. Keywords are in quotes,
// in any case; names keep their spelling and case. Lexical errors are
// reported and give an ERROR symbol; it goes on after them.
//
class Lexer {
public:
    // on_line is called with the number and text of each line read.
    Lexer(const std::string &source, Diagnostics &diag,
          std::function<void(int, const std::string &)> on_line = {});

    Token next();

    int line() const { return line_; }

private:
    struct EndOfInput {};

    std::vector<std::string> lines_;
    Diagnostics &diag_;
    std::function<void(int, const std::string &)> on_line_;
    size_t next_line_{};
    std::u32string text_; // the current line and '\n'
    size_t col_{};
    char32_t ch_{};
    bool skip_blanks_{ true };
    int line_{};
    bool after_end_{};
    bool warned_{};
    bool semicolon_{}; // a number ended with '.,'
    bool eof_{};
    int string_line_{}; // where an open string began
    Sym last_{ Sym::SEMICOLON };

    char32_t fetch();
    char32_t advance();
    void step() { col_++; }
    void skip_line() { col_ = text_.size(); }
    std::string word(); // letters and digits from ch_

    Token make(Sym s) const;
    Token error(const std::string &message);
    Token scan();
    Token name_or_keyword(bool quoted);
    Token string();
    Token octal();
    Token number(bool point); // point: after a leading '.'
    Token after_end();
    Token common();
    void comment();
};

#endif
