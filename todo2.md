# Milestone 2 — LLVM backend

Goal (from [Roadmap.md](Roadmap.md)): `algc file.alg -o prog` makes a native executable on
x86_64, arm64 and riscv64, and every program in `tests/algol/` that ran under `dubna`
prints the same output natively.

**Where we are.** The compiler is the front end of the original: passes 1 and 2 (the
BESM-6 back end is on the `besm6` branch). Pass 2 writes a type-checked postfix stream
(doc/passes.md, "Output stream"); `Symbols` (src/symbols.h) decodes the declarations, scopes,
constants and strings its items refer to.

**How.** As in `blang` (`/home/vak/Project/Cursor/blang`, `driver.go`): the compiler writes
textual LLVM IR and runs `clang` to compile and link. No LLVM libraries are linked into
`algc`. Between the stream and the IR there is a tree (AST), so the IR generator and the
later milestones (full Algol-60, publication syntax) do not depend on the stream format.
The runtime library is C in `runtime/`, built with the compiler.

**Guards.** The pass-1 and pass-2 streams (`.s1`, `.s2`), the listings (`.lst`) and the
decoded tables (`.sym`) keep the front end unchanged; the frozen `.out` files from `dubna` are the
reference for the native runs.

## Tasks

### Tree

- **E7.1** Front end without BESM-6 addresses: passes 1 and 2 on real data structures —
  declarations as objects in a scope tree, the name table as a symbol table with scopes,
  constants and strings in their own tables — instead of D-list addresses, name slots,
  the cells at 02000…/04000…/016111… and the continuation addresses of pass 2. Stream
  operands become indexes into these tables, so `.s1`/`.s2` give way to the `.ast` goldens;
  `.lst` must not change, `.sym` prints by name. The size limits of the original (hash
  table, constant page, D-list) go. Blocks without arrays get their own `Block` nodes (the
  stream has no items for them, so `ast_builder` merges their declarations into the
  enclosing block), and declarations get their source positions.
- **E7.2** UTF-8 throughout: no more ISO (KOI-7) or TEXT encoding inside the compiler, and
  no forced uppercase. Source, names, strings and messages stay UTF-8; keywords are
  recognized and printed in lowercase; user identifiers keep their spelling as written.
  Listings and goldens are updated to match.

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
