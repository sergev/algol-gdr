# Milestone 0 — Groundwork: detailed plan

Goal (from [Roadmap.md](Roadmap.md)): a skeleton that builds, plus a way to compare our output
with the original compiler, so that Milestone 1 can be developed test-first.

The reference round trip works through `scripts/ref-compile` and `scripts/ref-run`
([doc/reference-toolchain.md](doc/reference-toolchain.md)). In Milestone 1 our compiler must
produce the same object (`.std`) from the same source; the tests in `tests/` are ready for it.

---

## A6. Map of the original compiler

The main risk of the project: the Madlen sources come from disassembly and have no comments
about their meaning. Tools: `scripts/trace-compile` shows which modules a program runs, in
what order, and the coverage of the corpus ([doc/traces.md](doc/traces.md)). Add test programs
for modules with low coverage as their roles become clear.

- **A6.4** **Tables** at 01736–11777:
  - the keyword table at 02166 and the internal symbol codes;
  - the character-code conversion (GOST/ISO card codes to internal codes);
  - the error message texts;
  - the code templates, if there are any.
- **A6.7** Give meaningful names to the symbols in `algol.sym` as their roles become clear. Keep
  the names in step with the docs.
- **A6.8** Deliverables:
  - `doc/tables.md` — the tables;

  Done: `doc/encoding.md` (with the findings of A5.2), `doc/runtime-abi.md` (A6.5–A6.6),
  `doc/passes.md` (A6.2–A6.3).

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
