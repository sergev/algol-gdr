# Milestone 0 — Groundwork: detailed plan

Goal (from [Roadmap.md](Roadmap.md)): a skeleton that builds, plus a way to compare our output
with the original compiler, so that Milestone 1 can be developed test-first.

The reference round trip works through `scripts/ref-compile` and `scripts/ref-run`
([doc/reference-toolchain.md](doc/reference-toolchain.md)). In Milestone 1 our compiler must
produce the same object (`.std`) from the same source; the tests in `tests/` are ready for it.

---

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
  Clue: syntax errors (`ERROR nn CLASS`) are printed right after their source line, while
  semantic errors (`IDENTIFIER 'Y' IS NOT DECLARED. NEXT STATEMENT IS 6/ 1`) come after the
  listing, in reverse order — so they come from different passes. See the `err_*.lst` files
  in `tests/algol/`.
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

  Found while writing the test programs (A5.2):
  - source lines are cards: columns 73–80 are ignored, so text past column 72 is lost;
  - the exponent sign is `⏨`; `dubna` turns `@` into `°`, which the compiler rejects;
  - `1.0⏨20` gives `ERROR 11 REAL OVERFLOW` (the BESM-6 range ends near 10¹⁹);
  - `'OWN'` is in the keyword table but gives `ERROR 34` in any position;
  - the keyword table (02166–02545 and on): word operators `'LE'`, `'NOTGREATER'`, `'POWER'`,
    …, I/O statements `'PRINT'`, `'READ'`, `'WRITE'`, `'PUNCH'`, `'ENCODE'`, `'DECODE'`,
    `'REWIND'`, `'BACKSPACE'`, `'ENDFILE'`, and `'CODE'`, `'ALGOL'`, `'COMMON'`, `'LABEL'`,
    `'BITS'`.

## A7. Plan for Milestone 1

- **A7.1** Using the pass map, choose the C++ module structure (one source file per pass or group
  of passes) and the intermediate data types.
- **A7.2** Choose how each pass can be checked on its own: compare its output buffer with a memory
  dump from `dubna` at the end of the pass. Note which passes allow this.
- **A7.3** Write `todo1.md`.

---

## Order and dependencies

```
A6 compiler map ── A7 plan M1
```

Done: A1 (skeleton), A2 (CLI), A3 (reference scripts, see
[doc/reference-toolchain.md](doc/reference-toolchain.md)), A4 (object format,
[doc/stdarray.md](doc/stdarray.md), `src/stdarray.*`, `algol-gdr --dump`), A5 (test corpus in
`tests/algol/` with expected outputs, `make goldens`, compile and run tests that are skipped
until the compiler works).

## Done when

- `make && make test` passes on a clean checkout without `dubna` installed.
- `scripts/ref-compile` and `scripts/ref-run` reproduce the Hello, World round trip.
- Every corpus program has committed `.std`, `.lst` and `.out` files.
- `stdarray` round-trips every `.std` file byte for byte, and the dump tool decodes them.
- `doc/stdarray.md`, `doc/passes.md`, `doc/tables.md`, `doc/runtime-abi.md` and
  `doc/encoding.md` exist and cover every pass and every runtime routine the corpus uses.
- `todo1.md` is written.
