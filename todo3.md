# Milestone 3 — Full Algol-60 and runtime

Goal (from [Roadmap.md](Roadmap.md)): a complete Revised Report Algol-60, with the IFIP I/O
procedures and run-time checks, and the remaining deviations documented.

**Where we are.** `algc` compiles Algol-GDR (with `_keywords`) to native code on x86_64,
arm64 and riscv64 (doc/codegen.md). The syntax it reads is doc/algol60.asdl, where `todo`
marks what the Revised Report has and the parser does not read yet. Each task deletes
its lines from doc/deviations.md.

**Guards.** The goldens (`.lst`, `.ast`, `.err`) and `run_test` (the 32 dubna programs, on
three targets) stay green. New programs of this milestone go to `tests/a60/`, with their
expected output written by hand and checked against the Revised Report.

## Tasks

### Input and messages

- **F9** Publication-style input, detected per file: keywords as letters with a combining
  underline (`b̲e̲g̲i̲n̲`, U+0332), strings in `` `…' `` and `‘…’` quotes, nested; `÷`, `≤` and
  the other symbols; no `_eop` needed. The `_keyword` form stays. All the `.a60` files of
  x1-algol-compiler/examples pass `--syntax-only` (their X1 procedures declared as externals
  for now). Listing and messages as for the other form.
- **F10** Messages with columns: `file:line:column: error: …`, the column of the symbol
  where the error is found; tokens and AST nodes keep their column. Goldens updated.

### Runtime

- **F11** The I/O of the IFIP modified report as standard procedures: `inreal`, `outreal`,
  `ininteger`, `outinteger`, `outstring`, `insymbol`, `outsymbol`, `length`, `outterminator`;
  and `maxreal`, `minreal`, `maxint`, `epsilon`, `stop`, `fault`. Channel 0 is standard input,
  1 standard output (documented). Tests.
- **F12** Algol-GDR I/O completed: `_read` with a format, `_decode` and `_encode` (formatted
  conversion to and from strings), `_rewind`, `_backspace`, `_endfile` on units mapped to
  files (`$ALGC_UNITn`, else `fort.n`; 54 is the terminal). Tests.
- **F13** Run-time checks with `--check`: integer overflow (`llvm.*.with.overflow`), division
  by zero, real overflow to infinity, string indices, switch indices, the number and kinds of
  actual parameters through formal procedures. Each check a test that it stops the program
  with a message.

### Conformance

- **F14** A conformance suite in `tests/a60/`: the examples of the Revised Report (Absmax,
  Innerproduct, Euler, Step, …), Knuth's man or boy for k up to 12 (stack size noted),
  Wichmann's Algol 60 test programs where available, and the dubna Algol examples; each with
  its expected output, run on the three targets by `run_test`.
- **F15** doc/deviations.md final: the deviations that remain, with the reason for each;
  Roadmap.md and README.md updated.
