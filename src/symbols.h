//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#ifndef ALGOL_SYMBOLS_H
#define ALGOL_SYMBOLS_H

#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "word.h"

struct Context;

//
// The tables of the front end after pass 2, decoded: declarations (D-list
// words as pass 1 leaves them), their scopes, the constants, the string
// pool and the 'COMMON' table. Stream items refer to them by D-list
// address, constant slot or string index. See doc/passes.md.
//
enum class Type { NONE, REAL, INTEGER, BOOLEAN, STRING, BITS };
enum class Kind { SIMPLE, ARRAY, PROCEDURE, SWITCH, LABEL };

enum class Mode {
    LOCAL,    // declared in a block or the body of a procedure
    VALUE,    // formal called by value
    NAME,     // formal called by name
    CODE,     // 'CODE' procedure, declared before the program
    ALGOL,    // 'ALGOL' procedure, declared before the program
    COMMON,   // 'COMMON' block name
    STANDARD, // standard identifier (05754…)
};

struct Decl {
    unsigned addr{}; // D-list address
    unsigned slot{}; // name slot
    std::string name;
    Type type{};
    Kind kind{};
    Mode mode{};
    unsigned count{}; // parameters of a procedure, dimensions of an array
    Word word{};      // as pass 1 leaves it
};

enum class ScopeKind {
    ROOT,      // what lies outside the program: 'CODE' procedures, standard identifiers
    BLOCK,     // a block
    PROCEDURE, // formals and the body of a procedure
    FOR,       // the body of a FOR statement
};

struct Scope {
    ScopeKind kind{};
    unsigned addr{};             // its header word
    bool arrays{};               // bit 48 of a block header: arrays or 'COMMON' variables
    unsigned procedure{};        // its procedure's descriptor
    std::vector<unsigned> items; // declarations and nested scopes, by address upward
};

struct CommonEntry {
    std::string name;
    unsigned decl{}; // D-list address of its declaration (of the block: 0)
    unsigned slot{}; // name slot (of the block: 0)
    unsigned size{}; // words (of the block: its length)
};

class Symbols {
public:
    // From the context after pass 2.
    explicit Symbols(const Context &ctx);

    unsigned root{};    // the scope around the program
    unsigned program{}; // the outer block of the program
    std::map<unsigned, Decl> decls;
    std::map<unsigned, Scope> scopes;
    std::vector<CommonEntry> common;

    bool is_decl(unsigned addr) const { return decls.count(addr) != 0; }
    bool is_scope(unsigned addr) const { return scopes.count(addr) != 0; }

    // The formals of the procedure declared at addr, in order.
    std::vector<unsigned> formals(unsigned addr) const;

    std::string name(unsigned slot) const;

    // Constant of a slot: a BESM-6 word (integers are unnormalized, with
    // exponent 0150). The range of slots holding constants.
    Word constant(unsigned slot) const;
    unsigned first_constant() const { return first_constant_; }
    static constexpr unsigned CONSTANTS_END = 01634;
    static int64_t to_integer(Word w);
    static double to_real(Word w);

    // String of an item 025/070 operand, in UTF-8.
    std::string string(unsigned k) const;

    // Text form, for tests.
    void print(std::ostream &out) const;

private:
    std::vector<Word> names_;     // page 02000-03777 after pass 1
    std::vector<Word> constants_; // page 02000-03777 after pass 2
    std::vector<Word> strings_;
    unsigned first_constant_{};

    void scan(unsigned addr, unsigned end, Scope &scope, const std::vector<Word> &dlist);
    Decl decode(unsigned addr, Word w, ScopeKind in) const;
    void print_scope(std::ostream &out, unsigned addr, int depth) const;
};

#endif
