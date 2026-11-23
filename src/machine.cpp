//
// Ported from dubna/processor.cpp and extracode.cpp:
// Copyright (c) 2022-2023 Leonid Broukhis, Serge Vakulenko. MIT license.
//
#include "machine.h"

#include "tables.h"

namespace {

constexpr Word BIT48 = 0'4000'0000'0000'0000;

Word pack(Word val, Word mask)
{
    Word result = 0;
    for (; mask; mask >>= 1, val >>= 1)
        if (mask & 1) {
            result >>= 1;
            if (val & 1)
                result |= BIT48;
        }
    return result;
}

Word unpack(Word val, Word mask)
{
    Word result = 0;
    for (unsigned i = 0; i < 48; ++i) {
        result <<= 1;
        if (mask & BIT48) {
            if (val & BIT48)
                result |= 1;
            val <<= 1;
        }
        mask <<= 1;
    }
    return result & WORD_MASK;
}

unsigned highest_bit(Word val)
{
    int n = 32, cnt = 0;
    do {
        Word tmp = val;
        if (tmp >>= n) {
            cnt += n;
            val = tmp;
        }
    } while (n >>= 1);
    return 48 - cnt;
}

unsigned count_ones(Word w)
{
    unsigned c = 0;
    for (; w; ++c)
        w &= w - 1;
    return c;
}

Word cyclic_add(Word a, Word b)
{
    Word s = a + b;
    if (s >> 48)
        s = (s + 1) & WORD_MASK;
    return s;
}

} // namespace

void Machine::load_image()
{
    std::fill(mem.begin(), mem.end(), 0);
    for (size_t i = 0; i < tables::image_size; i++)
        mem[tables::image_base + i] = tables::image[i];
    if (drums) {
        for (unsigned a = Workspace::LOW_BEGIN; a < Workspace::LOW_END; a++)
            mem[a] = (*drums)[a];
        for (unsigned a = Workspace::COMTAB; a < Workspace::COMTAB_END; a++)
            mem[a] = (*drums)[a];
    }
}

unsigned Machine::run(Code code, unsigned pc)
{
    code_ = code;
    for (;;) {
        pc = code(*this, pc);
        if (pc == stop)
            return pc;
        auto it = natives.find(pc);
        if (it == natives.end())
            return pc;
        pc = it->second(*this);
    }
}

void Machine::call(unsigned addr, unsigned reg, unsigned ret)
{
    unsigned saved = stop;
    stop           = ret;
    set_m(reg, ret);
    unsigned pc = run(code_, addr);
    stop        = saved;
    if (pc != ret)
        throw std::runtime_error("call of " + std::to_string(addr) + " left at " +
                                 std::to_string(pc));
}

void Machine::illegal(unsigned addr)
{
    throw std::runtime_error("illegal instruction at " + std::to_string(addr));
}

void Machine::shift(int n)
{
    rmr = 0;
    if (n > 0) {
        if (n < 48) {
            rmr = (acc << (48 - n)) & WORD_MASK;
            acc >>= n;
        } else {
            rmr = acc >> (n - 48);
            acc = 0;
        }
    } else if (n < 0) {
        n = -n;
        if (n < 48) {
            rmr = acc >> (48 - n);
            acc = (acc << n) & WORD_MASK;
        } else {
            rmr = (acc << (n - 48)) & WORD_MASK;
            acc = 0;
        }
    }
}

//
// A+X 004, A-X 005, X-A 006, AMX 007, AVX 014, A/X 016, A*X 017, E+X 024,
// E-X 025; E+N 034 and E-N 035 take the address itself.
//
void Machine::arith(unsigned reg, unsigned addr, int op)
{
    unsigned a = field(addr);
    Word x     = 0;
    if (op == 034 || op == 035)
        x = ea(reg, a);
    else
        x = operand(reg, a);
    Arith u(acc, rau);
    u.rmr = rmr;
    switch (op) {
    case 004:
        u.add(x);
        break;
    case 005:
        u.sub(x);
        break;
    case 006:
        u.rsub(x);
        break;
    case 007:
        u.sub_abs(x);
        break;
    case 014:
        u.change_sign((x >> 40) & 1);
        break;
    case 016:
        u.div(x);
        break;
    case 017:
        u.mul(x);
        break;
    case 024:
        u.add_exponent(int((x >> 41) & 0177) - 64);
        break;
    case 025:
        u.add_exponent(64 - int((x >> 41) & 0177));
        break;
    case 034:
        u.add_exponent(int(x & 0177) - 64);
        break;
    case 035:
        u.add_exponent(64 - int(x & 0177));
        break;
    default:
        illegal(op);
    }
    acc = u.acc;
    rmr = u.rmr;
    if (op <= 007 || op == 014)
        set_additive();
    else
        set_multiplicative();
}

void Machine::apx(unsigned reg, unsigned addr)
{
    acc = pack(acc, operand(reg, field(addr)));
    rmr = 0;
    set_logical();
}

void Machine::aux(unsigned reg, unsigned addr)
{
    acc = unpack(acc, operand(reg, field(addr)));
    rmr = 0;
    set_logical();
}

void Machine::acx(unsigned reg, unsigned addr)
{
    acc = cyclic_add(count_ones(acc), operand(reg, field(addr)));
    rmr = 0;
    set_logical();
}

void Machine::anx(unsigned reg, unsigned addr)
{
    Word x = operand(reg, field(addr));
    if (acc) {
        unsigned n = highest_bit(acc);
        shift(int(48 - n));
        acc = cyclic_add(n, x);
    } else {
        rmr = 0;
        acc = x;
    }
    set_logical();
}

void Machine::yta(unsigned reg, unsigned addr)
{
    unsigned a = ea(reg, field(addr));
    if (is_logical()) {
        acc = rmr;
        return;
    }
    Word y = rmr;
    acc    = (acc & ~Word(0'0037'7777'7777'7777)) | (rmr & 0'0017'7777'7777'7777);
    Arith u(acc, rau);
    u.rmr = rmr;
    u.add_exponent(int(a & 0177) - 64);
    acc = u.acc;
    rmr = y;
}

void Machine::extracode(unsigned op, unsigned reg, unsigned addr)
{
    unsigned a = ea(reg, field(addr));
    set_m(14, a);
    switch (op) {
    case 070:
        e70(a ? mem[a] : acc);
        break;
    default:
        throw std::runtime_error("extracode " + std::to_string(op) + " not implemented");
    }
    set_logical();
}

//
// *70: a page (1024 words) or a sector (256 words) between memory and a drum.
//
void Machine::e70(Word info)
{
    if (!drums)
        throw std::runtime_error("*70 without drums");
    bool read        = (info >> 39) & 1;
    bool sector_io   = (info >> 47) & 1;
    unsigned unit    = (info >> 12) & 077;
    unsigned page    = (info >> 30) & 037;
    unsigned tract   = info & 037;
    unsigned sector  = (info >> 6) & 3;
    unsigned para    = (info >> 24) & 3;
    unsigned addr    = page << 10;
    unsigned nwords  = 1024;
    unsigned drumoff = 0;
    if (unit >= 030 && unit < 070)
        throw std::runtime_error("*70: disk i/o not implemented, info " + std::to_string(info));
    if (sector_io) {
        if ((info >> 35) & 1) {
            sector = info & 3;
            tract  = (info >> 2) & 037;
        }
        addr += para << 8;
        nwords  = 256;
        drumoff = sector << 8;
    }
    unit &= 037;
    for (unsigned i = 0; i < nwords; i++) {
        Word &d = drums->drum(unit, tract, drumoff + i);
        if (read)
            store((addr + i) & 077777, d);
        else
            d = mem[(addr + i) & 077777];
    }
}
