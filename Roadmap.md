# Roadmap

Port the BESM-6 Algol-GDR compiler (ALGOL-ГДР, 7.01.82, Dubna monitor system) to modern
64-bit machines — x86_64, arm64, riscv64 — as `algc`: first re-create it in C++20 with UTF-8
input and output, then retarget it to native code with LLVM as an external backend, then grow
it to full Algol-60. Final goal: compile the Algol-60 programs in
`/home/vak/Project/Algol/x1-algol-compiler/examples/` (Milestone 4).

The original is about 12,900 lines of Madlen: the root overlay `ALGOL` (driver at 01614, tables
at 01736–11777, the pass modules `ag13666`…`ag30257`) and the listing overlay `A/PRINTM`
(`*FULL` only). The driver calls the passes in sequence (`AG13666`, `AG17223`, `AG22162`,
`AG23523`, `AG26260`, `AG26666`, …), so the C++ structure can follow it pass by pass.

Milestone 1 is done: the C++ compiler produces the same objects and listings as the original
(the reference oracle, running under `dubna`) for the test corpus; it is on the `besm6`
branch, and the analysis of the original is in `doc/`. Milestone 2 is done: a new front end
(lexer, parser, checker → AST) and an LLVM backend (doc/codegen.md) make native executables
on x86_64, arm64 and riscv64, and the 32 test programs with a `dubna` run print the same
output natively. Milestone 3 is done: Algol 60 of the Revised Report (call by name with
conversions, `own`, the full `for`, labels and switches, separate compilation), the
publication input form, messages with columns, the IFIP and Algol-GDR I/O, run-time checks
with `--check`, and a conformance suite in `tests/a60/`; what still differs from the Report
is in doc/deviations.md.

---

## Milestone 4 — The X1 Algol examples

Goal: compile and run every program in `/home/vak/Project/Algol/x1-algol-compiler/examples/`
(about 20 programs: `hello`, `sieve`, `hanoi`, `man_or_boy`, `whetstone`, `lisp`, `pentomino`,
`outer_planets`, …) natively, with the output shown in that directory's `README.md` and `*.md`
files.

- **Publication syntax.** Read since Milestone 3: with the X1 procedures declared as
  `_code`, every example passes the front end but `bulls_and_cows`, which lacks a `;`
  before `NLCR` (line 71).
- **X1 library procedures.** The examples call the Electrologica X1 environment, not
  Algol-GDR's I/O. Add them to the runtime:
  - output: `PRINTTEXT`, `print`, `PRSYM`, `SPACE`, `NLCR`, `ABSFIXT`, `FIXT`, `FLOT`;
  - input: `read`, `RESYM`, and the others used by interactive programs such as
    `bulls_and_cows`;
  - helpers: `SUM`, `entier`, and the rest.

  The full set and their semantics are in `x1-algol-compiler/library/` and `doc/`. Output
  formats must match X1's so the expected outputs compare exactly.
- **Stress cases.**
  - `man_or_boy`: deep recursion with call by name and procedure parameters. Needs efficient
    thunks and a large stack.
  - `lisp`: a Lisp interpreter written in Algol, many procedures and arrays.
  - `pentomino`: a long-running search; good for checking performance at `-O2`.
  - `whetstone`: floating-point accuracy and speed.
  - `quinio`, `quine`: exact character I/O.
- **Test integration.** A test target that compiles each example, runs it (with the stdin
  inputs from the README or the `*.sh` scripts), and compares the output with what the X1
  simulator (`x1sim`) prints.

Done when every example compiles without changes to its source and produces the same output as
under `x1sim`. Any example skipped on purpose is listed with the reason.

---

## Risks and open questions

- **Undocumented internals.** The passes come from disassembly. Their roles and data are mapped
  in `doc/passes.md`, but most handlers of the code generator (pass 4) are still unknown.
- **Arithmetic differences.** BESM-6 float rounding, range and integer width differ from IEEE
  `double` and `int64_t`: reals may differ from `dubna` in the last digit, which the
  comparison tests allow for.
- **Character set.** The original works in GOST/KOI-7 codes with upper case only; `algc`
  reads UTF-8, keeps identifiers as written (case-sensitive, any length) and takes keywords
  in any case.
