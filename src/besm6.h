#ifndef ALGOL_BESM6_H
#define ALGOL_BESM6_H

#include <string>

//
// BESM-6 instruction (24 bits), decoded.
//
struct Insn {
    unsigned reg;    // index register, bits 24-21
    unsigned opcode; // 000-077 short, 0200-0370 long
    unsigned addr;   // 12 bits (+070000 extension) or 15 bits
    bool is_long;    // long-address format, bit 20

    static Insn decode(unsigned cmd);
};

// Madlen mnemonic, upper case.
std::string besm6_opname(unsigned opcode);

// One instruction from text: [register] mnemonic address, the address in
// octal or * for 4444 (an operand of a pass 4 template).
unsigned besm6_assemble(const std::string &text);

#endif
