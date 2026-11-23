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
(the reference oracle, running under `dubna`) for the test corpus. The analysis of the
original is in `doc/`.

---

## Milestone 2 — LLVM backend

Goal: `algc file.alg -o prog` produces a native executable on x86_64, arm64 and riscv64.

- **Typed shared data instead of the workspace.** The passes share data at the original
  addresses (`Workspace`, about 850 accesses), and words hold addresses into it (slots in
  descriptors, D-list addresses in stream items, label and row addresses). The object,
  listing and run goldens become the only reference:
  - drop `PassMem` and the `.mem`-based inputs of the pass tests; keep the stream goldens
    only where items carry no addresses;
  - the named D-list cells (`D_SYM`, `D_LINE`, `D_CONST`, …) become fields of a struct;
  - the name table and the descriptors get their own types, keeping the slot numbers only
    where they reach the object; likewise the constant pool, the label table and the long
    names;
  - then `Workspace` goes away, and with it the original's size limits (one-page streams,
    the hash table, the 1K constant page).
- Find the cleanest point to split the backend: after the last machine-independent pass, or
  an AST/IR built from it. The BESM-6 backend is not kept; that version stays on the
  `besm6` branch.
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
- Tests: the test corpus compiled natively, with output compared to the `dubna` runs.

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
  a clean design is easier to retarget. Milestone 1 was faithful; refactor into an explicit
  IR at the start of Milestone 2.
- **Undocumented internals.** The passes come from disassembly. Their roles and data are mapped
  in `doc/passes.md`, but most handlers of the code generator (pass 4) are still unknown.
- **Arithmetic differences.** BESM-6 float rounding, range and integer width differ from IEEE
  `double` and `int64_t`. Numeric output may not match `dubna` exactly in Milestone 2; the
  comparison tests must allow for that.
- **Character set.** The original works in GOST/KOI-7 codes with upper case only. Decide how
  lower case and non-Cyrillic letters are handled in identifiers and strings.
