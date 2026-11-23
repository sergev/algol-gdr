# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

Goal: port the BESM-6 **Algol-GDR** compiler (ALGOL-ГДР, 7.01.82, from the Dubna monitor
system) to modern 64-bit machines (x86_64, arm64, riscv64) as **`algc`**, in **C++20** with
UTF-8 input/output and LLVM as an external backend. Milestones 0 (groundwork), 1 (the C++
rewrite, same objects and listings as the original; on the `besm6` branch), 2 (a new
front end and an LLVM backend: native executables for x86_64, arm64, riscv64), 3 (Algol
60 of the Revised Report, the publication form, IFIP I/O, checks; doc/deviations.md) and 4
(the X1 library: the x1-algol-compiler examples run as under `x1sim`) are done; no
milestone is planned next. Plan files `todoN.md` list only the tasks left (IDs like
A6.3); delete a task when it is done, in the same commit.

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
  IR), `ir.*` (the IR text builder), `frontend.*` (parse, check, listing), `source.*`
  (UTF-8 input), `lexer.*` (symbols), `parser.*` and
  `syntax.h` (the syntax tree), `checker.*` (scopes and types: the AST), `ast.*` (the AST
  after doc/ast.asdl, its YAML form), `diag.*` (GNU-style messages, `file:line:column:`), `listing.*`
  (the printout).
- `runtime/` — the run-time library `libalgol.a` (C, uses libc and libm): `main` calls the
  program's `algol_main`; printing, errors; `ifip.c` the IFIP procedures, `format.c`
  Algol-GDR I/O, `x1.c` the X1 library. algc finds it in `../runtime` or `../lib` from
  itself (or `$ALGC_RUNTIME`); for `--target=T` in `../runtime/T` or `../lib/T`, built by
  CMake for each `/usr/T` sysroot (aarch64, riscv64). `run_test` runs the programs under
  qemu for those targets too.
- `tests/` — GoogleTest. `tests/algol/` holds test programs with expected outputs from the
  original compiler: `.out` run output under `dubna` (frozen); and ours, which
  `ALGC_UPDATE_GOLDENS=1` rewrites: `.lst` listings (`algc --list`), `.ast` the AST, `.err` the messages; side files
  `.pre` (`*check`), `.in` (input of the run), `.env` (its environment), `.run` (the extra `dubna` cards of the run:
  libraries, Fortran or Madlen externals), `.norun` (why there is no `.out`). The goldens
  from `dubna` are frozen: the tools that made them are on the `besm6` branch.
  `listing_test` checks the `.lst` files, `ast_test` the `.ast` and `.err` files; `run_test`
  compiles the programs it lists natively and compares their output with `NAME.native.out`
  (where the native run differs for a reason) or `NAME.out`. `tests/a60/` holds the
  conformance programs (outputs by hand); `tests/x1/` the X1 examples with their `x1sim`
  outputs (frozen, made by `tests/x1/goldens.sh`), compared line by line.
- `doc/` — [passes.md](doc/passes.md) (the passes of algc, what each gives),
  [encoding.md](doc/encoding.md) (source characters and symbols, text at run time); findings
  about the original compiler (history: the code they describe, and the original passes.md
  and encoding.md, are on the `besm6` branch): [tables.md](doc/tables.md),
  [symbols.md](doc/symbols.md), [runtime-abi.md](doc/runtime-abi.md) (what the run-time
  routines do); [algol60.asdl](doc/algol60.asdl) — the abstract syntax the
  parser reads (Algol 60 with the Algol-GDR extensions); [ast.asdl](doc/ast.asdl) — the AST of `algc`, the reference
  for its implementation; [deviations.md](doc/deviations.md) — how algc differs from the
  Revised Report, and the task that removes each difference; [codegen.md](doc/codegen.md) — data, frames, calling convention,
  thunks, labels, arrays, runtime names; [ir.asdl](doc/ir.asdl) — the LLVM IR subset algc
  emits, as src/ir.h builds it.

Milestones (all done; the history is in README.md):
1. Rewrite the Madlen assembler sources into C++; compile simple examples into BESM-6 object
   files (standard arrays) and run them on the `dubna` simulator (done, `besm6` branch).
2. Replace the backend: emit LLVM IR and invoke external `clang` to link a native executable
   (done).
3. Extend the parser to full Algol-60 and develop a full runtime library (done; what
   remains different is in [doc/deviations.md](doc/deviations.md)).
4. Compile and run all programs from x1-algol-compiler/examples (done: `tests/x1/`).

## Reference material (outside this repo)

- `/home/vak/Project/Besm-6/os-dubna/algol` — the original compiler in Madlen assembler,
  reconstructed from disassembly and rebuilt bit-identical to the monsys.9 tape. `tmp/asm-sources/`
  here is a gitignored copy of it (`tmp/` is in `.gitignore` — never commit it).
- `/home/vak/Project/Besm-6/dubna` — C++17 BESM-6 / Dubna emulator; runs Algol-GDR jobs and
  BESM-6 object files. Has its own CLAUDE.md and Algol examples.
- `/home/vak/Project/Lang/bcpl-compiler/blang` — BCPL compiler (Go) that emits LLVM IR and calls
  `clang`; the model for the backend of algc.
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

UTF-8 text. Keywords are `_` and a word, in any case (`_begin`, `_eop` may end the program); a
blank ends a keyword (`_real_array` is two, `_integer i` needs the blank). Strings in
`` `…' `` or `‘…’`, nested, the inner quotes kept; in the outer level of `` `…' `` an
apostrophe is `` '` `` (`` `it'`s' ``); the Algol-GDR `''…''` and `'('…')'`
(`'ddd'` inside: a character by its octal code point; `′` is a quote too) are deprecated, a
warning at the first one of a file. Octal constants `'777'`, word operators like `_le`, `_and`, `_power`. Messages and the AST give
keywords in lower case, messages as `_begin`. Blanks are ignored inside names and numbers
(`noun phrase` is `nounphrase`). The original wrote keywords in quotes: `'BEGIN'`. Identifiers:
Latin, Greek or Cyrillic letters and digits, any length, case-sensitive, kept as written;
standard identifiers (`PRINT`, `sqrt`, …) in any case. Lines have no column limit. The exponent
sign is `⏨`. Operator symbols: `×`, `↑` (or `^`), `≥`, `≤`, `≠`, `¬`, `∧` (or `&`), `∨`, `⊃`,
`≡`, `÷`. A file with underlined letters (U+0332) is in the publication form: `b̲e̲g̲i̲n̲`,
strings `` `…' `` as in the other form (`.a60`, as x1-algol-compiler/examples). The X1 library procedures
(`PRINTTEXT`, `FLOT`, `SUM`, `read`, …) are standard identifiers; `print` is X1's in the
publication form, Algol-GDR's `PRINT` otherwise. The original (on `besm6`) read KOI-7 cards, columns 1–72, upper case only, names by their
first 6 characters.
