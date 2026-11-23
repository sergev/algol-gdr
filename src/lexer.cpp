//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The lexer. It reads the cards in KOI-7 as the original does (see
// doc/encoding.md) and gives names and strings in UTF-8.
//
#include "lexer.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

#include "card.h"
#include "diag.h"
#include "listing.h"

namespace {

constexpr unsigned TEXT_COLUMNS = 72;
constexpr unsigned QUOTE        = '\'';
constexpr unsigned TEN          = 027; // ⏨
constexpr unsigned TEN2         = 044; // ◇, also an exponent sign

bool is_letter(unsigned c)
{
    return (c >= 0100 && c <= 0132) || (c >= 0140 && c <= 0176);
}

bool is_digit(unsigned c)
{
    return c >= '0' && c <= '9';
}

bool is_octal(unsigned c)
{
    return c >= '0' && c <= '7';
}

// The symbol of a one-character delimiter; ERROR for others.
Sym single(unsigned c)
{
    switch (c) {
    case 020: // ∨
    case 036: // ∨ (from Unicode)
        return Sym::OR;
    case 021: // ∧
    case '&':
        return Sym::AND;
    case 022: // ⊃
    case 034:
        return Sym::IMPL;
    case 023: // ¬
    case 037:
        return Sym::NOT;
    case 024: // ≡
    case 035:
        return Sym::EQUIV;
    case 030: // ≠
        return Sym::NE;
    case 031: // ≤
    case 016:
        return Sym::LE;
    case 032: // ≥
    case 017:
        return Sym::GE;
    case 006: // ×
    case 0134:
        return Sym::TIMES;
    case 026: // ↑
    case 0136:
        return Sym::POWER;
    case ')':
        return Sym::RPAREN;
    case '+':
        return Sym::PLUS;
    case '-':
        return Sym::MINUS;
    case ';':
        return Sym::SEMICOLON;
    case '<':
        return Sym::LT;
    case '=':
        return Sym::EQ;
    case '>':
        return Sym::GT;
    case '[':
        return Sym::LBRACKET;
    case ']':
        return Sym::RBRACKET;
    default:
        return Sym::ERROR;
    }
}

// Keywords by their first 6 characters.
const std::map<std::string, Sym> &keywords()
{
    static const std::map<std::string, Sym> table = [] {
        static const std::pair<const char *, Sym> list[] = {
            { "ALGOL", Sym::ALGOL },     { "AND", Sym::AND },
            { "ARRAY", Sym::ARRAY },     { "BACKSPACE", Sym::BACKSPACE },
            { "BEGIN", Sym::BEGIN },     { "BITS", Sym::BITS },
            { "BOOLEAN", Sym::BOOLEAN }, { "CODE", Sym::CODE },
            { "COMMON", Sym::COMMON },   { "DECODE", Sym::DECODE },
            { "DO", Sym::DO },           { "ELSE", Sym::ELSE },
            { "ENCODE", Sym::ENCODE },   { "END", Sym::END },
            { "ENDFILE", Sym::ENDFILE }, { "EOP", Sym::EOP },
            { "EQ", Sym::EQ },           { "EQUAL", Sym::EQ },
            { "EQUIV", Sym::EQUIV },     { "FALSE", Sym::FALSE },
            { "FOR", Sym::FOR },         { "GE", Sym::GE },
            { "GOTO", Sym::GOTO },       { "GREATER", Sym::GT },
            { "GT", Sym::GT },           { "IF", Sym::IF },
            { "IMPL", Sym::IMPL },       { "INTEGER", Sym::INTEGER },
            { "LABEL", Sym::LABEL },     { "LE", Sym::LE },
            { "LESS", Sym::LT },         { "LT", Sym::LT },
            { "NE", Sym::NE },           { "NOT", Sym::NOT },
            { "NOTEQUAL", Sym::NE },     { "NOTGREATER", Sym::LE },
            { "NOTLESS", Sym::GE },      { "OR", Sym::OR },
            { "OWN", Sym::OWN },         { "POWER", Sym::POWER },
            { "PRINT", Sym::PRINT },     { "PROCEDURE", Sym::PROCEDURE },
            { "PUNCH", Sym::PUNCH },     { "READ", Sym::READ },
            { "REAL", Sym::REAL },       { "REWIND", Sym::REWIND },
            { "STEP", Sym::STEP },       { "STRING", Sym::STRING },
            { "SWITCH", Sym::SWITCH },   { "THEN", Sym::THEN },
            { "TRUE", Sym::TRUE },       { "UNTIL", Sym::UNTIL },
            { "VALUE", Sym::VALUE },     { "WHILE", Sym::WHILE },
            { "WRITE", Sym::WRITE },
        };
        std::map<std::string, Sym> t;
        for (const auto &[name, sym] : list)
            t.emplace(std::string(name).substr(0, 6), sym);
        t.emplace("COMMEN", Sym::ERROR); // 'COMMENT', handled by the lexer
        return t;
    }();
    return table;
}

std::string to_utf8(const std::string &koi)
{
    std::string s;
    for (unsigned char c : koi)
        s += listing_char(c);
    return s;
}

} // namespace

const char *to_string(Sym s)
{
    static const char *const names[] = {
        "identifier",    "integer constant",
        "real constant", "octal constant",
        "string",        "'COMMON'",
        "'+'",           "'-'",
        "'*'",           "'/'",
        "'**'",          "'<'",
        "'<='",          "'='",
        "'!='",          "'>='",
        "'>'",           "'NOT'",
        "'AND'",         "'OR'",
        "'IMPL'",        "'EQUIV'",
        "'('",           "')'",
        "'['",           "']'",
        "','",           "':'",
        "';'",           "':='",
        "'GOTO'",        "'IF'",
        "'THEN'",        "'ELSE'",
        "'FOR'",         "'DO'",
        "'STEP'",        "'UNTIL'",
        "'WHILE'",       "'BEGIN'",
        "'END'",         "'OWN'",
        "'BOOLEAN'",     "'INTEGER'",
        "'REAL'",        "'STRING'",
        "'BITS'",        "'ARRAY'",
        "'SWITCH'",      "'PROCEDURE'",
        "'LABEL'",       "'VALUE'",
        "'TRUE'",        "'FALSE'",
        "'ALGOL'",       "'CODE'",
        "'READ'",        "'PRINT'",
        "'PUNCH'",       "'WRITE'",
        "'DECODE'",      "'ENCODE'",
        "'REWIND'",      "'BACKSPACE'",
        "'ENDFILE'",     "'EOP'",
        "error",         "end of file",
    };
    static_assert(sizeof(names) / sizeof(names[0]) == size_t(Sym::END_OF_FILE) + 1);
    return names[int(s)];
}

std::string name_key(const std::string &name)
{
    // The first 6 characters, UTF-8.
    size_t i = 0;
    for (int n = 0; n < 6 && i < name.size(); n++) {
        i++;
        while (i < name.size() && (name[i] & 0xc0) == 0x80)
            i++;
    }
    return name.substr(0, i);
}

Lexer::Lexer(const std::string &source, Diagnostics &diag,
             std::function<void(int, const std::string &)> on_card)
    : diag_(diag), on_card_(std::move(on_card))
{
    size_t pos = 0;
    while (pos < source.size()) {
        size_t end = source.find('\n', pos);
        if (end == std::string::npos)
            end = source.size();
        std::string card = line_to_koi7(source.substr(pos, end - pos));
        card.resize(CARD_COLUMNS, ' ');
        cards_.push_back(card);
        pos = end + 1;
    }
}

// The current character, from the next card when this one is done; blanks
// skipped unless in a string.
unsigned Lexer::fetch()
{
    for (;;) {
        if (col_ >= TEXT_COLUMNS) {
            if (next_card_ >= cards_.size() || cards_[next_card_].compare(0, 6, "*END F") == 0)
                throw EndOfInput{};
            card_ = cards_[next_card_++];
            line_++;
            col_ = 0;
            if (on_card_) {
                std::string text;
                for (unsigned char c : card_) {
                    if (c == 3)
                        break;
                    text += listing_char(c);
                }
                on_card_(line_, text);
            }
        }
        unsigned c = (unsigned char)card_[col_];
        if (skip_blanks_ && c == ' ') {
            col_++;
            continue;
        }
        return ch_ = c;
    }
}

unsigned Lexer::advance()
{
    step();
    return fetch();
}

Token Lexer::make(Sym s) const
{
    Token t;
    t.sym  = s;
    t.line = line_;
    return t;
}

Token Lexer::error(const std::string &message)
{
    diag_.error(line_, message);
    return make(Sym::ERROR);
}

Token Lexer::next()
{
    if (eof_)
        return make(Sym::END_OF_FILE);
    try {
        Token t = scan();
        last_   = t.sym;
        return t;
    } catch (const EndOfInput &) {
        eof_ = true;
        return make(Sym::END_OF_FILE);
    }
}

Token Lexer::scan()
{
    unsigned c = fetch();
    if (after_end_)
        return after_end();
    if (is_letter(c))
        return name_or_keyword(false);
    if (is_digit(c) || c == TEN || c == TEN2)
        return number(false);
    switch (c) {
    case QUOTE:
        c = advance();
        if (c == QUOTE)
            return string();
        if (is_letter(c))
            return name_or_keyword(true);
        if (is_digit(c))
            return octal();
        if (c == '(' && advance() == QUOTE)
            return string();
        return error("Unexpected character after a quote");
    case '"':
        return string();
    case '.':
        if (advance() == ',') {
            step();
            return make(Sym::SEMICOLON);
        }
        return number(true);
    case ':':
        if (advance() != '=')
            return make(Sym::COLON);
        step();
        return make(Sym::ASSIGN);
    case '*':
        if (advance() != '*')
            return make(Sym::TIMES);
        step();
        return make(Sym::POWER);
    case '(':
        if (advance() != '/')
            return make(Sym::LPAREN);
        step();
        return make(Sym::LBRACKET);
    case '/':
        if (advance() != ')')
            return make(Sym::SLASH);
        step();
        return make(Sym::RBRACKET);
    case ',':
        step();
        if (semicolon_) {
            semicolon_ = false;
            return make(Sym::SEMICOLON);
        }
        return make(Sym::COMMA);
    default: {
        step();
        Sym s = single(c);
        if (s == Sym::ERROR)
            return error("Illegal character '" + std::string(listing_char(c)) + "'");
        return make(s);
    }
    }
}

// A name of letters and digits; in quotes, a keyword.
Token Lexer::name_or_keyword(bool quoted)
{
    int start = line_;
    std::string koi(1, char(ch_));
    for (;;) {
        unsigned c = advance();
        if (!is_letter(c) && !is_digit(c))
            break;
        koi += char(c);
    }
    std::string name = to_utf8(koi);
    if (!quoted) {
        Token t = make(Sym::IDENT);
        t.line  = start;
        t.text  = name;
        return t;
    }
    if (ch_ != QUOTE)
        return error("Missing closing quote of '" + name + "'");
    step();
    auto it = keywords().find(koi.substr(0, 6));
    if (it == keywords().end())
        return error("Unknown keyword '" + name + "'");
    if (koi.substr(0, 6) == "COMMEN") {
        comment();
        return scan();
    }
    switch (it->second) {
    case Sym::END:
        after_end_ = true;
        break;
    case Sym::COMMON:
        return common();
    default:
        break;
    }
    Token t = make(it->second);
    t.line  = start;
    return t;
}

// 'COMMENT' … ';' (or '.,').
void Lexer::comment()
{
    if (last_ != Sym::SEMICOLON && last_ != Sym::BEGIN)
        diag_.error(line_, "'COMMENT' must follow ';' or 'BEGIN'");
    unsigned c = fetch();
    for (;;) {
        if (c == ';')
            break;
        if (c != '.') {
            c = advance();
            continue;
        }
        c = advance();
        if (c == ',')
            break;
    }
    step();
}

// The text after END, up to ';', 'END', 'ELSE' or 'EOP'. Letters and digits
// are a comment; anything else gets a warning.
Token Lexer::after_end()
{
    unsigned c = ch_;
    for (;;) {
        if (c == ';')
            break;
        if (c == '.') {
            c = advance();
            if (c == ',')
                break;
        } else if (is_letter(c) || is_digit(c)) {
            c = advance();
            continue;
        } else if (c == QUOTE) {
            c = advance();
            if (is_letter(c)) {
                int start = line_;
                std::string koi(1, char(c));
                while (is_letter(c = advance()) || is_digit(c))
                    koi += char(c);
                if (c == QUOTE) {
                    auto it = keywords().find(koi.substr(0, 6));
                    if (it != keywords().end() &&
                        (it->second == Sym::END || it->second == Sym::ELSE ||
                         it->second == Sym::EOP)) {
                        step();
                        warned_ = false;
                        if (it->second != Sym::END)
                            after_end_ = false;
                        Token t = make(it->second);
                        t.line  = start;
                        return t;
                    }
                }
            }
        }
        if (!warned_) {
            diag_.warning(line_, "Text after 'END' is skipped up to ';', 'END', 'ELSE' or 'EOP'");
            warned_ = true;
        }
        if (c == ';')
            break;
        if (c == '.')
            continue;
        c = advance();
    }
    after_end_ = false;
    warned_    = false;
    step();
    return make(Sym::SEMICOLON);
}

// A string: ''…'' or '('…')'; 'ddd' inside is a character by its octal code.
Token Lexer::string()
{
    int start    = line_;
    skip_blanks_ = false;
    std::string koi;
    for (;;) {
        unsigned c = advance();
        if (c != QUOTE) {
            koi += char(c);
            continue;
        }
        skip_blanks_ = true;
        c            = advance();
        if (c == QUOTE)
            break;
        if (c == ')') {
            if (advance() == QUOTE)
                break;
            col_ = TEXT_COLUMNS;
            return error("Bad end of a string");
        }
        unsigned v = 0;
        for (int n = 0;; n++) {
            if (n == 3 || !is_octal(ch_)) {
                col_ = TEXT_COLUMNS;
                return error("Bad character code in a string");
            }
            v = v << 3 | (ch_ & 7);
            if (advance() == QUOTE)
                break;
        }
        if (v > 0377) {
            col_ = TEXT_COLUMNS;
            return error("Bad character code in a string");
        }
        koi += char(v);
        skip_blanks_ = false;
    }
    step();
    Token t = make(Sym::STRING_CONST);
    t.line  = start;
    t.text  = to_utf8(koi);
    return t;
}

// An octal constant: 'ddd', up to 16 digits.
Token Lexer::octal()
{
    uint64_t v = 0;
    for (int n = 1;; n++) {
        if (!is_octal(ch_) || n > 16) {
            const char *message = n > 16 ? "Octal constant is too long" : "Bad octal digit";
            while (ch_ != QUOTE && col_ < TEXT_COLUMNS)
                advance();
            step();
            return error(message);
        }
        v = v << 3 | (ch_ & 7);
        if (advance() == QUOTE)
            break;
    }
    step();
    Token t = make(Sym::BITS_CONST);
    t.bits  = v;
    return t;
}

//
// A number: digits, '.' and a fraction, ⏨ and an exponent. A number ending in
// '.,' is followed by ';'. With a point or an exponent it is real.
//
Token Lexer::number(bool point)
{
    int start = line_;
    std::string whole, fraction, exponent;
    bool exp = false, negative = false, digit = false, bad = point && !is_digit(ch_);
    for (unsigned c = ch_; !bad;) {
        if (is_digit(c)) {
            (exp ? exponent : point ? fraction : whole) += char(c);
            digit = true;
            c     = advance();
            continue;
        }
        if (c == TEN || c == TEN2) {
            if (exp || (!digit && point)) {
                bad = true;
                break;
            }
            if (!digit)
                whole = "1";
            exp = true;
            c   = advance();
            if (c == '-' || c == '+') {
                negative = c == '-';
                c        = advance();
            } else if (!is_digit(c)) {
                bad = true;
                break;
            }
            digit = false;
            continue;
        }
        if (c == '.') {
            c = advance();
            if (c == ',') {
                semicolon_ = true;
                break;
            }
            if (point || exp) {
                bad = true;
                break;
            }
            point = true;
            digit = false;
            continue;
        }
        break;
    }
    if (bad || !digit) {
        while (is_digit(ch_) || ch_ == '.')
            advance();
        Token t = error("Malformed number");
        t.line  = start;
        return t;
    }
    Token t = make(Sym::INTEGER_CONST);
    t.line  = start;
    if (!point && !exp) {
        errno       = 0;
        char *end   = nullptr;
        long long v = std::strtoll(whole.c_str(), &end, 10);
        if (errno == ERANGE)
            return error("Integer constant is too large");
        t.ival = v;
        return t;
    }
    std::string text = (whole.empty() ? "0" : whole) + "." + (fraction.empty() ? "0" : fraction);
    if (exp)
        text += std::string("e") + (negative ? "-" : "") + exponent;
    t.sym  = Sym::REAL_CONST;
    t.rval = std::strtod(text.c_str(), nullptr);
    if (!std::isfinite(t.rval))
        return error("Real constant is too large");
    return t;
}

//
// 'COMMON' /block/ name, name(d1, d2, d3), … ; read here as one symbol.
//
Token Lexer::common()
{
    int start = line_;
    auto bad  = [&](const std::string &message) {
        while (ch_ != ';')
            advance();
        step();
        Token t = error("'COMMON': " + message);
        t.line  = start;
        return t;
    };
    auto block = std::make_shared<CommonBlock>();
    unsigned c = fetch();
    if (c != '/')
        return bad("'/' expected");
    std::string koi;
    while ((c = advance()) != '/') {
        if (c == ';')
            return bad("'/' expected after the block name");
        koi += char(c);
    }
    block->name = to_utf8(koi);
    for (;;) {
        CommonBlock::Element e;
        koi.clear();
        c = advance();
        while (c != ',' && c != ';' && c != '(' && c != '.') {
            koi += char(c);
            c = advance();
        }
        if (koi.empty())
            return bad("element name expected");
        e.name = to_utf8(koi);
        if (c == '(') {
            for (;;) {
                std::string n;
                while (is_digit(c = advance()))
                    n += char(c);
                if (n.empty() || (c != ',' && c != ')'))
                    return bad("bad dimension of '" + e.name + "'");
                e.dims.push_back(std::atoi(n.c_str()));
                e.size *= e.dims.back();
                if (c == ')')
                    break;
            }
            c = advance();
        }
        if (c == '.') {
            if (advance() != ',')
                return bad("',' or ';' expected");
            c = ';';
        }
        block->elements.push_back(std::move(e));
        if (c == ';')
            break;
        if (c != ',')
            return bad("',' or ';' expected");
    }
    step();
    Token t  = make(Sym::COMMON);
    t.line   = start;
    t.common = block;
    return t;
}
