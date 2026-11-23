# Milestone 1 — C++ rewrite, BESM-6 object output

Goal (from [Roadmap.md](Roadmap.md)): from each program in `tests/algol/`, our compiler
produces the same object (`.std`) and listing as the original, in readable C++.

**Where we are.** The whole compiler runs and `compile_test`/`run_test` pass for every
program. Passes 1, 4, 5, 6 are plain C++ on `Context` (pass 4 keeps the continuation
addresses of the original on its own stack). Passes 2 and 3 are C++ on a `Machine`
(`machine.*`, `translated.*`): routines registered as natives at their original addresses,
chained by the addresses they return. Their data, stack and registers live in the machine
(pass 3 is still instruction by instruction: `m.xta`, `m.aex`, …). Their streams
go through the original stream module (pages, drums, cursors at 02000/02001), a native
instruction by instruction in `translated.cpp` with 01644, 27733 and 30001
([doc/translation.md](doc/translation.md)). What we know about the original is in `doc/`
([passes.md](doc/passes.md), [tables.md](doc/tables.md), [runtime-abi.md](doc/runtime-abi.md),
[stdarray.md](doc/stdarray.md), [encoding.md](doc/encoding.md)).

**Streams.** The `.sN` goldens record the items in the order of the write calls; pass 3
rewrites a FOR loop over what it has written (moving the cursors back, editing page words),
so its stream is read from drum where it left it (`stream_left`), and `NAME.s3` holds that.
When a pass leaves the machine it takes its input from the `Stream` argument as a vector
and writes a vector, and `PassMem` skips the stream pages, their drum zones, the cursors
and the registers of that pass (done for pass 4).

**How the rest is done.** A pass leaves the machine from the inside out: leaf routines first
become functions with real arguments and results, called from the remaining machine-level
handlers through thin adapters; then the handlers, group by group; last the main loop and
dispatch, when the pass becomes a class on `Context` with named state (no accumulator, mode,
stack or index registers). Data of the workspace stays at its original addresses in
`ctx.ws`; cells of the pass's own code area become members; constant tables become named
constants. Guards at every step: `Pass.stream`, `PassMem`, `ListingTest`, `Compile.object`,
`Run.output`. Each task ends with its routines documented in `doc/passes.md`, and one commit.

## Tasks

### Pass 3 off the machine

- **D9** Main loop, folding, procedure end, EOP, FOR first scan (22232), constants.
- **D10** The FOR rewrite (prefix-tree parser, register allocation, loop end) on vectors;
  `Pass3` a class on `Context`, no `Machine`.

### Pass 2 off the machine

- **D11** Messages (after_message, formats, positions), scopes and lookup, the emitters.
- **D12** Expressions: operators, types and type errors, subscripts, relations, power.
- **D13** Statements and declarations: assignments, IF, FOR, switches, arrays and bounds,
  procedures, blocks.
- **D14** Calls, actual parameters, thunks, I/O and non-ALGOL arguments; the main loop;
  streams as vectors; `Pass2` a class on `Context`, no `Machine`.

### Cleanup

- **D15** Remove `machine.*` and `translated.*` (01644, 27733, 30001 to plain
  functions on `Listing`); `ctx.regs` keeps only what a later pass reads, and `PassMem`
  checks only those registers; doc/translation.md retired, CLAUDE.md
  and doc/passes.md updated.
- **D16** Pass 1 parser as readable functions instead of the labels and the explicit stack
  of the original, under the same tests.
- **D17** Listing details not covered yet: a left cell over 64 characters, a right cell over
  63 (dubna wraps), characters that print as nothing; test programs for them.
- **D18** `--full`: the object listing of the sub-overlay `A/PRINTM` (30355…, modules not
  analysed yet); or move it to a later milestone.
