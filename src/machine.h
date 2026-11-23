#ifndef ALGOL_MACHINE_H
#define ALGOL_MACHINE_H

#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "arith.h"
#include "stdarray.h"
#include "workspace.h"

//
// BESM-6 state for the code translated from the overlay (scripts/gen-pass.py):
// one call per instruction, with the semantics of dubna's processor.cpp.
// The address field of each instruction goes through field(), which adds the
// modifier C of a preceding UTC/WTC and clears it.
//
class Machine {
public:
    static constexpr unsigned MEMSIZE = 0100000;
    static constexpr unsigned SP      = 017;

    std::vector<Word> mem = std::vector<Word>(MEMSIZE);
    unsigned M[16]{};
    Word acc{};
    Word rmr{};
    unsigned rau{};
    unsigned mod{};

    // Drums for *70.
    Workspace *drums{};

    // Routines in C++: entered by a jump, they return the address to go on.
    std::map<unsigned, std::function<unsigned(Machine &)>> natives;

    // Called at the marked entries of the translated code.
    std::function<void(Machine &, unsigned)> on_entry;

    // Called before each instruction: address and half (1 = right).
    std::function<void(Machine &, unsigned, bool)> trace;

    // The whole overlay, then the workspace over it.
    void load_image();

    // Run translated code from pc until it jumps out of it; returns the address.
    using Code = unsigned (*)(Machine &, unsigned);
    unsigned run(Code code, unsigned pc);

    //
    // Addressing.
    //
    unsigned field(unsigned addr)
    {
        unsigned a = (addr + mod) & 077777;
        mod        = 0;
        return a;
    }
    unsigned ea(unsigned reg, unsigned a) const { return (a + M[reg]) & 077777; }
    void set_m(unsigned reg, unsigned v)
    {
        M[reg] = v & 077777;
        M[0]   = 0;
    }
    // Operand of a reading instruction: from the stack when 15,…,0.
    Word &operand(unsigned reg, unsigned a)
    {
        if (!a && reg == SP)
            M[SP] = (M[SP] - 1) & 077777;
        return mem[ea(reg, a)];
    }
    // Address 0 always reads as zero.
    void store(unsigned a, Word w)
    {
        if (a)
            mem[a] = w;
    }
    void push(Word w)
    {
        store(M[SP], w);
        M[SP] = (M[SP] + 1) & 077777;
    }

    //
    // Mode of the arithmetic unit.
    //
    enum : unsigned { LOG = 004, MULT = 010, ADD = 020, OMEGA = 034 };
    void set_logical() { rau = (rau & ~OMEGA) | LOG; }
    void set_additive() { rau = (rau & ~OMEGA) | ADD; }
    void set_multiplicative() { rau = (rau & ~OMEGA) | MULT; }
    bool is_additive() const { return rau & ADD; }
    bool is_multiplicative() const { return (rau & (ADD | MULT)) == MULT; }
    bool is_logical() const { return (rau & OMEGA) == LOG; }

    //
    // Instructions.
    //
    void atx(unsigned reg, unsigned addr)
    {
        unsigned a = field(addr);
        store(ea(reg, a), acc);
        if (!a && reg == SP)
            M[SP] = (M[SP] + 1) & 077777;
    }
    void stx(unsigned reg, unsigned addr)
    {
        unsigned a = field(addr);
        store(ea(reg, a), acc);
        M[SP] = (M[SP] - 1) & 077777;
        acc   = mem[M[SP]];
        set_logical();
    }
    void xts(unsigned reg, unsigned addr)
    {
        unsigned a = field(addr);
        push(acc);
        acc = mem[ea(reg, a)];
        set_logical();
    }
    void arith(unsigned reg, unsigned addr, int op);
    void xta(unsigned reg, unsigned addr)
    {
        acc = operand(reg, field(addr));
        set_logical();
    }
    void aax(unsigned reg, unsigned addr)
    {
        acc &= operand(reg, field(addr));
        rmr = 0;
        set_logical();
    }
    void aex(unsigned reg, unsigned addr)
    {
        Word x = operand(reg, field(addr));
        rmr    = acc;
        acc ^= x;
        set_logical();
    }
    void arx(unsigned reg, unsigned addr)
    {
        acc += operand(reg, field(addr));
        if (acc >> 48)
            acc = (acc + 1) & WORD_MASK;
        rmr = 0;
        set_multiplicative();
    }
    void aox(unsigned reg, unsigned addr)
    {
        acc |= operand(reg, field(addr));
        rmr = 0;
        set_logical();
    }
    void apx(unsigned reg, unsigned addr);
    void aux(unsigned reg, unsigned addr);
    void acx(unsigned reg, unsigned addr);
    void anx(unsigned reg, unsigned addr);
    void asx(unsigned reg, unsigned addr)
    {
        shift(int((operand(reg, field(addr)) >> 41) & 0177) - 64);
        set_logical();
    }
    void xtr(unsigned reg, unsigned addr) { rau = (operand(reg, field(addr)) >> 41) & 077; }
    void rte(unsigned reg, unsigned addr)
    {
        acc = Word(rau & ea(reg, field(addr)) & 0177) << 41;
        set_logical();
    }
    void yta(unsigned reg, unsigned addr);
    void asn(unsigned reg, unsigned addr)
    {
        shift(int(ea(reg, field(addr)) & 0177) - 64);
        set_logical();
    }
    void ntr(unsigned reg, unsigned addr) { rau = ea(reg, field(addr)) & 077; }
    void ati(unsigned reg, unsigned addr) { set_m(ea(reg, field(addr)) & 017, unsigned(acc)); }
    void sti(unsigned reg, unsigned addr)
    {
        unsigned rg = ea(reg, field(addr)) & 017;
        unsigned ad = acc & 077777;
        if (rg != SP)
            M[SP] = (M[SP] - 1) & 077777;
        acc = mem[rg != SP ? M[SP] : ad];
        set_m(rg, ad);
        set_logical();
    }
    void ita(unsigned reg, unsigned addr)
    {
        acc = M[ea(reg, field(addr)) & 017];
        set_logical();
    }
    void its(unsigned reg, unsigned addr)
    {
        push(acc);
        ita(reg, addr);
    }
    void mtj(unsigned reg, unsigned addr) { set_m(field(addr) & 017, M[reg]); }
    void jam(unsigned reg, unsigned addr) // J+M
    {
        unsigned j = field(addr) & 017;
        set_m(j, M[j] + M[reg]);
    }
    void extracode(unsigned op, unsigned reg, unsigned addr);
    void utc(unsigned reg, unsigned addr) { mod = ea(reg, field(addr)); }
    void wtc(unsigned reg, unsigned addr) { mod = operand(reg, field(addr)) & 077777; }
    void vtm(unsigned reg, unsigned addr) { set_m(reg, field(addr)); }
    void utm(unsigned reg, unsigned addr) { set_m(reg, ea(reg, field(addr))); }

    // Conditional jumps: true to jump.
    bool uza()
    {
        rmr = acc;
        if (is_additive())
            return !(acc & 0'0020'0000'0000'0000);
        if (is_multiplicative())
            return acc & 0'4000'0000'0000'0000;
        if (is_logical())
            return !acc;
        return false;
    }
    bool u1a()
    {
        rmr = acc;
        if (is_additive())
            return acc & 0'0020'0000'0000'0000;
        if (is_multiplicative())
            return !(acc & 0'4000'0000'0000'0000);
        if (is_logical())
            return acc;
        return true;
    }
    bool vlm(unsigned reg)
    {
        if (!M[reg])
            return false;
        set_m(reg, M[reg] + 1);
        return true;
    }

    [[noreturn]] static void illegal(unsigned addr);

private:
    void shift(int n);
    void e70(Word info);
};

#endif
