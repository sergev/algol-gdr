# Roadmap

Re-create the BESM-6 Algol-GDR compiler (ALGOL-ГДР, 7.01.82, Dubna monitor system) in C++20
with UTF-8 input and output, then retarget it to native code via LLVM, then grow it to full
Algol-60. Final goal: compile the Algol-60 programs in
`/home/vak/Project/Algol/x1-algol-compiler/examples/` (Milestone 4).

The original is about 12,900 lines of Madlen: the root overlay `ALGOL` (driver at 01614, tables
at 01736–11777, the pass modules `ag13666`…`ag30257`) and the listing overlay `A/PRINTM`
(`*FULL` only). The driver calls the passes in sequence (`AG13666`, `AG17223`, `AG22162`,
`AG23523`, `AG26260`, `AG26666`, …), so the C++ structure can follow it pass by pass.

The original compiler running under `dubna` is the reference oracle for Milestone 1: for any
input, its listing and object module are the expected output. The groundwork is done: the
reference scripts, the test corpus with expected outputs, and the analysis of the original
in `doc/`. The detailed plan for Milestone 1 is [todo1.md](todo1.md).

---

## Milestone 1 — C++ rewrite, BESM-6 object output

Goal: compile simple programs into Dubna standard modules (standard arrays) that load and run
on `dubna` the same way the original's output does.

1. **Input and lexer.** Read UTF-8 source. Accept the original notation (quoted keywords
   `'BEGIN'`, `''…''` strings, `'EOP'`) and the Unicode symbols `× ÷ ↑ ≥ ≤ ≠ ¬ ∧ ∨ ⊃ ≡ ⏨`,
   with `*` and `@` as ASCII alternatives. Convert to the compiler's internal codes in a
   single place.
2. **Passes.** Translate each pass from Madlen into C++, keeping its algorithm and
   intermediate representation, so outputs can be diffed pass by pass against `dubna` traces.
   Use real types (enums, structs, `std::vector`) instead of raw 48-bit words where the meaning
   is clear, and keep word-exact data where the bit layout reaches the object code.
3. **Code generator.** Emit BESM-6 instructions and constants exactly as the original does.
4. **Object writer.** Write the standard module format (Madlen.md §17; loader in
   Dynamic_Loader.md), including external references to the Algol runtime routines that
   already exist in the Dubna library.
5. **Listing.** The source listing and diagnostics in UTF-8; the `*FULL` code listing
   (A/PRINTM) last.
6. **Integration with `dubna`.** A way to feed the produced object module into a Dubna job
   (in place of `*algol`) and run it with `*execute`.

Done when, for the test corpus:
- the object modules match the original's word for word, or differ only where justified and
  documented;
- the programs run on `dubna` with the same output;
- error messages appear for the same erroneous programs.

## Milestone 2 — LLVM backend

Goal: `algol-gdr file.alg -o prog` produces a native executable.

- Find the cleanest point to split the backend: after the last machine-independent pass, or
  an AST/IR built from it. Keep the BESM-6 backend working as a second target.
- **IR generation** modelled on `blang` (`irbuilder.go`, `driver.go`): emit textual LLVM IR,
  call `clang` to compile and link. Options `-S` (keep `.ll`), `-c`, `-O`.
- **Semantics on a 64-bit host.** Decide and document:
  - `integer` and `real` representation (BESM-6 has 48-bit words with a 40-bit mantissa; on the
    host likely `int64_t` and `double`);
  - `Boolean`;
  - strings (UTF-8);
  - arrays with dynamic bounds;
  - own variables.
- **Procedures.** Recursion, nested scopes (static link or display), parameters by value, and
  by name (thunks), labels and non-local `goto` out of procedures.
- **Runtime library in C** (as `blang/runtime`): the standard functions (`abs`, `sign`, `sqrt`,
  `sin`, `cos`, `arctan`, `ln`, `exp`, `entier`) and the I/O procedures used by Algol-GDR
  programs (`print` and its relatives), with output formatted like the original.
- Tests: the Milestone 1 corpus compiled natively, with output compared to the `dubna` runs.

Done when the whole corpus runs natively with the same output as on `dubna`.

## Milestone 3 — Full Algol-60 and runtime

Goal: a complete Revised Report Algol-60 implementation.

- **Language gaps.** List what Algol-GDR leaves out or restricts compared with the Revised
  Report, and close each one: full call by name, `switch` declarations and designational
  expressions, `own` arrays with dynamic bounds, procedures as parameters, labels as
  parameters, side effects in `for` lists, and so on.
- **Input.** Accept a modern publication-style input form alongside the quoted-keyword one
  (for example, bold keywords written in lower case), selected by an option or detected
  automatically.
- **Diagnostics.** Clear messages with file, line and column; error recovery that continues
  after the first error.
- **Runtime library.** I/O following the IFIP modified report (`inreal`, `outreal`, `ininteger`,
  `outinteger`, `outstring`, …) plus the Algol-GDR procedures. Formatted output, input from
  files and stdin, and run-time checks (array bounds, overflow) that can be turned on.
- **Conformance.** Collect Algol-60 test programs (the Revised Report examples, classic
  algorithm collections, and the `dubna` Algol examples) into a regression suite.

Done when the conformance suite passes, and the remaining known deviations from the Revised
Report are documented.

## Milestone 4 — The X1 Algol examples

Goal: compile and run every program in `/home/vak/Project/Algol/x1-algol-compiler/examples/`
(about 20 programs: `hello`, `sieve`, `hanoi`, `man_or_boy`, `whetstone`, `lisp`, `pentomino`,
`outer_planets`, …) natively, with the output shown in that directory's `README.md` and `*.md`
files.

- **Publication syntax.** Keywords written with combining underline (`b̲e̲g̲i̲n̲`, U+0332 after each
  letter), strings in `` `…' `` quotes, lower-case identifiers, `⏨` exponents. This is the
  modern input form from Milestone 3; these files are its reference.
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

- **Faithful vs clean rewrite.** A literal translation is easier to verify against the original;
  a clean design is easier to retarget. Plan: faithful in Milestone 1, refactor into an explicit
  IR at the start of Milestone 2.
- **Undocumented internals.** The passes come from disassembly. Their roles and data are mapped
  in `doc/passes.md`, but most handlers of the code generator (pass 4) are still unknown.
- **Arithmetic differences.** BESM-6 float rounding, range and integer width differ from IEEE
  `double` and `int64_t`. Numeric output may not match `dubna` exactly in Milestone 2; the
  comparison tests must allow for that.
- **Character set.** The original works in GOST/KOI-7 codes with upper case only. Decide how
  lower case and non-Cyrillic letters are handled in identifiers and strings.
