# Algol-GDR

A re-creation of the BESM-6 Algol-GDR compiler (ALGOL-ГДР, 1982, Dubna monitor system)
in C++20, with UTF-8 source and output.

Plan:

1. Rewrite the original Madlen assembler sources in C++, producing BESM-6 object modules
   that run on the `dubna` simulator.
2. Emit LLVM IR and build native executables with `clang`.
3. Support full Algol-60, with a complete runtime library.
4. Compile the Algol-60 programs in
   [x1-algol-compiler/examples](https://github.com/sergev/x1-algol-compiler/tree/main/examples).

Details are in [Roadmap.md](Roadmap.md).

## Related projects

- [os-dubna](https://github.com/besm6/os-dubna): the original compiler in Madlen
  (`algol/`), and documentation (`docs/`).
- [dubna](https://github.com/besm6/dubna): BESM-6 simulator running the Dubna monitor
  system.
- [blang](https://github.com/sergev/bcpl-compiler/tree/master/blang): BCPL compiler with an LLVM IR backend.
- [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler): Algol-60 compiler for
  the Electrologica X1, source of the test programs.
