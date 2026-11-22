# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

Greenfield. Goal: re-create the BESM-6 **Algol-GDR** compiler (ALGOL-ГДР, 7.01.82, from the
Dubna monitor system) in **C++20**, with UTF-8 input/output. No C++ code, build system or tests
exist yet — when adding them, update this file with the real commands.

Milestones (from README.md):
1. Rewrite the Madlen assembler sources into C++; compile simple examples into BESM-6 object
   files (standard arrays) and run them on the `dubna` simulator.
2. Replace the backend: emit LLVM IR and invoke external `clang` to link a native executable.
3. Extend the parser to full Algol-60 and develop a full runtime library.
4. Compile and run all programs from x1-algol-compiler/examples.

Detailed plan: [Roadmap.md](Roadmap.md).

## Reference material (outside this repo)

- `/home/vak/Project/Besm-6/os-dubna/algol` — the original compiler in Madlen assembler,
  reconstructed from disassembly and rebuilt bit-identical to the monsys.9 tape. `tmp/asm-sources/`
  here is a gitignored copy of it (`tmp/` is in `.gitignore` — never commit it).
- `/home/vak/Project/Besm-6/dubna` — C++17 BESM-6 / Dubna emulator; runs Algol-GDR jobs and
  BESM-6 object files. Has its own CLAUDE.md and Algol examples.
- `/home/vak/Project/Lang/bcpl-compiler/blang` — BCPL compiler (Go) that emits LLVM IR and calls
  `clang`; model for the Milestone 2 backend.
- `/home/vak/Project/Algol/x1-algol-compiler/examples/` — Algol-60 programs in publication
  syntax (underlined keywords, X1 I/O procedures); compiling them all is the final goal.
- `/home/vak/Project/Besm-6/os-dubna/docs/` — BESM-6 instruction set, data representation,
  Madlen assembler, Dubna commands, KOI-7 encoding, dynamic loader.

## Structure of the original compiler

- Root overlay `ALGOL` loads at 01604 (octal): driver at 01614 (`algol.asm`, calls the passes and,
  with `*FULL`, loads sub-overlay `A/PRINTM`), tables at 01736–11777 (keyword table at 02166),
  shared `ASMPRINT` at 12000, then the pass modules `agNNNNN.asm` (named after their octal load
  address). Layout is described in `algol.layout.py`.
- Sub-overlay `A/PRINTM` loads at 30355: code listing printer for `*FULL` (`aprintm.asm`,
  `ag312xx–ag314xx.asm`).
- `*.sym` / `*.tsym` hold symbol names for addresses; `*.tsym` comes from execution traces of
  `test*.alg`.

## Running Algol-GDR on the simulator

A job file (see `tmp/algol.dub`):

```
*name test
*algol
'begin'
    print(''Hello, World!'');
'end'
'eop'
*execute
*end file
```

Run with `dubna file.dub`. Options `*full` (listing with code) or `*no list` go before `*algol`.
Trace instructions with `dubna -d i --trace=out.txt file.dub`.

## Source language

Original input is upper/lower case with keywords in quotes (`'BEGIN'`, `'EOP'` ends the program,
strings in `''…''`). UTF-8 operator symbols to accept: `×` (or `*`), `÷`, `↑`, `≥`, `≤`, `≠`, `¬`,
`∧`, `∨`, `⊃`, `≡`, `⏨` (or `@`).
