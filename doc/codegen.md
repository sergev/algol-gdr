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
parameter) rounds as `A/RTOI`: ⌊x + ½⌋. `/` always gives a real.

## Functions and frames

The program body is `void @algol_main()`. Each procedure is one LLVM function; blocks inside a
function are not functions of their own.

- **Globals.** Variables of `algol_main` (the program and all its inner blocks, outside any
  procedure) are internal globals, `@V.<id>.<name>`: the program is entered once.
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

  The frame is zeroed on entry. A formal takes one slot when called by value (a simple
  value) or when it is an array (the address of its descriptor); otherwise two, a pair:
  the thunk of a name parameter or a label, the `{fn, sl}` of a procedure or a switch.

- **Access.** A variable of the function itself: its slot in the frame. A variable of an
  enclosing procedure: through the static links, one per level. A global: directly.

## Procedures

Every procedure has the same signature, so that a procedure passed as a parameter can be
called without knowing its parameters:

```llvm
define internal i64 @P.<id>.<name>(ptr %sl, ptr %args)
```

- `%sl` is the static link; `%args` points to an array of one pair `{ptr code, ptr env}`
  per parameter (a thunk, below), or is null without parameters.
- The result is an `i64`: an integer as is, a real by `bitcast`, a Boolean by `zext`, a
  string by `ptrtoint`; a procedure without a type returns 0. The caller converts it back.
- On entry the procedure evaluates each value parameter once: it calls the thunk, loads the
  value in the formal's type and stores it in the frame. A value array is copied
  (`algol_array_copy`). Name parameters keep their pair in two slots of the frame.
- A call: the caller fills the pairs in its frame (an `alloca` at the entry of the
  function), finds the static link of the callee (its own frame if the callee is declared in
  it, otherwise up the static links) and calls. A formal procedure is called through the
  pair `{fn, sl}` its thunk gives.

## Thunks: parameters by name

Every actual parameter is a thunk, a pair `{ptr code, ptr env}`:

```llvm
define internal ptr @T.<n>(ptr %env)
```

`env` is the frame of the function where the call is (null in `algol_main`, which uses
globals); the code returns the address of a cell:

| Actual | The cell |
|---|---|
| variable, array element | the variable or the element itself, so it can be assigned |
| expression, constant | a temporary slot of the thunk in `env`, holding the value |
| array | its descriptor |
| procedure | a temporary pair `{fn, sl}` |
| label, designational expression | a temporary label value `{frame, index}` |
| switch | a temporary pair `{fn, frame}` of the switch |
| a formal parameter by name of the caller | its own pair, passed on as is |

The caller reads the cell right after the call, so one temporary per thunk is enough even
in recursion. An expression is converted to the type of the formal in the thunk when the
callee is known; through a formal procedure the value comes in the actual's own type (the
types of its formals are not known). An assignment to an expression parameter changes only
the temporary.

## Labels and `goto`

- A `goto` to a label of the same function is a `br`; leaving blocks with arrays frees them
  first.
- Any other jump uses a label value `{frame, index}`: the frame of the function the label
  is in, and the label's number there. `algol_goto(frame, index)` stores the index in slot 2
  and calls `longjmp` on the frame's `jmp_buf`.
- A function whose labels can be reached that way (a label used in a switch, as an actual
  parameter, in a thunk, or by a `goto` in a nested procedure) calls `setjmp` at entry, on a
  `jmp_buf` in its frame (`[64 x i64]`, 16-aligned), and on a return by `longjmp` switches
  on slot 2 to the label. Its variables are in memory, so nothing is lost; arrays of the
  blocks left are not freed.
- A switch is a function `void @S.<id>.<name>(ptr %frame, i64 %index, ptr %out)` writing
  the label value of element `index` to `out`; an index out of range is a run-time error.

## FOR

The body is generated once. Each element stores its number in a state cell and jumps to it;
after the body a `switch` on the state goes back to the element. A step-until element
follows the Revised Report: `V := A`; while not `(V − C) × sign(B) > 0`: the body, then
`V := V + B`, with `B` and `C` evaluated again each time, in real when one of `V`, `B`, `C`
is real. After the loop `V` keeps its last value.

## Arrays

A descriptor takes `3 + 2·n` slots for `n` dimensions: the data pointer, the number of
elements, the number of dimensions, then the lower and upper bound of each dimension. Elements are 8-byte cells in
row-major order (the last subscript varies fastest). Bounds are evaluated on block entry,
then `algol_array_alloc(desc, n)` allocates zeroed storage; `algol_array_free(desc)` frees
it on block exit. An array is passed by the address of its descriptor. With `--check`,
every subscript is checked by `algol_check_index`.

## Standard functions and operators

- `sqrt`, `sin`, `cos`, `arctan`, `ln`, `exp`: libm (`sqrt`, `sin`, `cos`, `atan`, `log`,
  `exp`); `abs`, `sign`, `entier`: inline.
- `↑`: integer↑integer `algol_ipow`, real↑integer `algol_rpow`, any↑real `algol_power`,
  after `A/IHOCHI`, `A/RHOCHI` and `POWER` (doc/runtime-abi.md).
- Strings: `algol_string_compare`, `algol_string_concat`, `algol_string_slice`,
  `algol_string_length`; indices count characters (code points), from 1.

## Output

PRINT calls `algol_print_integer`, `algol_print_real`, `algol_print_boolean`,
`algol_print_string`, `algol_newline`, `algol_newpage`: `printf` on `stdout`, no line buffer
of its own.

| Item | Format | Example |
|---|---|---|
| integer | `"%14" PRId64 " "` | `             -1 ` |
| real | `"% .11e   "` | ` 1.00000000000e+00   ` |
| Boolean | `"%-6s"` | `true  `, `false ` |
| string | as is | |
| `NEWLINE`, `NEWPAGE` | `'\n'`, `'\f'` | |

The fields are the original's (15, 20 and 6 columns), but there is no carriage-control
column and no wrapping at column 126. At exit a last `'\n'` is written when the output does
not end with one.

**Tests.** `run_test` compares the output with `NAME.out` from `dubna` by tokens (see
tests/run_test.cpp). `NAME.native.out` replaces it where the output really differs:

| Program | Why |
|---|---|
| hello, hello_algol | the original prints upper case only |
| symbols | the original's printer shows `≤ ≥ @` as `⩽ ⩾ °` |

## Names

| Name | What |
|---|---|
| `@algol_main` | the program |
| `@V.<id>.<name>` | a variable or array descriptor of `algol_main` |
| `@P.<id>.<name>` | a procedure |
| `@T.<n>` | a thunk |
| `@S.<id>.<name>` | a switch |
| `@algol_*` | the runtime |

`<id>` is the declaration's id in the AST, so names are unique; names that are not plain
ASCII are quoted.
