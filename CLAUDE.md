# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

Goal: port the BESM-6 **Algol-GDR** compiler (ALGOL-ГДР, 7.01.82, from the Dubna monitor
system) to modern 64-bit machines (x86_64, arm64, riscv64) as **`algc`**, in **C++20** with
UTF-8 input/output and LLVM as an external backend. Milestones 0 (groundwork) and 1 (the
C++ rewrite, same objects and listings as the original) are done; the BESM-6 back end
(passes 3–6) is on the `besm6` branch, `main` keeps passes 1–2. Plan files `todoN.md` list only the tasks left (IDs like A6.3); delete a
task when it is done, in the same commit.

## Commands

```
make                     # build (CMake, build/), cppcheck runs as part of it
make test                # unit tests via ctest
./build/tests/unit_tests --gtest_filter='cli.*'   # one test group
make test                # rebuilds unit_tests; plain make does not
./build/src/algc [--list] [--check] X.alg   # check; errors (listing with --list) on stdout
./build/src/algc --stop-after=2 X.alg      # print the pass-2 stream
./build/src/algc --dump-ast X.alg          # print the AST (YAML)
```

Warnings are errors (`-Wall -Werror -Wshadow`, cppcheck `--enable=style`). Format with
`clang-format` (config from `dubna`).

## Layout

- `src/` — compiler: `main.cpp` (CLI), `source.*` (UTF-8 input), `card.*` (lines → KOI-7 cards, ISO text words), `arith.*` (BESM-6 floating point), `pass1*` (lexer, parser), `pass2*` (scopes, type checks), `listing.*` (printer characters, one-column printout), `item.*` (pass stream items), `passes.*` (pass interface, `Context`, driver), `tables.*` (the name table, D-list and 'COMMON' table the passes share), `symbols.*` (the tables decoded after pass 2), `ast.*` (the AST after doc/ast.asdl, its YAML form), `ast_builder.*` (the AST from the pass-2 stream), `word.h` (48-bit word).
- `tests/` — GoogleTest. `tests/algol/` holds test programs with expected outputs from the
  original compiler: `.lst` listings (the original's from the banner on, without the line
  table and summary of passes 5–6; made now by `algc --list --check`),
  `.out` run output under `dubna`, `.s1`/`.s2` pass streams, `.lex` lexer symbols, `.sym` the decoded tables, `.ast` the AST (ours: `ALGC_UPDATE_GOLDENS=1` rewrites them); side files
  `.pre` (`*check`), `.in` (input of the run), `.run` (the extra `dubna` cards of the run:
  libraries, Fortran or Madlen externals), `.norun` (why there is no `.out`). The goldens
  from `dubna` are frozen: the tools that made them are on the `besm6` branch.
  `listing_test`, `pass_test` and `symbols_test` compare our compiler with them; `ast_test` checks the `.ast` files.
- `doc/` — findings about the original compiler: [passes.md](doc/passes.md) (driver, passes
  1–2, their streams), [tables.md](doc/tables.md), [symbols.md](doc/symbols.md),
  [runtime-abi.md](doc/runtime-abi.md) (what the run-time routines do),
  [encoding.md](doc/encoding.md); [ast.asdl](doc/ast.asdl) — the AST of `algc`, the reference
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

Original input is upper/lower case with keywords in quotes (`'BEGIN'`, `'EOP'` ends the program,
strings in `''…''` or `'('…')'`), word operators like `'LE'`, `'AND'`, `'POWER'`. Lines are
cards: only columns 1–72 count. The exponent sign is `⏨` (`@` is a letter, printed `°`).
Cards are encoded as `dubna` does; UTF-8 operator symbols our compiler also accepts (where the
original gives ERROR 01): `×`, `↑`, `≥`, `≤`, `≠`, `¬`, `∧`, `∨`, `⊃`, `≡` (`÷` reads as `≥`,
as in the original).
