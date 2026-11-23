# Milestone 1 — C++ rewrite, BESM-6 object output

Goal (from [Roadmap.md](Roadmap.md)): from each program in `tests/algol/`, our compiler
produces the same object (`.std`) and listing as the original, in readable C++.

**Where we are.** The whole compiler runs and `compile_test`/`run_test` pass for every
program. Passes 1, 5, 6 are C++; passes 2–4 run as the original's code translated by
`scripts/gen-pass.py` ([doc/translation.md](doc/translation.md)). What we know about the
original is in `doc/` ([passes.md](doc/passes.md), [tables.md](doc/tables.md),
[runtime-abi.md](doc/runtime-abi.md), [stdarray.md](doc/stdarray.md),
[encoding.md](doc/encoding.md)).

**How the rest is done.** A translated pass is replaced routine by routine: a hand-written
routine is registered as a native of the `Machine` at its original address and works on the
machine's memory, so the translated code around it keeps working. When no translated code
of a pass is left, its data moves from `Machine` memory to `Context` (the workspace, streams
as vectors), and `passN_gen.cpp` is deleted. The guards at every step: `Pass.stream`, the
pass's `PassNMem` test, `ListingTest`, `Compile.object`, `Run.output`. Each task ends with
its routines documented in `doc/passes.md` (V where verified).

## Tasks

### Pass 2 (G17223)

- **C6** Scopes: block open and close over the reversed stream, identifier binding and the
  shadowed entries, `TWICE DECLARED`, `IS NOT DECLARED`, `ILLEGAL USE`.
- **C7** Type checking of expressions: the dispatch by code (21720), types and kinds in M5/M6,
  int→real conversion items, `TYPE ERROR IN CONNECTION WITH …`.
- **C8** Statements: assignment and left parts, IF, FOR, GOTO and switches, `NO ASSIGNMENT
  TO`.
- **C9** Procedures: declarations and formal parameters, calls and actual-formal
  correspondence, value parameters, 'CODE'/'ALGOL' externals, standard procedures and I/O
  statements (`PRINT`, `LENGTH`, …).
- **C10** Pass 2 fully native: data in `Context`, streams as vectors, drop `pass2_gen.cpp`;
  `Pass2Mem` compares the workspace without the stream pages. Decode the pass-2 output codes
  completely in passes.md.

### Pass 3 (G22162)

- **C11** Test programs for what C1 shows unrun in pass 3 (FOR loops with several arrays,
  nested loops, steps of both signs, loops it gives up on).
- **C12** The copy loop and the simple rewrites: constant folding of unary minus and int→real,
  adjacent procedure declarations, EOP.
- **C13** FOR loops, first scan: the classification table 22264, when a loop is given up.
- **C14** FOR loops, rewrite: register choice (23154), subscripts linear in the control
  variable, the preamble, the increments, inner loops (`167 … 166`); drop `pass3_gen.cpp`.

### Pass 4 (G23523)

- **C15** Test programs for what C1 shows unrun in pass 4, and a `*check` variant of the array
  programs (expected outputs with `A/SUBCH`); a `--check` option for the `*CHECK` card, apart
  from `--setftn`.
- **C16** The frame of pass 4: init (names from drum 26 zone 0, prologue), reading and the
  dispatch by code (26056), emitting half-words and pseudo-items (23655), EOP.
- **C17** The template interpreter (23712, 23734, templates at 25747) and the operand stack.
- **C18** Arithmetic, constants and assignment.
- **C19** Booleans, relations, IF, GOTO, labels and switches.
- **C20** Blocks and storage: frame offsets, block entry (25525), own variables.
- **C21** Arrays: declarations and bounds, subscripts (24324), the `A/SUBCH` call in check
  mode, the access forms made by pass 3 (`162`–`165`).
- **C22** FOR loops, including the loops rewritten by pass 3.
- **C23** Procedures: declarations, parameters by value and by name, calls, function results,
  procedures as parameters.
- **C24** I/O, strings, external names (the table at 27245, G30034), 'CODE'/'ALGOL'
  procedures, 'COMMON'; then pass 4 fully native, drop `pass4_gen.cpp`.

### Cleanup

- **C25** The translator: remove `machine.*`, `translated.*` and the shared translated code if
  nothing uses them, or keep `gen-pass.py` and `trace-diff.py` documented as tools. Remove
  the stream-page parts of the `.mem` checks that no longer apply.
- **C26** Pass 1 parser as readable functions instead of the labels and the explicit stack of
  the original, under the same tests.
- **C27** Listing details not covered yet: a left cell over 64 characters, a right cell over
  63 (dubna wraps), characters that print as nothing; test programs for them.
- **C28** `--full`: the object listing of the sub-overlay `A/PRINTM` (30355…, modules not
  analysed yet); or move it to a later milestone.
