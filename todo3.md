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

### Conformance

- **F14** A conformance suite in `tests/a60/`: the examples of the Revised Report (Absmax,
  Innerproduct, Euler, Step, …), Knuth's man or boy for k up to 12 (stack size noted),
  Wichmann's Algol 60 test programs where available, and the dubna Algol examples; each with
  its expected output, run on the three targets by `run_test`.
- **F15** doc/deviations.md final: the deviations that remain, with the reason for each;
  Roadmap.md and README.md updated.
