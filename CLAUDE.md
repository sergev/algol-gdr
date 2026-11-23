# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

Goal: port the BESM-6 **Algol-GDR** compiler (ALGOL-ГДР, 7.01.82, from the Dubna monitor
system) to modern 64-bit machines (x86_64, arm64, riscv64) as **`algc`**, in **C++20** with
UTF-8 input/output and LLVM as an external backend. Milestones 0 (groundwork) and 1 (the
C++ rewrite, same objects and listings as the original) are done; that rewrite is on the
`besm6` branch. `main` has a new front end (lexer, parser, checker → AST). Plan files `todoN.md` list only the tasks left (IDs like A6.3); delete a
task when it is done, in the same commit.

## Commands

```
make                     # build (CMake, build/), cppcheck runs as part of it
make test                # unit tests via ctest
./build/tests/unit_tests --gtest_filter='cli.*'   # one test group
make test                # rebuilds unit_tests; plain make does not
./build/src/algc X.alg -o X                # executable (clang, libalgol.a)
./build/src/algc --syntax-only [--list] X.alg   # check; listing on stdout, errors on stderr
./build/src/algc --dump-ast X.alg          # print the AST (YAML)
./build/src/algc --emit-llvm X.alg         # X.ll
```

Warnings are errors (`-Wall -Werror -Wshadow`, cppcheck `--enable=style`). Format with
`clang-format` (config from `dubna`).

## Layout

- `src/` — compiler: `main.cpp` (CLI), `driver.*` (runs clang), `codegen.*` (AST → LLVM
  IR), `frontend.*` (parse, check, listing), `source.*`
  (UTF-8 input), `lexer.*` (symbols), `parser.*` and
  `syntax.h` (the syntax tree), `checker.*` (scopes and types: the AST), `ast.*` (the AST
  after doc/ast.asdl, its YAML form), `diag.*` (GNU-style messages), `listing.*`
  (the printout).
- `runtime/` — the run-time library `libalgol.a` (C, uses libc and libm): `main` calls the
  program's `algol_main`; printing, errors. algc finds it in `../runtime` or `../lib` from
  itself (or `$ALGC_RUNTIME`).
- `tests/` — GoogleTest. `tests/algol/` holds test programs with expected outputs from the
  original compiler: `.out` run output under `dubna` (frozen); and ours, which
  `ALGC_UPDATE_GOLDENS=1` rewrites: `.lst` listings (`algc --list`), `.ast` the AST, `.err` the messages; side files
  `.pre` (`*check`), `.in` (input of the run), `.run` (the extra `dubna` cards of the run:
  libraries, Fortran or Madlen externals), `.norun` (why there is no `.out`). The goldens
  from `dubna` are frozen: the tools that made them are on the `besm6` branch.
  `listing_test` checks the `.lst` files, `ast_test` the `.ast` and `.err` files.
- `doc/` — findings about the original compiler (history: the code they describe is on the
  `besm6` branch): [passes.md](doc/passes.md) (driver, passes 1–2, their streams),
  [tables.md](doc/tables.md), [symbols.md](doc/symbols.md),
  [runtime-abi.md](doc/runtime-abi.md) (what the run-time routines do),
  [encoding.md](doc/encoding.md); [algol60.asdl](doc/algol60.asdl) — the abstract syntax the
  parser reads (Algol 60 with the Algol-GDR extensions); [ast.asdl](doc/ast.asdl) — the AST of `algc`, the reference
  for its implementation.

Milestones (from README.md):
1. Rewrite the Madlen assembler sources into C++; compile simple examples into BESM-6 object
   files (standard arrays) and run them on the `dubna` simulator.
2. Replace the backend: emit LLVM IR and invoke external `clang` to link a native executable
   (plan: [todo2.md](todo2.md)).
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

## Source language

UTF-8 text. Keywords are `_` and a word, in any case (`_begin`, `_eop` ends the program); a
blank ends a keyword (`_real_array` is two, `_integer i` needs the blank). Strings in `''…''`
or `'('…')'` (`'ddd'` inside: a character by its octal code point; `′` is a quote too), octal
constants `'777'`, word operators like `_le`, `_and`, `_power`. Messages and the AST give
keywords in lower case, messages as `_begin`. Blanks are ignored inside names and numbers
(`noun phrase` is `nounphrase`). The original wrote keywords in quotes: `'BEGIN'`. Identifiers:
Latin, Greek or Cyrillic letters and digits, any length, case-sensitive, kept as written;
standard identifiers (`PRINT`, `sqrt`, …) in any case. Lines have no column limit. The exponent
sign is `⏨`. Operator symbols: `×`, `↑` (or `^`), `≥`, `≤`, `≠`, `¬`, `∧` (or `&`), `∨`, `⊃`,
`≡`. The original (on `besm6`) read KOI-7 cards, columns 1–72, upper case only, names by their
first 6 characters.
