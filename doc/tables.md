# Tables of the compiler

The fixed tables of `algc`: what names the compiler knows before it reads a program, and
what each becomes. They are C++ tables in the sources named in each section.

## Keywords (src/lexer.cpp, `keywords()`)

The keyword words in lower case, without `_`, mapped to their `Sym`. A keyword in the
source is looked up in lower case, so `_BEGIN` and `_Begin` are `_begin`. doc/symbols.md
lists the symbols and the words of each. `comment` maps to `ERROR` there only as a marker:
the lexer skips the comment.

## Standard identifiers (src/checker.cpp, `standard_names()`)

Names that need no declaration. Each is recognized only in the case given here; a
declaration of the same name hides it. A name that matches only when case is ignored gets a
hint: `Undeclared identifier 'SQRT' (the standard one is 'sqrt')` (`standard_spelling`).

Parameters: `i` integer, `r` real (an integer converted), `s` string, `c` channel (an
integer), `I`, `R` an integer or real destination (a variable or an element).

### Functions

| Name | Parameters | Type | AST | Code |
|---|---|---|---|---|
| `abs` | r | real | `StdFunc(Abs)` | `llvm.fabs.f64` |
| `sign` | r or i | integer | `StdFunc(Sign)` | inline |
| `sqrt` | r | real | `StdFunc(Sqrt)` | `algol_sqrt` |
| `sin`, `cos`, `arctan` | r | real | `StdFunc(Sin)`, … | libm `sin`, `cos`, `atan` |
| `ln` | r | real | `StdFunc(Ln)` | `algol_ln` |
| `exp` | r | real | `StdFunc(Exp)` | libm `exp` |
| `entier` | r or i | integer | `StdFunc(Entier)` | `llvm.floor.f64`, `fptosi` |
| `length` | s | integer | `StdFunc(Length)` | `algol_string_length` |
| `maxreal`, `minreal`, `epsilon` | | real | a `RealConst` | IEEE double's |
| `maxint` | | integer | an `IntConst` | 2⁶³ − 1 |
| `read` | | real | `StdFunc(Read)` | `algol_x1_read` |
| `RESYM` | | integer | `StdFunc(Resym)` | `algol_x1_resym` |
| `XEEN` | i | integer | `StdFunc(Xeen)` | `algol_x1_xeen` |
| `EVEN` | i | integer | `StdFunc(Even)` | inline: 1 for even, −1 for odd |
| `RANDOM` | | real | `StdFunc(Random)` | `algol_x1_random` |
| `TIMEOFDAY` | | real | `StdFunc(Timeofday)` | `algol_x1_timeofday` |
| `SUM` | I, i, i, r | real | `StdFunc(Sum)`, `r` by name | inline loop |

### Procedures

| Name | Parameters | AST | Code |
|---|---|---|---|
| `print` | values, strings, whole arrays, `NEWLINE`, `NEWPAGE` | `Io(Print)` | `algol_print_*` |
| `read` | variables, elements, whole arrays | `Io(Read)` | `algol_read_*` |
| `NEWLINE` (`NEWLIN`) | | `Io(Print)` of `NewLine` | `algol_newline` |
| `NEWPAGE` (`NEWPAG`) | | `Io(Print)` of `NewPage` | `algol_newpage` |
| `stop`, `STOP` | | `StdProc(Stop)` | `algol_stop` |
| `ininteger` | c, I | `StdProc(InInteger)` | `algol_ininteger` |
| `inreal` | c, R | `StdProc(InReal)` | `algol_inreal` |
| `insymbol` | c, s, I | `StdProc(InSymbol)` | `algol_insymbol` |
| `outinteger` | c, i | `StdProc(OutInteger)` | `algol_outinteger` |
| `outreal` | c, r | `StdProc(OutReal)` | `algol_outreal` |
| `outstring` | c, s | `StdProc(OutString)` | `algol_outstring` |
| `outsymbol` | c, s, i | `StdProc(OutSymbol)` | `algol_outsymbol` |
| `outterminator` | c | `StdProc(OutTerminator)` | `algol_outterminator` |
| `fault` | s, r | `StdProc(Fault)` | `algol_fault` |
| `PRINTTEXT` | s | `StdProc(PrintText)` | `algol_x1_printtext` |
| `PRSYM` | i | `StdProc(PrSym)` | `algol_x1_prsym` |
| `SPACE` | i | `StdProc(Space)` | `algol_x1_space` |
| `TAB`, `NLCR` | | `StdProc(Tab)`, `StdProc(Nlcr)` | `algol_x1_tab`, `algol_x1_nlcr` |
| `FIXT`, `ABSFIXT` | i, i, r | `StdProc(Fixt)`, `StdProc(AbsFixt)` | `algol_x1_fixt`, `algol_x1_absfixt` |
| `FLOT` | i, i, r or i, r | `StdProc(Flot)`; `FLOT(n, x)` is `FLOT(n, 2, x)` | `algol_x1_flot` |
| `SETRANDOM` | r | `StdProc(SetRandom)` | `algol_x1_setrandom` |
| `LOAD`, `LOADGO`, `ASSIGN`, `ATOS`, `STOA` | any | `StdProc(Load)`, … | none: the code generator reports them as not supported |

`read` is a function without arguments and a procedure with them. A function used as a
statement, or a procedure in an expression, is `Illegal use of 'NAME'`.

## I/O statements (src/checker.cpp, `io_stmt`)

| Keyword | `IoOp` | Code |
|---|---|---|
| `_read` without a format | `Read` | `algol_read_*`, as `read(…)` |
| `_read (unit, format)` | `Read` | `algol_fread_begin`, `algol_fread_*`, `algol_fread_end` |
| `_print`, `_write` | `Write` | `algol_format_begin`, `algol_format_*`, `algol_format_end` |
| `_punch` | `Punch` | as `_print` |
| `_decode (n, format, s)` | `Decode` | `algol_decode_begin`, then as `_read` |
| `_encode (n, format, s)` | `Encode` | `algol_encode_begin`, `algol_format_*`, `algol_encode_end` |
| `_rewind`, `_backspace`, `_endfile` | `Rewind`, … | `algol_rewind`, `algol_backspace`, `algol_endfile` |

## Items of input and output (src/codegen.cpp, `io_item`)

Each item calls the routine of its statement and its type: the prefix `algol_print_`,
`algol_format_`, `algol_read_` or `algol_fread_`, then `integer`, `real`, `boolean`,
`string`, or `array` for a whole array (with its descriptor and element type); `bits` only
in `print`. An input item is a variable or an element; the routine returns the value and
the code stores it.

## Type and kind codes (src/ast.h, runtime/internal.h)

The same numbers are used by the compiler and the runtime:

| Code | Type (`Type`, `T_*`) | Kind of an actual (`A_*`) | `--check` spec letter: kind, type |
|---|---|---|---|
| 0 | integer | a value | `s` simple, `i` |
| 1 | real | a variable | `a` array, `r` |
| 2 | Boolean | an array | `p` procedure, `b` |
| 3 | string | a procedure | `l` label, `s` |
| 4 | bits | a label | `w` switch, `t` |
| 5 | none | a switch | `n` |

A thunk's tag is type + 16 × kind (doc/codegen.md, Thunks). `algol_check_actuals` gets a
spec of two letters per formal: its kind and its type.
