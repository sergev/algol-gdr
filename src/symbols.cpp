//
// Copyright (c) 2026 Serge Vakulenko. MIT license.
//

#include "symbols.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ostream>
#include <stdexcept>

#include "listing.h"
#include "passes.h"

namespace {

// D-list words, as pass 1 makes them (pass1_parser.cpp).
constexpr Word BIT48      = 0'4000'0000'0000'0000; // a block header with arrays; a value formal; 'CODE'
constexpr Word DESCRIPTOR = 0'2000'0000'0000'0000;
constexpr Word BIT46      = 0'1000'0000'0000'0000; // a formal by name; a FOR
constexpr Word BIT45      = 0'0400'0000'0000'0000; // a procedure's head
constexpr Word STRING     = 0'0200'0000'0000'0000;
constexpr Word BOOLEAN    = 0'0100'0000'0000'0000;
constexpr Word BITS       = 0'0060'0000'0000'0000;
constexpr Word INTEGER    = 0'0040'0000'0000'0000;
constexpr Word REAL       = 0'0020'0000'0000'0000;
constexpr Word LABEL      = 0'0010'0000'0000'0000;
constexpr Word PROCEDURE  = 0'0002'0000'0000'0000;
constexpr Word ARRAY      = 0'0001'0000'0000'0000;

constexpr unsigned DLIST_BEGIN = Tables::DLIST_BEGIN;
constexpr unsigned STANDARD    = Tables::STANDARD;
constexpr unsigned DLIST_END   = Tables::DLIST_END;
constexpr unsigned POOL_BASE   = 01634; // string index of the first pool word

unsigned link(Word w)
{
    return w & 07777;
}

const char *type_name(Type t)
{
    static const char *const names[] = { "none", "real", "integer", "boolean", "string", "bits" };
    return names[int(t)];
}

const char *kind_name(Kind k)
{
    static const char *const names[] = { "simple", "array", "procedure", "switch", "label" };
    return names[int(k)];
}

const char *mode_name(Mode m)
{
    static const char *const names[] = { "local",  "value",  "name",    "code",
                                         "algol", "common", "standard" };
    return names[int(m)];
}

// ISO characters of words, 6 per word, up to a zero byte.
std::string iso_text(const Word *w, size_t n, size_t *used = nullptr)
{
    std::string s;
    size_t i = 0;
    for (; i < n; i++) {
        bool end = false;
        for (int k = 5; k >= 0; k--) {
            unsigned c = (w[i] >> (8 * k)) & 0377;
            if (!c) {
                end = true;
                break;
            }
            s += listing_char(c);
        }
        if (end) {
            i++;
            break;
        }
    }
    if (used)
        *used = i;
    return s;
}

} // namespace

Symbols::Symbols(const Context &ctx)
    : names_(ctx.saved_names), strings_(ctx.strings)
{
    if (ctx.dlist.size() != DLIST_END - DLIST_BEGIN || names_.size() != Tables::SLOTS)
        throw std::logic_error("symbols: passes 1 and 2 have not run");
    constants_.assign(ctx.tables.names().begin(), ctx.tables.names().end());
    first_constant_ = ctx.tables.free_const & 07777;

    // The D-list as pass 1 leaves it, at its addresses.
    std::vector<Word> dlist(DLIST_END);
    std::copy(ctx.dlist.begin(), ctx.dlist.end(), dlist.begin() + DLIST_BEGIN);

    // The 'COMMON' table: a zero second word ends it; block entries have slot 0.
    const Tables &t = ctx.tables;
    for (unsigned i = 0; i + 2 < Tables::COMMON_SIZE && t.common(i + 1); i += 3) {
        Word info = t.common(i + 1);
        CommonEntry e;
        Word text = t.common(i);
        e.name    = rtrim(iso_text(&text, 1));
        e.slot = (info >> 24) & 07777;
        if (e.slot) {
            e.decl = (info >> 36) & 07777;
            e.size = t.common(i + 2) & 077777777;
        } else {
            e.size = info & 077777777;
        }
        common.push_back(e);
    }

    root          = ctx.regs[3] & 07777;
    Scope &top    = scopes[root];
    top.kind      = ScopeKind::ROOT;
    top.addr      = root;
    scan(root, DLIST_END, top, dlist);
    const auto &items = scopes[root].items;
    auto block        = std::find_if(items.begin(), items.end(), [this](unsigned a) {
        return is_scope(a) && scopes[a].kind == ScopeKind::BLOCK;
    });
    if (block != items.end())
        program = *block;

    // The formals: the last ones pushed into a procedure's scope.
    for (auto &[addr, scope] : scopes) {
        if (scope.kind != ScopeKind::PROCEDURE)
            continue;
        for (unsigned f : formals(scope.procedure)) {
            Decl &d = decls[f];
            d.mode  = (d.word & BIT46) ? Mode::NAME : Mode::VALUE;
        }
    }
}

// The declarations and nested scopes in addr+1 … end-1.
void Symbols::scan(unsigned addr, unsigned end, Scope &scope, const std::vector<Word> &dlist)
{
    for (unsigned p = addr + 1; p != end;) {
        if (p >= DLIST_END)
            throw std::logic_error("symbols: bad D-list link");
        Word w = dlist[p];
        scope.items.push_back(p);
        if (w & DESCRIPTOR) {
            decls[p] = decode(p, w, scope.kind);
            p++;
            continue;
        }
        Scope s;
        s.addr = p;
        if (w & BIT45) {
            s.kind      = ScopeKind::PROCEDURE;
            s.procedure = p - 1;
        } else if (w & BIT46) {
            s.kind = ScopeKind::FOR;
        } else {
            s.kind   = ScopeKind::BLOCK;
            s.arrays = (w & BIT48) != 0;
        }
        scan(p, link(w), scopes[p] = s, dlist);
        p = link(w);
    }
}

Decl Symbols::decode(unsigned addr, Word w, ScopeKind in) const
{
    Decl d;
    d.addr  = addr;
    d.word  = w;
    d.slot  = w & 07777;
    d.name  = name(d.slot);
    d.count = (w >> 15) & 037;

    if (w & STRING)
        d.type = Type::STRING;
    else if (w & BOOLEAN)
        d.type = Type::BOOLEAN;
    else if ((w & BITS) == BITS)
        d.type = Type::BITS;
    else if (w & INTEGER)
        d.type = Type::INTEGER;
    else if (w & REAL)
        d.type = Type::REAL;

    if (w & LABEL)
        d.kind = (w & ARRAY) ? Kind::SWITCH : Kind::LABEL;
    else if (w & PROCEDURE)
        d.kind = Kind::PROCEDURE;
    else if (w & ARRAY)
        d.kind = Kind::ARRAY;

    if (addr >= STANDARD) {
        d.mode = Mode::STANDARD;
        d.kind = Kind::PROCEDURE;
    } else if (w & BIT45) {
        bool block = std::any_of(common.begin(), common.end(), [&d](const CommonEntry &e) {
            return !e.slot && e.name == d.name;
        });
        if (block)
            d.mode = Mode::COMMON;
        else if (in == ScopeKind::ROOT)
            d.mode = (w & BIT48) ? Mode::CODE : Mode::ALGOL;
    }
    return d;
}

std::vector<unsigned> Symbols::formals(unsigned addr) const
{
    std::vector<unsigned> list;
    auto d = decls.find(addr);
    auto s = scopes.find(addr + 1);
    if (d == decls.end() || s == scopes.end() || s->second.kind != ScopeKind::PROCEDURE)
        return list;
    auto &items = s->second.items;
    for (unsigned i = 0; i < d->second.count && i < items.size(); i++)
        list.push_back(items[items.size() - 1 - i]);
    return list;
}

std::string Symbols::name(unsigned slot) const
{
    if (slot >= names_.size())
        return "?";
    return rtrim(iso_text(&names_[slot], 1));
}

Word Symbols::constant(unsigned slot) const
{
    if (slot < first_constant_ || slot >= constants_.size())
        throw std::out_of_range("no constant in slot " + std::to_string(slot));
    return constants_[slot];
}

double Symbols::to_real(Word w)
{
    int exponent     = int((w >> 41) & 0177) - 64;
    int64_t mantissa = int64_t(w << 23) >> 23; // 41 bits, signed
    return std::ldexp(double(mantissa), exponent - 40);
}

int64_t Symbols::to_integer(Word w)
{
    if (((w >> 41) & 0177) == 0150)
        return int64_t(w << 23) >> 23;
    return std::llround(to_real(w));
}

std::string Symbols::string(unsigned k) const
{
    if (k < POOL_BASE || k - POOL_BASE >= strings_.size())
        throw std::out_of_range("no string " + std::to_string(k));
    unsigned i = k - POOL_BASE;
    return iso_text(&strings_[i], strings_.size() - i);
}

void Symbols::print_scope(std::ostream &out, unsigned addr, int depth) const
{
    static const char *const kinds[] = { "root", "block", "procedure", "for" };
    char buf[32];
    const Scope &s = scopes.at(addr);
    std::snprintf(buf, sizeof(buf), "%05o", addr);
    out << std::string(2 * depth, ' ') << kinds[int(s.kind)] << ' ' << buf;
    if (s.kind == ScopeKind::PROCEDURE)
        out << ' ' << decls.at(s.procedure).name;
    if (addr == program)
        out << " program";
    if (s.arrays)
        out << " arrays";
    out << '\n';
    for (unsigned a : s.items) {
        if (is_scope(a)) {
            print_scope(out, a, depth + 1);
            continue;
        }
        const Decl &d = decls.at(a);
        std::snprintf(buf, sizeof(buf), "%05o", a);
        out << std::string(2 * depth + 2, ' ') << buf << ' ' << d.name << ' ' << type_name(d.type)
            << ' ' << kind_name(d.kind) << ' ' << mode_name(d.mode);
        if (d.count)
            out << ' ' << d.count;
        out << '\n';
    }
}

void Symbols::print(std::ostream &out) const
{
    print_scope(out, root, 0);
    char buf[64];
    for (unsigned s = first_constant_; s < CONSTANTS_END; s++) {
        Word w = constant(s);
        std::snprintf(buf, sizeof(buf), "const %04o %016llo %lld %.12g", s, (unsigned long long)w,
                      (long long)to_integer(w), to_real(w));
        out << buf << '\n';
    }
    for (size_t i = 0; i < strings_.size() && strings_[i];) {
        size_t used;
        std::string text = iso_text(&strings_[i], strings_.size() - i, &used);
        std::snprintf(buf, sizeof(buf), "string %04o ", unsigned(POOL_BASE + i));
        out << buf << '"' << text << "\"\n";
        i += used;
    }
    for (const auto &e : common) {
        out << "common " << e.name;
        if (e.slot) {
            std::snprintf(buf, sizeof(buf), " %05o", e.decl);
            out << buf;
        }
        out << " size " << e.size << '\n';
    }
}
