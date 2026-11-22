# Milestone 0 — Groundwork: detailed plan

Goal (from [Roadmap.md](Roadmap.md)): a skeleton that builds, plus a way to compare our output
with the original compiler, so that Milestone 1 can be developed test-first.

The reference round trip already works (in `tmp/`):

```
dubna algol-compile.dub   # *algol … *punch  → stdarray.out (object module, card text)
dubna algol-run.dub       # stdarray.out cards + *execute → program output
```

In Milestone 1 our compiler must produce the same `stdarray.out` from the same source.
Milestone 0 builds everything around that comparison.

---

## A1. Repository skeleton

- [x] **A1.1** Layout:
  - `src/` — compiler sources;
  - `tests/` — GoogleTest and test data;
  - `tests/algol/` — the test programs and their expected outputs;
  - `scripts/` — reference toolchain wrappers;
  - `doc/` — format and internals notes.
- [x] **A1.2** `CMakeLists.txt`: C++20, `-Wall -Werror -Wshadow`, GoogleTest fetched at configure time,
      `ctest` integration. Follow `dubna`'s CMake setup.
- [x] **A1.3** Top-level `Makefile` wrapper like `dubna`'s: `make`, `make debug`, `make test`,
      `make clean`.
- [x] **A1.4** `.clang-format` copied from `dubna` (Google base, 4-space indent, 100 columns, WebKit
      braces).
- [x] **A1.5** Keep `tmp/` ignored. Move what stays useful from it (job templates, test programs) into
      `tests/algol/`.

Done when `make && make test` passes with a placeholder test.

## A2. CLI skeleton

- [x] **A2.1** `algol-gdr [options] file.alg`, option parsing with `getopt_long`.
- [x] **A2.2** Options:
  - `-o FILE` — the object file (default `stdarray.out`);
  - `--full`, `--no-list` — the same as the `*full` and `*no list` control cards;
  - `-h`, `-V`.
- [x] **A2.3** Read the source as UTF-8 and report file errors. Compilation itself returns "not
      implemented" for now.
- [x] **A2.4** Tests for usage, version and a missing file (like `dubna/tests/cli_test.cpp`).

## A3. Reference toolchain scripts

Wrap `dubna` so that any `.alg` file can be compiled and run by the original compiler.

- [x] **A3.1** `scripts/ref-compile FILE.alg [OUTDIR]`:
  - put the source into a `*name / *algol / … / *punch / *end file` job;
  - run `dubna` in a temporary directory, because it writes `stdarray.out` (and `punch.out`)
    into the current directory;
  - save `FILE.std` (the object cards) and `FILE.lst` (the printed listing) in OUTDIR;
  - exit with an error if no `stdarray.out` was produced, i.e. the program did not compile.
- [x] **A3.2** `scripts/ref-run FILE.std`: put the cards into a `*name / … / *execute / *end file` job,
      run `dubna`, print the program output with the simulator banner and statistics removed.
- [x] **A3.3** Variant: `--full` adds `*full` before `*algol`, which puts the code listing (A/PRINTM) in
      the `.lst` file.
- [x] **A3.4** Check that the output is deterministic: compile the same source twice and diff. Find
      what varies between runs (date and time in the listing, `dubna` randomization — see
      `dubna -r`) and filter it or turn it off.
- [x] **A3.5** The listing contains the load map (`PROGRAM 01000`, `A/START`, `A/IPRI`, …). Keep it in
      the expected output: it shows which runtime routines a program pulls in.

## A4. Standard array (object module) format

The object module is the contract between Milestone 1 and `dubna`, so it needs a precise
description and tooling.

- [ ] **A4.1** **Card text form**, as written by `dubna/puncher.cpp` (`punch_stdarray`) and read back
      in a job:
  - a header line `` `77761 NAME  N``, where NAME is the six-character label from columns
    77–80 and N is the card number;
  - then 16 lines of `` `oooooooooooooooo `` (one 48-bit word each);
  - the last card is padded with zeros.

  Check how the `dubna` card reader parses it: whether checksums or the column 1–4 fields
  matter, and whether a shorter last card is accepted.
- [ ] **A4.2** **Logical layout.** Write it down from `os-dubna/docs/Writlib.md` (entries, the
      *structure* header, declarations, calls, long names, data, instructions, constants), from
      the WRITLIBR source (`besmmons/system2.txt`), from `docs/Dynamic_Loader.md`, and from
      Madlen.md §17 (references `4000₈+N` / `40000₈+N` / `74000₈+N`, description table, row 0 =
      module name in TEXT code). The long form that compilers make has a 6-word header.
- [ ] **A4.3** Decode `tmp/stdarray.out` (Hello, World) by hand as the first worked example. Word 1,
      `6062574762415500`, is the name `PROGRAM` in TEXT code.
- [ ] **A4.4** Write `doc/stdarray.md` with the format, field by field.
- [ ] **A4.5** C++ library `src/stdarray.{h,cpp}`:
  - model: a list of 48-bit words, with a typed view for the header, entries, tables and
    code;
  - reader and writer for the card text form, with a round-trip test on every `.std`
    expected-output file;
  - a writer that produces output byte-identical to `dubna`'s `stdarray.out`.
- [ ] **A4.6** Dump tool `algol-gdr --dump FILE.std` (or a separate `stddump`): print the header,
      entries, external references and data, and disassemble the code (reuse the
      instruction-decoding logic of `disbesm6`, or call it). A readable dump makes Milestone 1
      differences easy to find.

## A5. Test corpus and expected outputs

- [ ] **A5.1** Collect the programs into `tests/algol/`:
  - `tmp/asm-sources/test1–4.alg`;
  - `tmp/algol-compile.dub` (Hello, World);
  - from `dubna/examples`: `algol.dub`, `man_or_boy.dub`, `juggle.dub`, `bozp-ybyb.dub`,
    `overlay/hello-algol.dub`, `games/quinio.dub`, `quine/algol.dub`. Extract the program part
    of each job.
- [ ] **A5.2** Write small feature programs, one topic each, in the original notation:
  - an empty program (`'BEGIN' 'END'`);
  - integer and real arithmetic, operator precedence, `↑`, `÷`;
  - Boolean expressions with all logical operators;
  - `if` statements and conditional expressions;
  - `for` with `step/until`, `while` and value lists;
  - labels and `goto`, including a `goto` out of a block;
  - one- and two-dimensional arrays, with constant and dynamic bounds;
  - procedures: value and name parameters, recursion, function procedures, a procedure as a
    parameter;
  - `switch`;
  - `own` variables;
  - nested blocks;
  - comments;
  - `print` of integers, reals and strings, and the other Algol-GDR I/O procedures found in the
    runtime (`A/IPRI`, `A/RPRI`, `A/BPRI`, `A/SPRI`, `A/LPRI`, `A/NLINE`, `A/NPAGE`, …);
  - the standard functions (`abs`, `sqrt`, `sin`, `entier`, …).
- [ ] **A5.3** Erroneous programs: syntax errors, undeclared identifiers, type mismatches. Record the
      original's diagnostics.
- [ ] **A5.4** For each program, store the expected output next to it:
  - `.std` — the object module;
  - `.lst` — the listing, and the `*full` listing as `.full.lst`;
  - `.out` — the run output.

  Commit them, so tests do not need `dubna`.
- [ ] **A5.5** `make goldens` regenerates all expected-output files with the scripts from A3 (A3.1, A3.2).
      Run it only on purpose, and review the diff.
- [ ] **A5.6** Test driver in `tests/`: for each program, run our compiler and compare with the `.std`
      file. All these tests are expected to fail until Milestone 1. Mark them as such, so they
      don't break `make test`.
- [ ] **A5.7** Optional tests that use `dubna` (run our `.std` files and compare with `.out`). They are
      skipped when `dubna` is not in `PATH`.

## A6. Map of the original compiler

The main risk of the project: the Madlen sources come from disassembly and have no comments
about their meaning.

- [ ] **A6.1** Regenerate execution traces: `make trace` in `os-dubna/algol` (or the copy in
      `tmp/asm-sources`). Add new test programs to the traced set, so all passes and branches
      of interest are covered.
- [ ] **A6.2** **Driver** (`algol.asm`, `ALGOL` at 01614): the order of passes, the conditions between
      them (`FLGEX*`, `CHECKFL*`, `FULISFL*`, `NOLISFL*`), where `*full` / `*no list` are set,
      and how the compiler finishes (the object module going to the punch or to the library).
- [ ] **A6.3** **Each pass module** (`ag13666`, `ag14504`, `ag17223`, `ag22162`, `ag23523`, `ag26260`,
      `ag26666`, and the small ones `ag27126`…`ag30257`). For each, find:
  - its purpose (input, lexing, parsing, code generation, …);
  - its input and output, and where they live (memory buffers such as `AD06000` with 2048
    words, the drum, scratch files);
  - its main subroutines;
  - the error messages it can print.

  The time a pass runs and the data it touches can be read from the traces (`dubna -d im`).
- [ ] **A6.4** **Tables** at 01736–11777:
  - the keyword table at 02166 and the internal symbol codes;
  - the character-code conversion (GOST/ISO card codes to internal codes);
  - the error message texts;
  - the code templates, if there are any.
- [ ] **A6.5** **Runtime interface.** List the `A/…` library routines that compiled code calls. The load
      map shows `A/START`, `A/STOP`, `A/DRUCK`, `A/IPRI`, `A/RPRI`, `A/BPRI`, `A/SPRI`,
      `A/LPRI`, `A/NLINE`, `A/NPAGE`, `A/I*ISO`, `A/R*ISO`, `A/ARRD/K`, `A/ARRD/V`, and more for
      larger programs. For each: its calling convention, registers, and how parameters and
      strings are passed. Find their sources or disassembly in the Dubna library.
- [ ] **A6.6** **Generated code conventions.** From the `*full` listings: the program prologue and
      epilogue, the layout of variables, array descriptors, procedure calls and returns,
      display/static link, and the parameters passed by name.
- [ ] **A6.7** Give meaningful names to the symbols in `algol.sym` as their roles become clear. Keep
      the names in step with the docs.
- [ ] **A6.8** Deliverables:
  - `doc/passes.md` — the driver and each pass;
  - `doc/tables.md` — the tables;
  - `doc/runtime-abi.md` — the runtime interface and generated-code conventions;
  - `doc/encoding.md` — source character set: the original card codes and the UTF-8 mapping,
    including the Unicode operators from the README and the quoted keywords.

## A7. Plan for Milestone 1

- [ ] **A7.1** Using the pass map, choose the C++ module structure (one source file per pass or group
      of passes) and the intermediate data types.
- [ ] **A7.2** Choose how each pass can be checked on its own: compare its output buffer with a memory
      dump from `dubna` at the end of the pass. Note which passes allow this.
- [ ] **A7.3** Write `todo1.md`.

---

## Order and dependencies

```
A1 skeleton ─┬─ A2 CLI
             └─ A4 stdarray library ──┐
A3 ref scripts ── A5 corpus/goldens ──┼── A7 plan M1
A6 compiler map ──────────────────────┘
```

A3 and A6 need no C++ and can start at once. A4 needs both the documentation work
and code. A6 is the longest; start it early and continue it in parallel.

## Done when

- `make && make test` passes on a clean checkout without `dubna` installed.
- `scripts/ref-compile` and `scripts/ref-run` reproduce the Hello, World round trip.
- Every corpus program has committed `.std`, `.lst` and `.out` files.
- `stdarray` round-trips every `.std` file byte for byte, and the dump tool decodes them.
- `doc/stdarray.md`, `doc/passes.md`, `doc/tables.md`, `doc/runtime-abi.md` and
  `doc/encoding.md` exist and cover every pass and every runtime routine the corpus uses.
- `todo1.md` is written.
