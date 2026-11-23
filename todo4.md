# Milestone 4 — The X1 Algol examples

Goal (from [Roadmap.md](Roadmap.md)): every program in
`/home/vak/Project/Algol/x1-algol-compiler/examples/` compiles without changes to its
source and runs natively, with the same output as under `x1sim`. Any example skipped on
purpose is listed with the reason.

**Where we are.** Every example passes the front end as it is (`ast_test` x1_examples):
the X1 procedures are standard identifiers, `print` is X1's in the publication form, `read`
in an expression is X1's (doc/ast.asdl). The whole X1 library runs (runtime/x1.c, doc/codegen.md); `tests/x1/` has the examples with their `x1sim` outputs (README there).

**Reference.** The behaviour to match is `x1sim`'s, not the X8 manual's where they differ.
Its sources are in `x1-algol-compiler/`:
- `simulator/opcodes.cpp`: `print`, `SPACE`, `NLCR`, `TAB`, `FLOT`, `FIXT`, `ABSFIXT`, `XEEN`.
- `simulator/machine.cpp`: fixed and floating formats, `RANDOM`, `TIMEOFDAY`, the output
  of `PRSYM`.
- `simulator/input.cpp`: `read`, `RESYM`, the console switches.
- `simulator/encoding.cpp` and `doc/algol-encoding.md`: the symbol codes.
- `library/*.a60`, `*.src`: `SUM`, `PRSYM`, `PRINTTEXT`, `RANDOM`, `SETRANDOM`.
- `doc/library-procedures.md`: the X8 manual.

**Guards.** The goldens and `run_test` (dubna programs, `tests/a60/`, three targets) stay
green.

## Tasks

### Running the examples

- **G6** `run_test` runs `tests/x1/`: each example is compiled, run on the three targets
  and compared with `NAME.out`, or with `NAME.native.out` where the reason is documented.
  - Reals: X1 has a 40-bit mantissa and doubles have 53, so the last of the 13 printed
    digits may differ. Numbers compare with a relative tolerance (e.g. whetstone), as the
    dubna comparison allows.
  - `time`: the output is checked against a pattern, since it is the time of day.
  - `bulls_and_cows`: the secret comes from `SETRANDOM(TIMEOFDAY)`. An environment variable
    (`ALGC_TIMEOFDAY`) fixes the time for the test, and a `.native.out` replaces the
    `x1sim` output. The reason goes in the README.
  - Integers are 64-bit in algc and 27-bit on X1. An example whose output depends on that
    is a documented `.native.out`.
- **G7** The stress cases are correct and fast enough:
  - `man_or_boy` and its stack;
  - `lisp`, with the missionaries and cannibals in lisp.md;
  - `pentomino`, with all four sizes;
  - `sieve` (a million), `sqrt_test` (exact `sqrt`), `whetstone`, and `quine`, whose output
    is its source except for the underlines.

  Time each at `-O1` and `-O2` against `x1sim` and record the times in
  `tests/x1/README.md`. A case that runs too long for `make test` runs only under a
  separate label (`ctest -L slow`). Fix any codegen bug found; each fix gets its own test.

### Finish

- **G8** Documentation:
  - doc/codegen.md: the X1 runtime names.
  - doc/deviations.md: the X1 differences (integer width, real precision, `RANDOM`) and
    any example skipped, with the reason.
  - README.md: how to run X1 programs.
  - Roadmap.md: Milestone 4 marked done.
  - CLAUDE.md: `tests/x1/` in the layout.
