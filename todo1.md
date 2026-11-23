# Milestone 1 — C++ rewrite, BESM-6 object output

Goal (from [Roadmap.md](Roadmap.md)): from each program in `tests/algol/`, our compiler
produces the same object (`.std`) and listing as the original, in readable C++.

**Where we are.** The whole compiler runs and `compile_test`/`run_test` pass for every
program. All six passes are plain C++ on `Context`; passes 2, 3 and 4 keep the continuation
addresses of the original on their own stack, and their streams are vectors. What we know
about the original is in `doc/`
([passes.md](doc/passes.md), [tables.md](doc/tables.md), [runtime-abi.md](doc/runtime-abi.md),
[stdarray.md](doc/stdarray.md), [encoding.md](doc/encoding.md)).

**Streams.** The `.sN` goldens record the items in the order of the write calls, except
`.s3`: pass 3 rewrites a FOR loop over what it has written, so `NAME.s3` is the stream it
leaves on drum. `PassMem` skips the stream pages, their drum zones, the cursors and the
registers of passes 2-4, and for pass 2 the cells of 01644 and the cursors saved with page
02000.

Guards at every step: `Pass.stream`, `PassMem`, `ListingTest`, `Compile.object`,
`Run.output`. Each task ends with its routines documented in `doc/passes.md`, and one commit.

## Tasks

### Cleanup

- **D18** Low memory (00000-01603, the resident monitor) as the original pass 2 sees
  it: after the first TWICE DECLARED of a block, `open_scope` checks the next ones against
  the "block" at the name slot, whose end word lies there; the original reports every later
  declaration that shadows a binding, ours none (found by fuzzing against `ref-compile`).
  Also the page after ↑EOP↑ BEFORE END OF PROGRAM ends with a form feed in the original.
- **D19** `--full`: the object listing of the sub-overlay `A/PRINTM` (30355…, modules not
  analysed yet); or move it to a later milestone. Its rows are the only cells that can be
  longer than 64 characters (left) or 63 (right, which dubna wraps at 128).
