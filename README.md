# algc

An Algol compiler for modern 64-bit machines (x86_64, arm64, riscv64), using LLVM as the
backend. It began as a re-creation in C++20 of the BESM-6 Algol-GDR compiler (ALGOL-ГДР,
1982, Dubna monitor system), with UTF-8 source and output.

## Status

The front end (passes 1 and 2) is a complete C++ rewrite of the original: for every
program in `tests/algol` it gives the same listing and diagnostics. The BESM-6 back end that
made the same object modules as the original is on the `besm6` branch. Next: an LLVM backend for native executables on
x86_64, arm64 and riscv64, full Algol-60 with a complete runtime library, and the Algol-60
programs in
[x1-algol-compiler/examples](https://github.com/sergev/x1-algol-compiler/tree/main/examples).
The plan is in [Roadmap.md](Roadmap.md).

## Build

```
make            # build/src/algc
make test       # unit tests and the test programs in tests/algol
```

Needs CMake and a C++20 compiler.

## Usage

```
algc [options...] file.alg
```

| Option | Meaning |
| --- | --- |
| `--list` | Listing of the program; otherwise errors only |
| `--check` | Run-time checks (like `*CHECK`) |
| `--stop-after=N` | Print the stream of pass N (1-2) and stop |

The source is the original notation in UTF-8: keywords in quotes (`'BEGIN'`), strings in
`''…''`, `'EOP'` at the end; only columns 1-72 of a line count. The operators `× ↑ ≥ ≤ ≠ ¬
∧ ∨ ⊃ ≡` may be used for their word forms, and `⏨` is the exponent sign. The listing is
printed in one column with long lines, as the original does with `*call setftn:one,long`.

For now the compiler only checks the program: code generation is being moved to LLVM.

## Related projects

- [os-dubna](https://github.com/besm6/os-dubna): the original compiler in Madlen
  (`algol/`), and documentation (`docs/`).
- [dubna](https://github.com/besm6/dubna): BESM-6 simulator running the Dubna monitor
  system.
- [blang](https://github.com/sergev/bcpl-compiler/tree/master/blang): BCPL compiler with an LLVM IR backend.
- [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler): Algol-60 compiler for
  the Electrologica X1, source of the test programs.
