# Milestone 2 — LLVM backend

Goal (from [Roadmap.md](Roadmap.md)): `algc file.alg -o prog` makes a native executable on
x86_64, arm64 and riscv64, and every program in `tests/algol/` that ran under `dubna`
prints the same output natively.

**Where we are.** The front end (lexer, parser, checker → AST) and the code generator to
LLVM IR (doc/codegen.md) are done; all 32 test programs with a `.out` run natively and match
`dubna`, on x86_64, aarch64 and riscv64.
The BESM-6 compiler is on the `besm6` branch.

**How.** As in `blang` (`/home/vak/Project/Cursor/blang`, `driver.go`): the compiler writes
textual LLVM IR and runs `clang` to compile and link. No LLVM libraries are linked into
`algc`. The IR generator works on the AST.
The runtime library is C in `runtime/`, built with the compiler.

**Guards.** The listings (`.lst`), the ASTs (`.ast`) and the messages (`.err`) keep the
front end unchanged; the frozen `.out` files from `dubna` are the reference for the native
runs.

## Tasks

All tasks of Milestone 2 are done; Milestone 3 is next (Roadmap.md).
