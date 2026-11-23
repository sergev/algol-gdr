# algc

`algc` is a compiler for the Algol programming language. It turns an Algol program into an
ordinary program you can run on a modern computer: a PC (x86_64) or an ARM or RISC-V machine.

It started as a revival of Algol-GDR, the Algol compiler of the Soviet BESM-6 computer.
It reads that dialect and Algol 60 as the Revised Report defines it, also in the
publication form with underlined keywords (`.a60` files). Programs can use the input and
output of Algol-GDR, of the IFIP report, or of the Electrologica X1 library.

## Build

You need CMake, a C++ compiler and clang.

```
make            # build the compiler: build/src/algc
make test       # run the tests
make install    # install to /usr/local
```

## A first program

Write `hello.alg`:

```
_begin
    PRINT(''Hello, World!'')
_end
_eop
```

Compile and run it:

```
$ algc hello.alg
$ ./hello
Hello, World!
```

## Writing programs

- Keywords start with an underscore: `_begin`, `_end`, `_integer`, `_if`, `_then`.
- Strings are in double apostrophes: `''text''`.
- The program ends with `_eop`.
- Use `PRINT(...)` to print numbers and strings, and `NEWLINE` to start a new line; or the
  standard Algol 60 procedures `outinteger(1, ...)`, `outreal(1, ...)`, `outstring(1, ...)`.

```
_begin _integer i;
    _for i := 1 _step 1 _until 5 _do
        PRINT(i, i * i, NEWLINE)
_end
_eop
```

The `tests/algol/` directory has more examples.

## Programs in the publication form

A file with underlined keywords, as Algol 60 was printed, is read in that form, with the
procedures of the Electrologica X1 library (`PRINTTEXT`, `print`, `NLCR`, `FIXT`, `FLOT`,
`read`, `SUM`, …). Write `hello.a60`:

```
b̲e̲g̲i̲n̲
    PRINTTEXT(`Hello, `Algol'');
    NLCR
e̲n̲d̲
```

```
$ algc hello.a60
$ ./hello
Hello, `Algol'
```

The programs of [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler) run this
way, with the output of its X1 simulator; they are in `tests/x1/`, with the Lisp
interpreter, the pentomino solver and others.

## Options

| Option | Meaning |
| --- | --- |
| `-o FILE` | Name of the program to make |
| `-O2` | Make a faster program |
| `--check` | Check array bounds, overflow and procedure arguments while running |
| `--syntax-only` | Only check the program for errors |
| `-c` | Make an object file, to link with other programs |
| `--target=TRIPLE` | Make a program for another machine, as for clang (`aarch64-linux-gnu`) |
| `--help` | All options |

## History

1. The original compiler, about 12,900 lines of BESM-6 assembler reconstructed from a
   Dubna monitor tape, rewritten in C++ with the same object code and listings (on the
   `besm6` branch).
2. A new front end and an LLVM backend: native programs for x86_64, arm64 and riscv64.
3. Algol 60 of the Revised Report: call by name, `own`, switches, the full `for`,
   separate compilation, the publication form, the IFIP input and output, run-time checks.
4. The Electrologica X1 library: the programs of x1-algol-compiler run unchanged, with the
   output of its X1 simulator.

## Learn more

- [doc/deviations.md](doc/deviations.md): how `algc` differs from the Revised Report and
  from the X1 simulator.
- [doc/](doc/): how the compiler works.
- [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler): the Algol 60 programs
  of `tests/x1/` and the X1 simulator.
- [dubna](https://github.com/besm6/dubna): a BESM-6 simulator, where the original compiler
  still runs.
