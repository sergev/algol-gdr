# algc

`algc` is a compiler for the Algol programming language. It turns an Algol program into an
ordinary program you can run on a modern computer: a PC (x86_64) or an ARM or RISC-V machine.

It started as a revival of Algol-GDR, the Algol compiler of the Soviet BESM-6 computer.
It reads that dialect and Algol 60 as the Revised Report defines it, also in the
publication form with underlined keywords (`.a60` files).

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

## Options

| Option | Meaning |
| --- | --- |
| `-o FILE` | Name of the program to make |
| `-O2` | Make a faster program |
| `--check` | Check array bounds while running |
| `--syntax-only` | Only check the program for errors |
| `--help` | All options |

## Learn more

- [Roadmap.md](Roadmap.md): where the project is going.
- [doc/deviations.md](doc/deviations.md): how `algc` differs from the Revised Report.
- [doc/](doc/): how the compiler works.
- [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler): Algol 60 programs this
  compiler aims to run.
- [dubna](https://github.com/besm6/dubna): a BESM-6 simulator, where the original compiler
  still runs.
