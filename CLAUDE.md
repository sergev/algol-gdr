# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

Goal: re-create the BESM-6 **Algol-GDR** compiler (ALGOL-ГДР, 7.01.82, from the Dubna monitor
system) in **C++20**, with UTF-8 input/output. Milestone 0 (groundwork) is done; Milestone 1
is planned in [todo1.md](todo1.md). Plan files `todoN.md` list only the tasks left (IDs like A6.3); delete a
task when it is done, in the same commit.

## Commands

```
make                     # build (CMake, build/), cppcheck runs as part of it
make test                # unit tests via ctest
./build/tests/unit_tests --gtest_filter='stdarray.*'   # one test group
make goldens             # regenerate tests/algol expected outputs (needs dubna)
scripts/ref-compile [--full] tests/algol/X.alg [OUTDIR]   # original compiler → X.std, X.lst
scripts/ref-run X.std    # run an object under dubna
scripts/capture-streams tests/algol/X.alg   # pass streams X.s1…X.s5, X.mem, X.lex
scripts/gen-tables.py > src/tables_data.cpp # tables of the original, from monsys.9
make test                # rebuilds unit_tests; plain make does not
./build/src/algol-gdr [--check] [--setftn=one,long] -o X.std X.alg   # compile; listing on stdout
./build/src/algol-gdr --dump X.std   # decode and disassemble an object
```

Warnings are errors (`-Wall -Werror -Wshadow`, cppcheck `--enable=style`). Format with
`clang-format` (config from `dubna`).

## Layout

- `src/` — compiler: `main.cpp` (CLI), `source.*` (UTF-8 input), `card.*` (lines → KOI-7 cards), `arith.*` (BESM-6 floating point), `pass1*` (lexer, parser), `pass2*`, `pass3*`, `pass4*` (C++ on a `Machine`, see [doc/translation.md](doc/translation.md)), `machine.*` + `translated.*` (BESM-6 state for passes 2–4, shared routines of the overlay), `listing.*` (printer characters, pages), `item.*` (pass stream items), `passes.*` (pass interface, `Context`), `pass5.*` (addresses, line table), `pass6.*` (object assembly), `tables.*` (generated tables of the original), `workspace.*` (its shared data as words), `stdarray.*` (object
  format), `besm6.*` + `dump.*` (disassembly).
- `tests/` — GoogleTest. `tests/algol/` holds test programs with expected outputs from the
  original compiler: `.std` object, `.lst`/`.full.lst` listings, `.out` run output,
  `.s1`…`.s5`/`.mem`/`.lex` pass streams; side files
  `.pre`/`.run`/`.in`/`.norun` ([doc/reference-toolchain.md](doc/reference-toolchain.md)).
  `compile_test` and `run_test` compare our compiler with them, `pass_test` each pass on its
  own (input: the previous expected stream, workspace from `.mem`); skipped until implemented.
- `doc/` — findings about the original compiler: [passes.md](doc/passes.md) (driver, the six
  passes, their streams), [tables.md](doc/tables.md), [symbols.md](doc/symbols.md),
  [runtime-abi.md](doc/runtime-abi.md) (run-time calls, generated code),
  [stdarray.md](doc/stdarray.md) (object format), [encoding.md](doc/encoding.md),
  [translation.md](doc/translation.md) (passes 2–4 on a machine),
  [traces.md](doc/traces.md) (`scripts/trace-compile`).

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

Use the scripts above. Facts they rely on: `*punch` must come **before** `*algol`, or no object
is punched; `dubna -r` fixes the date and makes output deterministic; `dubna` writes
`stdarray.out` into the current directory. Trace instructions with
`dubna -d i --trace=out.txt file.dub`.

## Source language

Original input is upper/lower case with keywords in quotes (`'BEGIN'`, `'EOP'` ends the program,
strings in `''…''` or `'('…')'`), word operators like `'LE'`, `'AND'`, `'POWER'`. Lines are
cards: only columns 1–72 count. The exponent sign is `⏨` (`dubna` turns `@` into `°`).
UTF-8 operator symbols our compiler should accept: `×` (or `*`), `÷`, `↑`, `≥`, `≤`, `≠`, `¬`,
`∧`, `∨`, `⊃`, `≡`, `⏨` (or `@`).
