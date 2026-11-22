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

## A5. Test corpus and expected outputs

- **A5.1** Collect the programs into `tests/algol/`:
  - `tmp/asm-sources/test1–4.alg`;
  - `tmp/algol-compile.dub` (Hello, World);
  - from `dubna/examples`: `algol.dub`, `man_or_boy.dub`, `juggle.dub`, `bozp-ybyb.dub`,
    `overlay/hello-algol.dub`, `games/quinio.dub`, `quine/algol.dub`. Extract the program part
    of each job.
- **A5.2** Write small feature programs, one topic each, in the original notation:
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
- **A5.3** Erroneous programs: syntax errors, undeclared identifiers, type mismatches. Record the
  original's diagnostics.
- **A5.4** For each program, store the expected output next to it:
  - `.std` — the object module;
  - `.lst` — the listing, and the `*full` listing as `.full.lst`;
  - `.out` — the run output.

  Commit them, so tests do not need `dubna`. Done for hello and test1–4.
- **A5.5** `make goldens` regenerates all expected-output files with `scripts/ref-compile` and `scripts/ref-run`.
  Run it only on purpose, and review the diff.
- **A5.6** Test driver in `tests/`: for each program, run our compiler and compare with the `.std`
  file. All these tests are expected to fail until Milestone 1. Mark them as such, so they
  don't break `make test`.
- **A5.7** Optional tests that use `dubna` (run our `.std` files and compare with `.out`). They are
  skipped when `dubna` is not in `PATH`.

## A6. Map of the original compiler

The main risk of the project: the Madlen sources come from disassembly and have no comments
about their meaning.

- **A6.1** Regenerate execution traces: `make trace` in `os-dubna/algol` (or the copy in
  `tmp/asm-sources`). Add new test programs to the traced set, so all passes and branches
  of interest are covered.
- **A6.2** **Driver** (`algol.asm`, `ALGOL` at 01614): the order of passes, the conditions between
  them (`FLGEX*`, `CHECKFL*`, `FULISFL*`, `NOLISFL*`), where `*full` / `*no list` are set,
  and how the compiler finishes (the object module going to the punch or to the library).
- **A6.3** **Each pass module** (`ag13666`, `ag14504`, `ag17223`, `ag22162`, `ag23523`, `ag26260`,
  `ag26666`, and the small ones `ag27126`…`ag30257`). For each, find:
  - its purpose (input, lexing, parsing, code generation, …);
  - its input and output, and where they live (memory buffers such as `AD06000` with 2048
    words, the drum, scratch files);
  - its main subroutines;
  - the error messages it can print.

  The time a pass runs and the data it touches can be read from the traces (`dubna -d im`).
- **A6.4** **Tables** at 01736–11777:
  - the keyword table at 02166 and the internal symbol codes;
  - the character-code conversion (GOST/ISO card codes to internal codes);
  - the error message texts;
  - the code templates, if there are any.
- **A6.5** **Runtime interface.** List the `A/…` library routines that compiled code calls. The load
  map shows `A/START`, `A/STOP`, `A/DRUCK`, `A/IPRI`, `A/RPRI`, `A/BPRI`, `A/SPRI`,
  `A/LPRI`, `A/NLINE`, `A/NPAGE`, `A/I*ISO`, `A/R*ISO`, `A/ARRD/K`, `A/ARRD/V`, and more for
  larger programs. For each: its calling convention, registers, and how parameters and
  strings are passed. Find their sources or disassembly in the Dubna library.
- **A6.6** **Generated code conventions.** From the `*full` listings: the program prologue and
  epilogue, the layout of variables, array descriptors, procedure calls and returns,
  display/static link, and the parameters passed by name.
- **A6.7** Give meaningful names to the symbols in `algol.sym` as their roles become clear. Keep
  the names in step with the docs.
- **A6.8** Deliverables:
  - `doc/passes.md` — the driver and each pass;
  - `doc/tables.md` — the tables;
  - `doc/runtime-abi.md` — the runtime interface and generated-code conventions;
  - `doc/encoding.md` — source character set: the original card codes and the UTF-8 mapping,
    including the Unicode operators from the README and the quoted keywords.

## A7. Plan for Milestone 1

- **A7.1** Using the pass map, choose the C++ module structure (one source file per pass or group
  of passes) and the intermediate data types.
- **A7.2** Choose how each pass can be checked on its own: compare its output buffer with a memory
  dump from `dubna` at the end of the pass. Note which passes allow this.
- **A7.3** Write `todo1.md`.

---

## Order and dependencies

```
A5 corpus/goldens ────┬── A7 plan M1
A6 compiler map ──────┘
```

Done: A1 (skeleton), A2 (CLI), A3 (reference scripts, see
[doc/reference-toolchain.md](doc/reference-toolchain.md)), A4 (object format,
[doc/stdarray.md](doc/stdarray.md), `src/stdarray.*`, `algol-gdr --dump`). A6 needs no C++ and is the
longest; continue it in parallel.

## Done when

- `make && make test` passes on a clean checkout without `dubna` installed.
- `scripts/ref-compile` and `scripts/ref-run` reproduce the Hello, World round trip.
- Every corpus program has committed `.std`, `.lst` and `.out` files.
- `stdarray` round-trips every `.std` file byte for byte, and the dump tool decodes them.
- `doc/stdarray.md`, `doc/passes.md`, `doc/tables.md`, `doc/runtime-abi.md` and
  `doc/encoding.md` exist and cover every pass and every runtime routine the corpus uses.
- `todo1.md` is written.
