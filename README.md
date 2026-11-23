# algc

An Algol compiler for modern 64-bit machines (x86_64, arm64, riscv64), using LLVM as the
backend. It began as a re-creation in C++20 of the BESM-6 Algol-GDR compiler (ALGOL-ГДР,
1982, Dubna monitor system), with UTF-8 source and output.

## Status

The front end reads the language of the original and builds a typed AST; its messages are
GNU style (`file.alg:12: error: …`). The C++ rewrite of the original compiler, which made
the same listings and object modules, is on the `besm6` branch. Next: an LLVM backend for native executables on
x86_64, arm64 and riscv64, full Algol-60 with a complete runtime library, and the Algol-60
programs in
[x1-algol-compiler/examples](https://github.com/sergev/x1-algol-compiler/tree/main/examples).
The plan is in [Roadmap.md](Roadmap.md).

## Build

```
make            # build/src/algc
make test       # unit tests and the test programs in tests/algol
```

Needs CMake, a C++20 compiler and clang (to compile the generated LLVM IR).

**Cross targets.** For each `TRIPLE` with a sysroot in `/usr/TRIPLE` (as installed by
`gcc-aarch64-linux-gnu` or `gcc-riscv64-linux-gnu`), the build also makes the runtime for that
target, `runtime/TRIPLE/libalgol.a`; set `-DALGC_TARGETS="…"` to choose them. Then
`algc --target=aarch64-linux-gnu x.alg` makes an executable for that machine, and the tests
run the test programs under `qemu-aarch64` / `qemu-riscv64` when they are installed.

## Usage

```
algc [options...] files...
```

Inputs are `.alg` (Algol), `.ll` (LLVM IR), `.s`, `.o` and `.a`. The compiler writes LLVM IR
and runs `clang` (from `PATH`, or `$ALGC_CLANG`) to make the output; by default an executable
named after the first `.alg` file, linked with `libalgol.a` and `-lm`.

| Option | Meaning |
| --- | --- |
| `-o FILE` | Output file |
| `-c`, `-S`, `--emit-llvm` | Stop at an object file, assembly, LLVM IR |
| `--syntax-only` | Only check the program |
| `-O0`…`-O3` | Optimization level (default `-O1`) |
| `-g` | Debug information |
| `-L DIR`, `-l LIB` | Libraries to link with |
| `--target=TRIPLE` | Target machine, as for clang |
| `--save-temps` | Keep the intermediate `.ll` files |
| `-v` | Show the commands run |
| `--list` | Listing of the program |
| `--check` | Run-time checks (like `*CHECK`) |
| `--dump-ast` | Print the AST in YAML, no output |

The source is UTF-8. Keywords are written with a leading underscore, in any case (`_begin`,
`_BEGIN`; the original had them in quotes, `'BEGIN'`), strings in `''…''`, `_eop` at the end.
Identifiers may have any length and are case-sensitive; letters may be Latin, Greek or
Cyrillic. The operators `× ↑ ≥ ≤ ≠ ¬ ∧ ∨ ⊃ ≡` may be used for their word forms, and `⏨` is the
exponent sign. The listing shows the source lines as written, numbered.

For now the compiler only checks the program: code generation is being moved to LLVM.

## Related projects

- [os-dubna](https://github.com/besm6/os-dubna): the original compiler in Madlen
  (`algol/`), and documentation (`docs/`).
- [dubna](https://github.com/besm6/dubna): BESM-6 simulator running the Dubna monitor
  system.
- [blang](https://github.com/sergev/bcpl-compiler/tree/master/blang): BCPL compiler with an LLVM IR backend.
- [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler): Algol-60 compiler for
  the Electrologica X1, source of the test programs.
