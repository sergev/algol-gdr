//
// Mnemonics from dubna/assembler.cpp (Leonid Broukhis, Serge Vakulenko, MIT).
//
#include "besm6.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <vector>

static const char *opname_short[64] = {
    // clang-format off
    "atx",  "stx",  "mod",  "xts",  "a+x",  "a-x",  "x-a",  "amx",
    "xta",  "aax",  "aex",  "arx",  "avx",  "aox",  "a/x",  "a*x",
    "apx",  "aux",  "acx",  "anx",  "e+x",  "e-x",  "asx",  "xtr",
    "rte",  "yta",  "*32",  "ext",  "e+n",  "e-n",  "asn",  "ntr",
    "ati",  "sti",  "ita",  "its",  "mtj",  "j+m",  "*46",  "*47",
    "*50",  "*51",  "*52",  "*53",  "*54",  "*55",  "*56",  "*57",
    "*60",  "*61",  "*62",  "*63",  "*64",  "*65",  "*66",  "*67",
    "*70",  "*71",  "*72",  "*73",  "*74",  "*75",  "*76",  "*77",
    // clang-format on
};

static const char *opname_long[16] = {
    "*20", "*21", "utc", "wtc",  "vtm", "utm", "uza", "u1a",
    "uj",  "vjm", "ij",  "stop", "vzm", "v1m", "*36", "vlm",
};

Insn Insn::decode(unsigned cmd)
{
    Insn insn;
    insn.reg     = (cmd >> 20) & 017;
    insn.is_long = (cmd >> 19) & 1;
    if (insn.is_long) {
        insn.opcode = (cmd >> 12) & 0370;
        insn.addr   = cmd & 077777;
    } else {
        insn.opcode = (cmd >> 12) & 077;
        insn.addr   = cmd & 07777;
        if (cmd & (1u << 18))
            insn.addr |= 070000;
    }
    return insn;
}

std::string besm6_opname(unsigned opcode)
{
    std::string name =
        (opcode & 0200) ? opname_long[(opcode >> 3) & 017] : opname_short[opcode & 077];
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return name;
}

unsigned besm6_assemble(const std::string &text)
{
    std::istringstream in(text);
    std::vector<std::string> f;
    for (std::string w; in >> w;)
        f.push_back(w);
    if (f.size() < 2 || f.size() > 3)
        throw std::invalid_argument("bad instruction: " + text);
    unsigned reg = f.size() == 3 ? std::stoul(f[0]) : 0;
    const std::string &name = f[f.size() - 2], &a = f.back();
    unsigned addr           = a == "*" ? 04444 : std::stoul(a, nullptr, 8);
    for (unsigned op = 0; op < 0400; op++) {
        if (op >= 0100 && (op < 0200 || (op & 7)))
            continue;
        if (besm6_opname(op) != name)
            continue;
        if (op & 0200)
            return reg << 20 | op << 12 | (addr & 077777);
        return reg << 20 | op << 12 | (addr & 07777) | (addr >= 070000 ? 1u << 18 : 0);
    }
    throw std::invalid_argument("bad mnemonic: " + text);
}
