//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//
// The lexer. It reads the source as Unicode characters; names and strings
// come out in UTF-8.
//
#include "lexer.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

#include "diag.h"
#include "source.h"

namespace {

constexpr char32_t QUOTE     = '\'';
constexpr char32_t TEN       = U'⏨';
constexpr char32_t UNDERLINE = 0x332; // combining low line

bool is_letter(char32_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= 0xc0 && c <= 0x24f && c != 0xd7 && c != 0xf7) || // Latin
           (c >= 0x370 && c <= 0x4ff);                            // Greek, Cyrillic
}

bool is_digit(char32_t c)
{
    return c >= '0' && c <= '9';
}

bool is_octal(char32_t c)
{
    return c >= '0' && c <= '7';
}

bool is_blank(char32_t c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

// The symbol of a one-character delimiter; ERROR for others.
Sym single(char32_t c)
{
    switch (c) {
    case U'∨':
        return Sym::OR;
    case U'∧':
    case '&':
        return Sym::AND;
    case U'⊃':
        return Sym::IMPL;
    case U'¬':
        return Sym::NOT;
    case U'≡':
        return Sym::EQUIV;
    case U'≠':
        return Sym::NE;
    case U'≤':
    case U'⩽':
        return Sym::LE;
    case U'≥':
    case U'⩾':
        return Sym::GE;
    case U'×':
        return Sym::TIMES;
    case U'÷':
        return Sym::INTDIV;
    case U'↑':
    case '^':
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

// Keywords, in lower case, without '_'; _comment is ERROR, the lexer skips it.
const std::map<std::string, Sym> &keywords()
{
    static const std::map<std::string, Sym> table = {
        { "algol", Sym::ALGOL },     { "and", Sym::AND },
        { "array", Sym::ARRAY },     { "backspace", Sym::BACKSPACE },
        { "begin", Sym::BEGIN },     { "bits", Sym::BITS },
        { "boolean", Sym::BOOLEAN }, { "code", Sym::CODE },
        { "comment", Sym::ERROR },   { "common", Sym::COMMON },
        { "decode", Sym::DECODE },   { "div", Sym::INTDIV },
        { "do", Sym::DO },           { "else", Sym::ELSE },
        { "encode", Sym::ENCODE },   { "end", Sym::END },
        { "endfile", Sym::ENDFILE }, { "eop", Sym::EOP },
        { "eq", Sym::EQ },           { "equal", Sym::EQ },
        { "equiv", Sym::EQUIV },     { "false", Sym::FALSE },
        { "for", Sym::FOR },         { "ge", Sym::GE },
        { "goto", Sym::GOTO },       { "greater", Sym::GT },
        { "gt", Sym::GT },           { "if", Sym::IF },
        { "impl", Sym::IMPL },       { "integer", Sym::INTEGER },
        { "label", Sym::LABEL },     { "le", Sym::LE },
        { "less", Sym::LT },         { "lt", Sym::LT },
        { "ne", Sym::NE },           { "not", Sym::NOT },
        { "notequal", Sym::NE },     { "notgreater", Sym::LE },
        { "notless", Sym::GE },      { "or", Sym::OR },
        { "own", Sym::OWN },         { "power", Sym::POWER },
        { "print", Sym::PRINT },     { "procedure", Sym::PROCEDURE },
        { "punch", Sym::PUNCH },     { "read", Sym::READ },
        { "real", Sym::REAL },       { "rewind", Sym::REWIND },
        { "step", Sym::STEP },       { "string", Sym::STRING },
        { "switch", Sym::SWITCH },   { "then", Sym::THEN },
        { "true", Sym::TRUE },       { "until", Sym::UNTIL },
        { "value", Sym::VALUE },     { "while", Sym::WHILE },
        { "write", Sym::WRITE },
    };
    return table;
}

// A keyword by its name in any case; nullptr when unknown.
const Sym *keyword(const std::string &name)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](char c) { return c >= 'A' && c <= 'Z' ? char(c + 'a' - 'A') : c; });
    auto it = keywords().find(lower);
    return it == keywords().end() ? nullptr : &it->second;
}

std::string utf8(char32_t c)
{
    std::string s;
    append_utf8(s, c);
    return s;
}

} // namespace

const char *to_string(Sym s)
{
    static const char *const names[] = {
        "identifier",    "integer constant",
        "real constant", "octal constant",
        "string",        "_common",
        "'+'",           "'-'",
        "'*'",           "'/'",
        "_div",          "'**'",
        "'<'",           "'<='",
        "'='",           "'!='",
        "'>='",          "'>'",
        "_not",          "_and",
        "_or",           "_impl",
        "_equiv",        "'('",
        "')'",           "'['",
        "']'",           "','",
        "':'",           "';'",
        "':='",          "_goto",
        "_if",           "_then",
        "_else",         "_for",
        "_do",           "_step",
        "_until",        "_while",
        "_begin",        "_end",
        "_own",          "_boolean",
        "_integer",      "_real",
        "_string",       "_bits",
        "_array",        "_switch",
        "_procedure",    "_label",
        "_value",        "_true",
        "_false",        "_algol",
        "_code",         "_read",
        "_print",        "_punch",
        "_write",        "_decode",
        "_encode",       "_rewind",
        "_backspace",    "_endfile",
        "_eop",          "error",
        "end of file",
    };
    static_assert(sizeof(names) / sizeof(names[0]) == size_t(Sym::END_OF_FILE) + 1);
    return names[int(s)];
}

Lexer::Lexer(const std::string &source, Diagnostics &diag,
             std::function<void(int, const std::string &)> on_line)
    : diag_(diag), on_line_(std::move(on_line))
{
    size_t pos = 0;
    while (pos < source.size()) {
        size_t end = source.find('\n', pos);
        if (end == std::string::npos)
            end = source.size();
        std::string text = source.substr(pos, end - pos);
        if (!text.empty() && text.back() == '\r')
            text.pop_back();
        lines_.push_back(text);
        pos = end + 1;
    }
    publication_ = source.find("\xCC\xB2") != std::string::npos;
}

// The current letter is underlined: a letter of a keyword.
bool Lexer::underlined() const
{
    return publication_ && col_ + 1 < text_.size() && text_[col_ + 1] == UNDERLINE;
}

// Underlined letters from ch_, up to anything else; g̲o̲ t̲o̲ is one.
std::string Lexer::underlined_word()
{
    std::string w;
    for (;;) {
        while (is_letter(ch_) && underlined()) {
            append_utf8(w, ch_);
            col_ += 2;
            while (text_[col_] == UNDERLINE) // a doubled underline
                col_++;
            ch_ = text_[col_];
        }
        if (w != "go")
            return w;
        if (!is_letter(fetch()) || !underlined())
            return w;
    }
}

// The current character, from the next line when this one is done; blanks
// skipped unless in a string.
char32_t Lexer::fetch()
{
    for (;;) {
        if (col_ >= text_.size()) {
            if (next_line_ >= lines_.size()) {
                if (soft_eof_) // a name or number at the end of the file
                    return ch_ = 0;
                throw EndOfInput{};
            }
            const std::string &text = lines_[next_line_++];
            text_                   = decode_utf8(text) + U'\n';
            std::replace(text_.begin(), text_.end(), U'′', QUOTE);
            line_++;
            col_ = 0;
            if (on_line_)
                on_line_(line_, text);
        }
        char32_t c = text_[col_];
        if (skip_blanks_ && is_blank(c)) {
            col_++;
            continue;
        }
        return ch_ = c;
    }
}

char32_t Lexer::advance()
{
    step();
    return fetch();
}

Token Lexer::make(Sym s) const
{
    Token t;
    t.sym  = s;
    t.line = start_;
    return t;
}

// At the symbol being read.
Token Lexer::error(const std::string &message)
{
    diag_.error(start_, message);
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
        if (!skip_blanks_)
            diag_.error(string_line_, "Unterminated string");
        eof_ = true;
        return make(Sym::END_OF_FILE);
    }
}

Token Lexer::scan()
{
    char32_t c = fetch();
    start_     = Pos(line_, int(col_) + 1);
    if (after_end_)
        return after_end();
    if (is_letter(c) && underlined()) {
        Pos start      = start_;
        std::string kw = underlined_word();
        if (!keyword(kw))
            return error("Unknown keyword " + kw);
        return keyword_token(kw, start);
    }
    if (is_letter(c))
        return name();
    if (is_digit(c) || c == TEN)
        return number(false);
    switch (c) {
    case '_':
        step();
        if (!is_letter(ch_ = text_[col_]))
            return error("Illegal character '_'");
        return keyword_symbol();
    case QUOTE:
        c = advance();
        if (c == QUOTE)
            return rejected_string(false);
        if (is_letter(c)) {
            Pos start     = start_;
            std::string w = word(true);
            if (text_[col_] == QUOTE)
                step();
            if (!keyword(w))
                return error("Write a keyword as _" + w + ", not in quotes");
            // Reported, then read as the keyword.
            diag_.error(start, "Write a keyword as _" + w + ", not in quotes");
            return keyword_token(w, start);
        }
        if (is_digit(c))
            return octal();
        if (c == '(' && advance() == QUOTE)
            return rejected_string(false);
        return error("Unexpected character after a quote");
    case '"':
        return rejected_string(true);
    case '`':
        return quoted_string(QUOTE);
    case U'‘':
        return quoted_string(U'’');
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
            return error("Illegal character '" + utf8(c) + "'");
        return make(s);
    }
    }
}

// Letters and digits from ch_; blanks inside are skipped, unless raw.
std::string Lexer::word(bool raw)
{
    std::string w;
    skip_blanks_ = !raw;
    for (char32_t c = ch_; (is_letter(c) || is_digit(c)) && !underlined(); c = advance())
        append_utf8(w, c);
    skip_blanks_ = true;
    return w;
}

Token Lexer::name()
{
    SoftEof guard(soft_eof_);
    Pos start = start_;
    Token t   = make(Sym::IDENT);
    t.text    = word(false);
    t.line    = start;
    return t;
}

// A keyword: _ and letters and digits, in any case, up to a blank.
Token Lexer::keyword_symbol()
{
    Pos start      = start_;
    std::string kw = word(true);
    if (!keyword(kw))
        return error("Unknown keyword _" + kw);
    return keyword_token(kw, start);
}

Token Lexer::keyword_token(const std::string &kw, Pos start)
{
    const Sym *sym = keyword(kw);
    switch (*sym) {
    case Sym::ERROR:
        comment();
        return scan();
    case Sym::END:
        after_end_ = true;
        break;
    case Sym::COMMON:
        return common();
    default:
        break;
    }
    Token t = make(*sym);
    t.line  = start;
    return t;
}

// _comment … ';' (or '.,').
void Lexer::comment()
{
    if (last_ != Sym::SEMICOLON && last_ != Sym::BEGIN)
        diag_.error(line_, "_comment must follow ';' or _begin");
    char32_t c = fetch();
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

// The text after _end, up to ';', _end, _else or _eop. Letters and digits
// are a comment; anything else gets a warning.
Token Lexer::after_end()
{
    char32_t c = ch_;
    for (;;) {
        if (c == ';')
            break;
        if (c == '.') {
            c = advance();
            if (c == ',')
                break;
        } else if ((is_letter(c) && underlined()) || (c == '_' && is_letter(text_[col_ + 1]))) {
            Pos start(line_, int(col_) + 1);
            std::string w;
            if (c == '_') {
                step();
                ch_ = text_[col_];
                w   = word(true);
            } else {
                w = underlined_word();
            }
            const Sym *sym = keyword(w);
            if (sym && (*sym == Sym::END || *sym == Sym::ELSE || *sym == Sym::EOP)) {
                warned_ = false;
                if (*sym != Sym::END)
                    after_end_ = false;
                Token t = make(*sym);
                t.line  = start;
                return t;
            }
            c = fetch();
            continue;
        } else if (is_letter(c) || is_digit(c)) {
            c = advance();
            continue;
        }
        if (!warned_) {
            diag_.warning(line_, "Text after _end is skipped up to ';', _end or _else");
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

// A string of Algol-GDR, ''…'' or '('…')' ('ddd' inside), or one in "…": an
// error, skipped up to its end.
Token Lexer::rejected_string(bool double_quoted)
{
    Pos start    = start_;
    string_line_ = start;
    skip_blanks_ = false;
    for (;;) {
        char32_t c = advance();
        if (double_quoted) {
            if (c == '"')
                break;
            continue;
        }
        if (c != QUOTE)
            continue;
        c = advance();
        if (c == QUOTE)
            break;
        if (c == ')' && text_[col_ + 1] == QUOTE) {
            step();
            break;
        }
        while (is_octal(c)) // 'ddd': c ends at its closing quote
            c = advance();
    }
    skip_blanks_ = true;
    step();
    Token t = error(double_quoted ? "Strings in \"…\" are not supported: write `text'"
                                  : "Strings in '' are not supported: write `text'");
    t.line  = start;
    return t;
}

// A string: `…' or ‘…’, nested; the inner quotes are kept. In `…', '` is
// an apostrophe.
Token Lexer::quoted_string(char32_t close)
{
    char32_t open = ch_;
    Pos start     = start_;
    string_line_  = start;
    skip_blanks_  = false;
    std::string text;
    for (int depth = 1;;) {
        char32_t c = advance();
        if (c == open) {
            depth++;
        } else if (c == close && open == '`' && text_[col_ + 1] == '`') {
            step(); // '`: an apostrophe
        } else if (c == close && --depth == 0) {
            break;
        }
        append_utf8(text, c);
    }
    skip_blanks_ = true;
    step();
    Token t = make(Sym::STRING_CONST);
    t.line  = start;
    t.text  = text;
    return t;
}

// An octal constant: 'ddd', up to 16 digits.
Token Lexer::octal()
{
    uint64_t v = 0;
    for (int n = 1;; n++) {
        if (!is_octal(ch_) || n > 16) {
            const char *message = n > 16 ? "Octal constant is too long" : "Bad octal digit";
            while (ch_ != QUOTE && col_ + 1 < text_.size())
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
    SoftEof guard(soft_eof_);
    Pos start = start_;
    std::string whole, fraction, exponent;
    bool exp = false, negative = false, digit = false, bad = point && !is_digit(ch_);
    for (char32_t c = ch_; !bad;) {
        if (is_digit(c)) {
            (exp ? exponent : point ? fraction : whole) += char(c);
            digit = true;
            c     = advance();
            continue;
        }
        if (c == TEN) {
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
// _common /block/ name, name(d1, d2, d3), … ; read here as one symbol.
//
Token Lexer::common()
{
    Pos start = start_;
    auto bad  = [&](const std::string &message) {
        while (ch_ != ';')
            advance();
        step();
        Token t = error("_common: " + message);
        t.line  = start;
        return t;
    };
    auto block = std::make_shared<CommonBlock>();
    char32_t c = fetch();
    if (c != '/')
        return bad("'/' expected");
    while ((c = advance()) != '/') {
        if (c == ';')
            return bad("'/' expected after the block name");
        append_utf8(block->name, c);
    }
    for (;;) {
        CommonBlock::Element e;
        c = advance();
        while (c != ',' && c != ';' && c != '(' && c != '.') {
            append_utf8(e.name, c);
            c = advance();
        }
        if (e.name.empty())
            return bad("element name expected");
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
