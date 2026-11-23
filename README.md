# Algol-GDR

A re-creation of the BESM-6 Algol-GDR compiler (ALGOL-ГДР, 1982, Dubna monitor system)
in C++20, with UTF-8 source and output.

## Status

The compiler is a complete C++ rewrite of the original: for every program in `tests/algol`
it produces the same BESM-6 object module and the same listing, and the objects run on the
`dubna` simulator with the same output. Next: an LLVM backend for native executables, full
Algol-60 with a complete runtime library, and the Algol-60 programs in
[x1-algol-compiler/examples](https://github.com/sergev/x1-algol-compiler/tree/main/examples).
The plan is in [Roadmap.md](Roadmap.md).

## Build

```
make            # build/src/algol-gdr
make test       # unit tests and the test programs in tests/algol
```

Needs CMake and a C++20 compiler; the tests that run objects need `dubna`.

## Usage

```
algol-gdr [options...] file.alg
```

| Option | Meaning |
| --- | --- |
| `-o FILE` | Object file (default: `file.o` for `file.alg`) |
| `--list` | Listing of the program; otherwise errors and the summary line only |
| `--full` | Also the object code listing (like `*FULL`); implies `--list` |
| `--check` | Run-time checks, no optimization (like `*CHECK`) |
| `--stop-after=N` | Print the stream of pass N (1-5) and stop |
| `--dump` | Print an object file (standard array) |

The source is the original notation in UTF-8: keywords in quotes (`'BEGIN'`), strings in
`''…''`, `'EOP'` at the end; only columns 1-72 of a line count. The operators `× ↑ ≥ ≤ ≠ ¬
∧ ∨ ⊃ ≡` may be used for their word forms, and `⏨` is the exponent sign. The listing is
printed in one column with long lines, as the original does with `*call setftn:one,long`.

The object is a Dubna standard array as punched cards; it runs under `dubna` with
`scripts/ref-run file.o`.

## Related projects

- [os-dubna](https://github.com/besm6/os-dubna): the original compiler in Madlen
  (`algol/`), and documentation (`docs/`).
- [dubna](https://github.com/besm6/dubna): BESM-6 simulator running the Dubna monitor
  system.
- [blang](https://github.com/sergev/bcpl-compiler/tree/master/blang): BCPL compiler with an LLVM IR backend.
- [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler): Algol-60 compiler for
  the Electrologica X1, source of the test programs.
