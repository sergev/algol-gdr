# Milestone 1 — C++ rewrite, BESM-6 object output

Goal (from [Roadmap.md](Roadmap.md)): from each program in `tests/algol/`, our compiler
produces the same object (`.std`) and listing as the original, in readable C++.

**Where we are.** The whole compiler runs and `compile_test`/`run_test` pass for every
program. Passes 1, 5, 6 are plain C++ on `Context`. Passes 2–4 are C++ on a `Machine`
(`machine.*`, `translated.*`): routines registered as natives at their original addresses,
chained by the addresses they return. Their data, stack and registers live in the machine
(passes 3 and 4 are still instruction by instruction: `m.xta`, `m.aex`, …). Their streams
go through the original stream module, which with the driver's print routine 01644, the
formatter 27733 and 30001 is translated code in `shared_gen.cpp`
([doc/translation.md](doc/translation.md)). What we know about the original is in `doc/`
([passes.md](doc/passes.md), [tables.md](doc/tables.md), [runtime-abi.md](doc/runtime-abi.md),
[stdarray.md](doc/stdarray.md), [encoding.md](doc/encoding.md)).

**How the rest is done.** A pass leaves the machine from the inside out: leaf routines first
become functions with real arguments and results, called from the remaining machine-level
handlers through thin adapters; then the handlers, group by group; last the main loop and
dispatch, when the pass becomes a class on `Context` with named state (no accumulator, mode,
stack or index registers). Data of the workspace stays at its original addresses in
`ctx.ws`; cells of the pass's own code area become members; constant tables become named
constants. Guards at every step: `Pass.stream`, `PassMem`, `ListingTest`, `Compile.object`,
`Run.output`. Each task ends with its routines documented in `doc/passes.md`, and one commit.

## Tasks

### Streams and printing

- **D1** Streams as vectors: the stream entries the passes call (27114, 27126 write; 27143,
  27154 read) as C++ over `Stream` vectors; passes 3 and 4 take their input from the
  `Stream` argument as pass 2 does, not from the pages and drums the previous pass left.
  `PassMem` skips the stream pages and their drum zones (they are no longer written).
- **D2** Printing in C++: 01644 and the formatter 27733 as functions on `Listing` (30001 is
  already C++); `shared_gen.cpp`, `scripts/gen-passes`, `gen-pass.py`, `coverage.py`,
  `trace-diff.py` and the `ALGOL_TRACE`/`ALGOL_COVERAGE` hooks removed; docs updated.

### Pass 4 off the machine

- **D3** Leaves: emit (23655), the template interpreter (23734), operand load and save,
  stream writes as functions with arguments; the constants at M7+… as named constants.
- **D4** Operators, IF and jumps, switches.
- **D5** Block entry (25525, without the Madlen-label gotos), labels, frame reset, line
  items, procedure entry and end.
- **D6** Subscripts and arrays, array segments, pass 3's register items.
- **D7** FOR loops, procedure calls, arguments and thunks, VALUE, non-ALGOL calls, G30034.
- **D8** Main loop and dispatch (table 26056) as a C++ loop; `Pass4` a class on `Context`,
  no `Machine`.

### Pass 3 off the machine

- **D9** Main loop, folding, procedure end, EOP, FOR first scan (22232), constants.
- **D10** The FOR rewrite (prefix-tree parser, register allocation, loop end); `Pass3` a
  class on `Context`, no `Machine`.

### Pass 2 off the machine

- **D11** Messages (after_message, formats, positions), scopes and lookup, the emitters.
- **D12** Expressions: operators, types and type errors, subscripts, relations, power.
- **D13** Statements and declarations: assignments, IF, FOR, switches, arrays and bounds,
  procedures, blocks.
- **D14** Calls, actual parameters, thunks, I/O and non-ALGOL arguments; the main loop;
  `Pass2` a class on `Context`, no `Machine`.

### Cleanup

- **D15** Remove `machine.*` and `translated.*`; `ctx.regs` keeps only what a later pass
  reads, and `PassMem` checks only those registers; doc/translation.md retired, CLAUDE.md
  and doc/passes.md updated.
- **D16** Pass 1 parser as readable functions instead of the labels and the explicit stack
  of the original, under the same tests.
- **D17** Listing details not covered yet: a left cell over 64 characters, a right cell over
  63 (dubna wraps), characters that print as nothing; test programs for them.
- **D18** `--full`: the object listing of the sub-overlay `A/PRINTM` (30355…, modules not
  analysed yet); or move it to a later milestone.
