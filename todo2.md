# Milestone 2 — LLVM backend

Goal (from [Roadmap.md](Roadmap.md)): `algc file.alg -o prog` makes a native executable on
x86_64, arm64 and riscv64, and every program in `tests/algol/` that ran under `dubna`
prints the same output natively.

**Where we are.** The front end is new (E7.1): a lexer, a parser to a syntax tree and a
checker that builds the AST (doc/ast.asdl), with GNU-style messages. The original's passes
are gone from `main`; the BESM-6 compiler is on the `besm6` branch.

**How.** As in `blang` (`/home/vak/Project/Cursor/blang`, `driver.go`): the compiler writes
textual LLVM IR and runs `clang` to compile and link. No LLVM libraries are linked into
`algc`. The IR generator works on the AST.
The runtime library is C in `runtime/`, built with the compiler.

**Guards.** The listings (`.lst`), the ASTs (`.ast`) and the messages (`.err`) keep the
front end unchanged; the frozen `.out` files from `dubna` are the reference for the native
runs.

## Tasks

### Code generation

Semantics on the host (decided in E11, written in doc/codegen.md): `integer` is `i64`,
`real` is `double`, `Boolean` is `i1` in registers and `i8` in memory, a string is a pointer
to a NUL-terminated UTF-8 array.

- **E11** doc/codegen.md: data representation, frames, calling convention for procedures,
  name parameters, labels, arrays, runtime names. Update it with each task below.
- **E12** Expressions and assignment: constants, simple variables (locals as `alloca`,
  globals of the outer block), arithmetic with integer/real conversion, `/` and `**` (the
  runtime's `A/RHOCHI`, `A/IHOCHI`, `POWER` rules), relations, Boolean operators,
  conditional expressions, multiple left parts, real-to-integer rounding (`A/RTOI`).
- **E13** PRINT with `printf` on `stdout`, no line buffer of our own: integer `"%14" PRId64 " "`
  (15 columns as the original, wider for big values), real `"% .11e   "` (C's `e` kept:
  ` 1.00000000000e+00   `), boolean `"%-6s"` in lower case (`true  `, `false `), string as
  is, `NEWLINE` `'\n'`, `NEWPAGE` `'\f'`. No carriage-control blank, no wrapping at column
  126, trailing blanks kept. At exit a final `'\n'` when the last character written was not
  one; `stdout` flushed before reading input. `run_test` compares tokens, not text: both
  outputs split on white space, the original's `d.ddd±ee` read as `d.ddde±ee` and
  `TRUE`/`FALSE` as lower case, a difference in the last digit of a real accepted (BESM-6
  rounding); `NAME.native.out` only where the output really differs (upper case only in the
  original). Unit tests of the runtime for the exact layout. Test print, exprs, exprs2,
  arith, boolean.
- **E14** Control flow: statement sequences, IF, GOTO to local labels, compound statements,
  blocks. Test cond, goto, blocks, empty.
- **E15** FOR: STEP-UNTIL (integer and real, the sign of the step known or not), WHILE,
  plain elements, lists of elements; the control variable after the loop. Test for, for2.
- **E16** Standard functions: `ABS`, `SIGN`, `SQRT`, `SIN`, `COS`, `ARCTAN`, `LN`, `EXP`,
  `ENTIER`, and the others pass 2 knows by number. Test stdfunc.
- **E17** Arrays: descriptors (base, bounds, strides), dynamic bounds evaluated on block
  entry, storage freed on block exit (`stacksave`/`stackrestore` or the heap for large
  ones), element access, `--check` bounds checks through the runtime. Test arrays,
  arrays_check.
- **E18** Procedures, by value: nested procedures as LLVM functions with a static link
  (pointer to the enclosing frame), function results, recursion, value parameters
  including arrays by reference. Test proc.
- **E19** Call by name: thunks as `{code, env}` pairs giving the address (or the value
  for expressions); assignment to a name parameter; actual parameters that are constants,
  variables, array elements, expressions, procedures, labels, strings (runtime-abi.md
  `A/PARxxx`). Test man_or_boy, mixed; juggle compiles.
- **E20** Labels and switches across procedures: non-local GOTO out of a procedure or a
  thunk (setjmp/longjmp, only for labels pass 2 marks as used outside), switches and
  designational expressions, labels as parameters. Test switch.
- **E21** Strings: relations, concatenation, slices, `LENGTH`, string parameters. Test
  strings, quine, quinio (with `.in`).
- **E22** _common blocks as named global structs shared between compilation units;
  _code procedures as external C functions with pointer arguments (C calling convention,
  documented). Test common; externals compiles and links against a C stub.

### Targets and finish

- **E23** Cross targets: `--target=aarch64-linux-gnu`, `riscv64-linux-gnu`, with the
  runtime built per target (CMake option or a `runtime/` Makefile like `blang`). `run_test`
  runs the cross executables under `qemu-aarch64` / `qemu-riscv64` when installed, skipped
  otherwise.
- **E24** Every program of `tests/algol` with a `.out` (32 of them) passes natively on x86_64. Where
  the output differs only because of 64-bit arithmetic, keep a separate expected file
  `X.native.out` and say why in doc/codegen.md. `make install` installs `algc` and
  `libalgol.a`; README usage updated.
