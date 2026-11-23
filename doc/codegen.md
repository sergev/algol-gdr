# Code generation

How `algc` turns the AST (doc/ast.asdl) into LLVM IR, and what the run-time library
(`runtime/`, `libalgol.a`) provides. The IR is text, made with the builder in src/ir.h; the
driver gives it to `clang` with `libalgol.a` and `-lm`.

## Data

| Algol | In registers | In memory | Notes |
|---|---|---|---|
| `integer` | `i64` | `i64` | |
| `real` | `double` | `double` | |
| `Boolean` | `i1` | `i8` (0, 1) | |
| `bits` | `i64` | `i64` | octal words |
| `string` | `ptr` | `ptr` | NUL-terminated UTF-8, never changed in place |
| label | | `{ptr frame, i64 index}` | a label value, see Labels |
| array | | descriptor | see Arrays |

Every variable, element and parameter cell takes an 8-byte slot. String constants are private
globals; strings made at run time (concatenation, slices) are allocated with `malloc` and
never freed, as on the original's heap.

Conversions: integer to real is `sitofp`. Real to integer (an assignment or a value
parameter) rounds as `A/RTOI`: ⌊x + ½⌋. `/` always gives a real; `÷` is `algol_idiv`, truncating toward zero, an error
when dividing by zero.

## Functions and frames

The program body is `void @algol_main()`. Each procedure is one LLVM function; blocks inside a
function are not functions of their own.

- **Globals.** Variables of `algol_main` (the program and all its inner blocks, outside any
  procedure) are internal globals, `@V.<id>.<name>`: the program is entered once. So are
  `own` variables and the descriptors of `own` arrays, wherever declared; an `own` array
  is allocated when its block is entered with a null data pointer, and never freed.
- **Frames.** A procedure's variables, of all its blocks, live in its frame: an `alloca` of
  8-byte slots, each declaration its own slots. `algol_main` has a frame too, with the header
  only.

  | Slot | Contents |
  |---|---|
  | 0 | static link: the frame of the function the procedure is declared in (`algol_main`'s for a procedure of the program level) |
  | 1 | the `jmp_buf` of the function, or null (see Labels) |
  | 2 | the index of the label a non-local `goto` goes to |
  | 3 | the result of a function procedure |
  | 4… | parameters, variables, array descriptors, temporaries of thunks |

  The frame is zeroed on entry. A formal takes:

  | Formal | Slots |
  |---|---|
  | simple, by value | the value |
  | simple or label, by name | the actual `{code, env, type}` |
  | label, by value | the label value `{frame, index}` |
  | array | the address of its descriptor, the type of the elements |
  | procedure | `{fn, sl, type}`, the type of its result |
  | switch | `{fn, frame}` |

- **Access.** A variable of the function itself: its slot in the frame. A variable of an
  enclosing procedure: through the static links, one per level. A global: directly.

## Procedures

Every procedure has the same signature, so that a procedure passed as a parameter can be
called without knowing its parameters:

```llvm
define internal i64 @P.<id>.<name>(ptr %sl, ptr %args)
```

- `%sl` is the static link; `%args` points to an array of one triple
  `{ptr code, ptr env, i64 tag}` per parameter (a thunk, below), or is null without
  parameters. The word before the first triple holds their number. With `--check` the
  procedure calls `algol_check_actuals` on entry: the number, and the kind and type of
  each actual against its formal.
- The result is an `i64`: an integer as is, a real by `bitcast`, a Boolean by `zext`, a
  string by `ptrtoint`; a procedure without a type returns 0. The caller converts it back.
- On entry the procedure evaluates each value parameter once: it calls the thunk, loads the
  value, converts it to the formal's type and stores it in the frame. A value array is
  copied (`algol_array_copy`) and keeps the type of its elements. Name parameters keep
  their triple in the frame.
- A call: the caller fills the pairs in its frame (an `alloca` at the entry of the
  function), finds the static link of the callee (its own frame if the callee is declared in
  it, otherwise up the static links) and calls. A formal procedure is called through the
  pair `{fn, sl}` its thunk gives.

## Thunks: parameters by name

Every actual parameter is a thunk with the type of its cell, `{ptr code, ptr env, i64
type}`:

```llvm
define internal ptr @T.<n>(ptr %env)
```

`env` is the frame of the function where the call is; the code returns the address of a
cell. The tag is the `Type` of the cell as in src/ast.h (0 integer, 1 real, 2 Boolean,
3 string, 4 bits, 5 none; of the elements for an array, of the result for a procedure),
plus 16 × the kind of the actual: 0 a value, 1 a variable, 2 an array, 3 a procedure,
4 a label, 5 a switch.

| Actual | The cell |
|---|---|
| variable, array element | the variable or the element itself, so it can be assigned |
| expression, constant | a temporary slot of the thunk in `env`, holding the value |
| array | its descriptor |
| procedure | a temporary pair `{fn, sl}` |
| label, designational expression | a temporary label value `{frame, index}` |
| switch | a temporary pair `{fn, frame}` of the switch |
| a formal parameter by name of the caller | its own triple, passed on as is |

The caller reads the cell right after the call, so one temporary per thunk is enough even
in recursion. The cell is in the actual's own type. Where a numeric formal (a name
parameter, an element of a formal array, the result of a formal procedure) meets a cell
of the other numeric type, each access converts: a load by `sitofp` or rounding, a store
back to the cell's type; the choice is a `select` on the type. So an `integer` variable
serves a `real` formal, and calls through a formal procedure need no knowledge of its
formals. A procedure as the actual of a simple formal (passed on through formals
the checker cannot see) is called at each access: its cell is the pair `{fn, sl}` and the
tag says so. An assignment to an expression parameter changes only the temporary.

## Labels and `goto`

- A `goto` to a label of the same function is a `br`; leaving blocks with arrays frees them
  first.
- Any other jump uses a label value `{frame, index}`: the frame of the function the label
  is in, and the label's number there. `algol_goto(frame, index)` stores the index in slot 2
  and calls `longjmp` on the frame's `jmp_buf`.
- A function whose labels can be reached that way (a label used in a switch, as an actual
  parameter, in a thunk, or by a `goto` in a nested procedure) calls `setjmp` at entry, on a
  `jmp_buf` in its frame (`[64 x i64]`, 16-aligned), and on a return by `longjmp` switches
  on slot 2 to the label. Its variables are in memory, so nothing is lost: the frame is
  passed to `algol_array_mark` before the `setjmp`, so that it escapes there and the
  optimizer reads it again after a `longjmp` (otherwise GVN at `-O2` keeps the values
  from before the `setjmp`). Before the label, `algol_array_unwind` frees the arrays
  allocated since the entry of the function (`algol_array_mark`), but for those of the
  blocks around the label: the blocks and the procedures the jump leaves.
- A switch is a function `void @S.<id>.<name>(ptr %frame, i64 %index, ptr %out)` writing
  the label value of element `index` to `out`; an index out of range is a run-time error.
  A switch as a parameter is the pair `{fn, frame}`.
- A formal label keeps the thunk pair of its actual; the thunk gives the address of a label
  value. A `goto` to it, or to a switch element or a conditional designational expression,
  goes through `algol_goto` even when the label is local.
- `setjmp` is glibc's `_setjmp` and the runtime jumps with `_longjmp`: the signal mask is
  not saved.

## FOR

The body is generated once. Each element stores its number in a state cell and jumps to it;
after the body a `switch` on the state goes back to the element. A step-until element
follows the Revised Report: `V := A`; while not `(V − C) × sign(B) > 0`: the body, then
`V := V + B`, with `B` and `C` evaluated again each time, in real when one of `V`, `B`, `C`
is real. After the loop `V` keeps its last value. A subscripted `V` is evaluated at each
use: its subscripts may change in the body.

## Arrays

A descriptor takes `3 + 2·n` slots for `n` dimensions: the data pointer, the number of
elements, the number of dimensions, then the lower and upper bound of each dimension. Elements are 8-byte cells in
row-major order (the last subscript varies fastest). Bounds are evaluated on block entry,
then `algol_array_alloc(desc, n)` allocates zeroed storage; `algol_array_free(desc)` frees
it on block exit. The runtime keeps the live arrays on a stack, for a non-local `goto`
(Labels). An array is passed by the address of its descriptor. With `--check`,
every subscript is checked by `algol_check_index`.

Arrays are row-major (the last subscript varies fastest). The original stored them
column-major, as Fortran: a `_code` procedure that indexes an array must take that into
account (see tests/algol/quinio.c).

## External procedures

- **`_code` procedures** are C functions, called with the address of each argument: a
  variable or an element is its own cell, an array its data (8-byte cells, row-major), an
  expression a temporary holding its value (a string: a `char *`). The result is the
  procedure's type in C: `int64_t` (integer), `double` (real), `bool`, `char *` (string),
  `void`. `_code: _real rndm;` is `double rndm(…)`.
- **Units.** A file of procedures alone (`_real _procedure f(…); …; _procedure g; …`)
  compiles to an object with no `algol_main`: its procedures are global functions under
  their own names, with the Algol convention below. A program declares them `_algol: _real
  f; _algol: g;` and is linked with the object.
- **`_algol` procedures**, compiled separately, use the Algol convention:
  `int64_t name(void *sl, void **args)`, with a null static link; `args` holds
  `{code, env, type}` per parameter.
- **`_common /NAME/ …`** is a global `NAME` with common linkage, `int64_t NAME[n]`: its
  elements in order, a variable one cell, an array its elements. C or another unit sees
  the same storage under the same name.

## Checks

With `--check`: integer `+ − ×` through `llvm.s{add,sub,mul}.with.overflow.i64` and
`algol_check_overflow`; real `/` checks its divisor (`algol_check_divisor`); real results
of arithmetic, `↑` and `exp` must be finite (`algol_check_real`); a real rounded to an
integer must fit (`algol_check_round`); subscripts are in their bounds
(`algol_check_index`); actual parameters as above. Always: switch and string indices,
integer `÷` by zero, overflow in `integer ↑ integer`.

## Standard functions and operators

- `sqrt`, `sin`, `cos`, `arctan`, `ln`, `exp`: libm (`sqrt`, `sin`, `cos`, `atan`, `log`,
  `exp`); `abs`, `sign`, `entier`: inline.
- `↑`: integer↑integer `algol_ipow`, real↑integer `algol_rpow`, any↑real `algol_power`,
  after `A/IHOCHI`, `A/RHOCHI` and `POWER` (doc/runtime-abi.md).
- Strings: `algol_string_compare`, `algol_string_concat`, `algol_string_slice`,
  `algol_string_length`; indices count characters (code points), from 1.

## Output

`print` calls `algol_print_begin`, then for each item `algol_print_integer`,
`algol_print_real`, `algol_print_boolean`, `algol_print_bits`, `algol_print_string`,
`algol_print_array` (its elements in order), `algol_newline`, `algol_newpage`
(runtime/print.c). The layout is X1's: a blank between the values of one call, none
between calls; no line buffer, and nothing is added at the end of the program.

| Item | Format | Example |
|---|---|---|
| integer | `"%" PRId64` | `-1` |
| real | `"%.13g"` | `0.5`, `1e+18`, `3.14159265359` |
| Boolean | `true`, `false` | |
| bits | `"%" PRIo64` | `777` |
| string | as is | |
| `NEWLINE`, `NEWPAGE` | `'\n'`, `'\f'` | |

All output goes through `algol_out`, flushed after each call when stdout is a terminal.
Before a run-time error message a line in progress is ended.

**Tests.** `run_test` compares the output of the programs of tests/algol and tests/a60 with
`NAME.out` by tokens, reals to 1e-10 relative (tests/run_test.cpp); tests/x1 line by line.
The `_code` and `_algol` procedures of quinio and bozp_ybyb are C in `NAME.c` next to the
program.

## Input and output

- The IFIP procedures call `algol_ininteger`, `algol_inreal`, `algol_insymbol`,
  `algol_outinteger`, `algol_outreal`, `algol_outstring`, `algol_outsymbol`,
  `algol_outterminator`, `algol_fault` (runtime/ifip.c), with the channel first; an input
  is stored in its destination with the conversion of its type. `maxreal`, `minreal`,
  `maxint`, `epsilon` are constants; `stop` is `algol_stop`.

- `_write (unit, format)`, `_print (format)` and `_punch (format)` write through
  `algol_format_*` (runtime/format.c): `Iw`, `Fw.d`, `Ew.d` (`0.ddddE+xx`), `Gw.d` (as `E`),
  `Aw`, `Lw`, `nX`, `nH…`, `'…'`, `/`, repeat counts and groups. When the items outrun the
  format it starts again on a new record; the statement ends its record. There is no
  carriage-control column. `_print` and `_punch` go to standard output.
- Units: 54 is the terminal (standard input and output); another unit `n` is the file
  `$ALGC_UNITn`, else `fort.n`, opened at its first use (for writing: emptied).
  `_rewind (n)`, `_backspace (n)` (to the start of the record before) and `_endfile (n)`
  (the file ends here) act on an open unit.
- `_read (unit, format)` reads records through `algol_fread_*` with the same edits: a
  field of width `w`; `Fw.d` without a point takes the last `d` digits as the fraction; a
  width of 0 takes the next word.
- `read(…)` and `_read` without a format read values separated by white space from standard
  input: integers, reals (`⏨` for `e`), `true`/`false`, words as strings; a whole array
  element by element. `stdout` is flushed before reading; bad input stops the program.
  The X1 procedures, IFIP and these share one input stream (runtime/input.c).
- `_encode (n, format, s)` writes the items into the string variable `s`, `n` characters
  (padded with blanks or cut); `_decode (n, format, s)` reads them from the first `n`
  characters of `s`. Records of a string are separated by `'\n'`. The order of the
  parameters is Fortran's `ENCODE (n, format, buffer)`.
- The X1 library (runtime/x1.c), as x1sim prints: `PRINTTEXT` is `algol_x1_printtext`,
  `PRSYM` `algol_x1_prsym` (by the X1 code), `SPACE`, `TAB`, `NLCR` `algol_x1_space`,
  `algol_x1_tab`, `algol_x1_nlcr`; `print` is above (Output); `FIXT`,
  `ABSFIXT`, `FLOT` are `algol_x1_fixt`, `algol_x1_absfixt`, `algol_x1_flot`. The formats
  round the exact decimal value of the double half up, the same on every target (x1sim
  rounds an exact half either way, by the error of its `long double`).
- X1 input from standard input: `read` is `algol_x1_read`, a number as C++ streams read
  it (`⏨` for `E`); the symbol after it is consumed, as in x1sim. On a terminal it asks
  `Enter number:` and again after a bad number; otherwise a bad number or the end of the
  input stops the program. `RESYM` is `algol_x1_resym`, the X1 code of the next symbol,
  255 at the end. `XEEN (mask)` is `algol_x1_xeen`: a line of binary digits, read at the
  first call.
- `SUM (i, a, b, x)` is inline, the loop of the X1 library's `sum.a60`: `i := a`; while
  `i ≤ b` (b evaluated once) add `x` and step `i`. `EVEN (n)` is inline. `RANDOM` is
  `algol_x1_random` (SplitMix64, not x1sim's generator), seeded by `SETRANDOM (x)` with the
  bits of `x`; `TIMEOFDAY` is `algol_x1_timeofday`, local seconds since midnight, or
  `$ALGC_TIMEOFDAY` when set.

## Names

| Name | What |
|---|---|
| `@algol_main` | the program |
| `@V.<id>.<name>` | a variable or array descriptor of `algol_main`, or an `own` one |
| `@P.<id>.<name>` | a procedure |
| `@T.<n>` | a thunk |
| `@S.<id>.<name>` | a switch |
| `@algol_*` | the runtime |

`<id>` is the declaration's id in the AST, so names are unique; names that are not plain
ASCII are quoted.
