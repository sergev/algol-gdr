# Milestone 2 — LLVM backend

Goal (from [Roadmap.md](Roadmap.md)): `algc file.alg -o prog` makes a native executable on
x86_64, arm64 and riscv64, and every program in `tests/algol/` that ran under `dubna`
prints the same output natively.

**Where we start.** Passes 1–6 reproduce the original bit for bit. Passes 1 and 2 are the
machine-independent front end: pass 2 writes a type-checked postfix stream (doc/passes.md,
"Output stream"). Passes 3–6, `printm`, `stdarray`, `besm6`, `dump` and the `dubna`
scripts are BESM-6 only; they stay on the `besm6` branch.

**How.** As in `blang` (`/home/vak/Project/Cursor/blang`, `driver.go`): the compiler writes
textual LLVM IR and runs `clang` to compile and link. No LLVM libraries are linked into
`algc`. Between the stream and the IR there is a tree (AST), so the IR generator and the
later milestones (full Algol-60, publication syntax) do not depend on the stream format.
The runtime library is C in `runtime/`, built with the compiler.

**Guards.** The pass-1 and pass-2 streams (`.s1`, `.s2`) and the listings of the error
programs keep the front end unchanged; the frozen `.out` files from `dubna` are the
reference for the native runs.

## Tasks

### Cut the BESM-6 back end

- **E1** Remove passes 3–6, `printm`, `stdarray`, `besm6`, `dump`, `workspace` parts used only
  by them, the options `--full`, `--dump`, `--stop-after=3..5`, and their tests. The driver
  stops after pass 2. Delete the goldens `.std`, `.full.lst`, `.s3`–`.s5`; strip the run
  goldens `.out` to the program output (no loader table); keep `.s1`, `.s2`, `.mem`,
  `.lex`, `.in`.
- **E2** Listing without the back end: source lines and messages as before; the line-address
  table and the `PROGRAM ДЛИHA` line go. Regenerate `.lst` from our compiler and check by
  diff that only those parts changed. `--check` stays (it will mean bounds checks).
- **E3** Remove the `dubna` tooling: `scripts/ref-*`, `trace-*`, `capture-streams*`,
  `disasm-overlay.py`, `dubna-filter`, `make goldens`; doc/stdarray.md, traces.md,
  reference-toolchain.md, runtime-abi.md part B. Keep runtime-abi.md part A (what the
  run-time routines do) as the specification of the new runtime. Update CLAUDE.md.

### Front end data

- **E4** Typed view of the front-end tables: the name table, descriptors (D-list words),
  constants, strings and 'COMMON' entries decoded into structs (`symbols.h`), read from the
  workspace after pass 2. Unit tests on the goldens' `.mem`.
- **E5** Move pass 1 and pass 2 off `Workspace` onto those structs and drop `Workspace`
  (Roadmap: one-page streams, the hash table, the 1K constant page go with it). The stream
  operands stay slot numbers and D-list indexes, so `.s1`/`.s2` still compare. Then drop
  `.mem` and `PassMem`.

### Tree

- **E6** AST (`ast.h`): program, blocks with declarations (variables, arrays with bound
  expressions, procedures with formals and VALUE, labels, switches), statements
  (assignment with left parts, IF, GOTO, FOR with its element list, calls, compound,
  dummy), expressions with their types from pass 2 (conversions explicit). Source line on
  every statement.
- **E7** Build the AST from the pass-2 stream (`ast_builder.cpp`), a stack machine over the
  postfix items. A `--dump-ast` option prints it; goldens `.ast` for all programs.

### Driver and runtime

- **E8** Command line as `blang`: default output is an executable (`file` for `file.alg`);
  `-c` object, `-S` assembly, `--emit-llvm` IR (`.ll`); `-O0..3` (default `-O1`), `-g`,
  `-v`, `--save-temps`, `-L`, `-l`, `--target=TRIPLE`; inputs `.alg`, `.ll`, `.s`, `.o`,
  `.a`. `clang` found in `PATH` (or `$ALGC_CLANG`). Temporary `.ll` removed unless
  `--save-temps`. CLI tests.
- **E9** Runtime skeleton `runtime/` → `libalgol.a`, built by CMake for the host, installed
  to `lib/`. Unlike `blang`, it uses libc and libm (math functions and number formatting);
  linked with `-lalgol -lm`. The compiler finds it next to itself (build tree) or under the
  install prefix. `main` calls the program's entry `algol_main`.
- **E10** IR emitter (`ir.cpp`): a small text builder for functions, blocks, SSA names,
  types, globals, string constants. Hello, World end to end: `PRINT` of a string,
  `run_test` compiling natively and comparing with `.out`.

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
- **E13** PRINT and the output format: integers in 15 columns, reals in 20 (`±d.ddddddddddd±ee`),
  `TRUE`/`FALSE`, strings, `NEWLINE`, `NEWPAGE`, the line width of the original; output
  buffered and flushed at exit. Test print, exprs, exprs2, arith, boolean.
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
- **E22** 'COMMON' blocks as named global structs shared between compilation units;
  'CODE' procedures as external C functions with pointer arguments (C calling convention,
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
