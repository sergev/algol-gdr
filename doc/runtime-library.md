# The run-time library

`libalgol.a` is linked into every program `algc` builds. It gives the standard functions,
input and output, strings and arrays. This manual is for the Algol programmer: what each
routine does when the program calls it. The C interface (argument types, descriptors) is in
[runtime-abi.md](runtime-abi.md); which name calls which routine is in
[tables.md](tables.md).

None of the names below needs a declaration. A declaration of the same name hides it.
Names are recognized only in the case shown: `sqrt`, not `SQRT`; `PRINTTEXT`, not
`printtext`.

## General rules

- **Output** goes to standard output. On a terminal it is flushed at once, so prompts
  appear before input is read.
- **Input** comes from standard input. All input routines share one stream: you may mix
  `read`, `inreal`, `RESYM`, `_read` freely.
- **Errors** stop the program: the line in progress is ended, `Error: ` and the message go
  to stderr, the exit status is 1.
- **The end** of the program flushes output and exits with status 0. No newline is added.
- **Numbers**: integer is 64-bit, real is IEEE double.
- **Strings** are UTF-8; length and indices count characters, from 1.

## Arithmetic

| Algol | What | Errors |
|---|---|---|
| `a ÷ b` | integer quotient, truncated toward zero: `-7 ÷ 2` is `-3` | division by zero |
| `i ↑ j` (both integer) | repeated multiplication | `j < 0`, `0 ↑ 0`, overflow |
| `x ↑ j` (real, integer) | repeated multiplication; `j < 0` gives `1 / x ↑ (-j)` | `0 ↑ 0` |
| `x ↑ y` (y real) | `exp(y × ln x)`; `x ↑ 0` is 1; for `x < 0` and whole `y`, the sign by parity: `(-8) ↑ 3.0` is `-512` | `0 ↑ 0`, `x < 0` with fractional `y`, `0` to a negative power |
| `b ↑ n` (bits) | shift left by `n`, right for `n < 0` | |

## Standard functions

| Function | Value |
|---|---|
| `abs(x)` | absolute value, real |
| `sign(x)` | `-1`, `0` or `1`, integer |
| `sqrt(x)` | square root; error for `x < 0` |
| `sin(x)`, `cos(x)`, `arctan(x)` | as in libm (radians) |
| `ln(x)` | natural log; error for `x ≤ 0` |
| `exp(x)` | eˣ |
| `entier(x)` | largest integer ≤ `x`: `entier(-2.5)` is `-3` |
| `length(s)` | number of characters of string `s` |
| `maxint` | 2⁶³ − 1 |
| `maxreal`, `minreal`, `epsilon` | largest, smallest positive normal, and machine epsilon of double |

## print and read

The common output and input of algc, in the style of the X1.

**`print(a, b, …)`** writes each argument, one blank between them. It does not end the
line.

| Argument | Printed as |
|---|---|
| integer | decimal: `42` |
| real | 13 significant digits (`%.13g`): `0.3333333333333` |
| Boolean | `true` or `false` |
| bits | octal |
| string | as is |
| whole array | all elements in order, row by row |
| `NEWLINE` | a newline; the next value gets no blank |
| `NEWPAGE` | a form feed; the next value gets no blank |

```
print(1, 2.5, _true, `abc'); NEWLINE;      →  1 2.5 true abc
```

**`NEWLINE`**, **`NEWPAGE`** may also be used alone as statements.

**`read`** has two forms.

- `x := read` — a function: the next number from input, real. It skips blanks, accepts a
  sign, digits, point, and an exponent with `e`, `E` or `⏨`. On a terminal it prompts
  `Enter number:` and asks again after a bad number; otherwise a bad number is an error.
- `read(v, …)` — a statement: reads each variable, element or whole array by its type.
  Input is words separated by white space: an integer in decimal, a real (`⏨` allowed),
  a Boolean `true`/`false` in any case, a string as one word. A wrong word is an error
  (`Not an integer in the input: W`), so is the end of input.

`_read` without a format (Algol-GDR) is the same as `read(…)`.

**`stop`**, **`STOP`** end the program at once, status 0.

## IFIP procedures

The input/output procedures of the IFIP modified report. Channel `0` is standard input,
`1` standard output; any other channel is an error.

| Procedure | What |
|---|---|
| `ininteger(c, i)` | reads an integer word into `i` |
| `inreal(c, x)` | reads a real word into `x` |
| `insymbol(c, s, i)` | reads one character; `i` is its position in `s` (from 1), or minus its code point if not in `s` |
| `outinteger(c, i)` | `i` in decimal, then a blank |
| `outreal(c, x)` | `x` with 15–17 digits, the fewest that read back exactly, then a blank: `0.1` |
| `outstring(c, s)` | `s` as is |
| `outsymbol(c, s, n)` | character `n` of `s`; for `n < 0`, the character with code point `−n` |
| `outterminator(c)` | a blank |
| `fault(s, x)` | error: prints `s` and `x`, stops the program |

```
outinteger(1, 42); outreal(1, 0.1); outsymbol(1, `abc', 2);   →  42 0.1 b
```

## The X1 library

Procedures of the Electrologica X1/X8 ALGOL system, as `x1sim` of x1-algol-compiler runs
them. Source: the X8 manual, chapter 7 (x1-algol-compiler/doc/library-procedures.md).
Differences from the X1 are listed in [deviations.md](deviations.md). Unlike the X1, lines
are not broken at 144 positions and pages not at 60 lines.

### Output

**`PRINTTEXT(s)`** — prints string `s`, without its quotes.

**`PRSYM(n)`** — prints the symbol with X1 code `n`: 0–9 digits, 10–35 `a`–`z`, 37–62
`A`–`Z`, 64 `+`, 65 `-`, 66 `×`, 70 `>`, 72 `=`, 87 `,`, 88 `.`, 89 `⏨`, 91 `;`,
93 space, 98 `(`, 99 `)`, 118 tab, 119 newline, and so on
(x1-algol-compiler/doc/algol-encoding.md). An unused code prints `⎕`.

**`SPACE(n)`** — `n` blanks.

**`TAB`** — a tab character.

**`NLCR`** — a newline ("new line, carriage return").

**`FIXT(n, m, x)`** — `x` in fixed point: the sign (`+` or `-`), `n` digits before the
point, `m` after, then a blank. Leading zeros become blanks, except the units digit. With
`m = 0` there is no point. `x` is rounded to the last printed digit, an exact half up.
If the value does not fit in `n` digits, or `n`, `m` are bad (`n < 0`, `m < 0`,
`n + m > 21`), it prints `FLOT(13, 3, x)` instead.

```
FIXT(3, 2, -3.14159)   →  "-  3.14 "
FIXT(2, 0, 7)          →  "+ 7 "
FIXT(2, 1, 1234.5)     →  "+.1234500000000⏨+  4 "    (does not fit)
```

**`ABSFIXT(n, m, x)`** — as `FIXT`, but prints `|x|` with a blank for the sign.

```
ABSFIXT(3, 2, -3.14159)  →  "   3.14 "
```

**`FLOT(n, m, x)`** — `x` in floating point: the sign, the point, `n` mantissa digits,
`⏨`, the exponent sign, `m` exponent digits (leading zeros as blanks), a blank. The
mantissa is in [0.1, 1). Needs `1 ≤ n ≤ 13`, `1 ≤ m ≤ 3` and an exponent that fits;
otherwise it prints `FLOT(13, 3, x)`. `FLOT(n, x)` is `FLOT(n, 2, x)`.

```
FLOT(5, 2, 123.456)    →  "+.12346⏨+ 3 "
FLOT(4, 3, -0.00123)   →  "-.1230⏨-  2 "
```

**`print`** — on the X1, one number in 21 positions. In algc `print` is the general
procedure above ([print and read](#print-and-read)); numbers print as `%.13g`, not in the
X1 layout.

### Input

**`read`** — the next number from input; see [print and read](#print-and-read). The X1
name `READ` is not supported; use `read`.

**`RESYM`** — reads one character and returns its X1 code (the table of `PRSYM`). Letters,
digits, operators and punctuation map back; tab is 118, newline 119. `\r` is skipped. The
end of input gives 255. A character with no X1 code is an error.

**`XEEN(n)`** — the console switches, ANDed with `n`. The first call reads one line of
binary digits (up to 27); on a terminal it prompts for them. Later calls reuse the value.

### Other procedures

**`SUM(i, a, b, x)`** — the sum of `x` for `i = a, a+1, …, b`; `x` is evaluated by name
for each `i`. 0 when `b < a`. Real.

```
SUM(i, 1, 4, i × i)    →  30
```

**`EVEN(n)`** — `1` if `n` is even, else `-1`: that is, (−1)ⁿ.

**`RANDOM`** — a pseudo-random real in [0, 1). The generator is SplitMix64 with a fixed
start, so a run repeats unless seeded.

**`SETRANDOM(x)`** — seeds `RANDOM` with the bits of real `x`. The same seed gives the same
sequence.

**`TIMEOFDAY`** — local time, seconds since midnight, real (with fractions). If
`$ALGC_TIMEOFDAY` is set, its value instead (for tests).

**`LOAD`, `LOADGO`, `ASSIGN`, `ATOS`, `STOA`** — accepted by the parser, but the compiler
reports them as not supported.

## Algol-GDR formatted input and output

Statements of Algol-GDR with a Fortran-style format string.

| Statement | What |
|---|---|
| `_write (u, f) items` | write to unit `u` |
| `_print (f) items`, `_punch (f) items` | write to standard output |
| `_read (u, f) items` | read from unit `u` |
| `_encode (n, f, s) items` | write into string `s`, `n` characters, padded with blanks or cut |
| `_decode (n, f, s) items` | read from the first `n` characters of `s` |
| `_rewind (u)` | back to the start of unit `u` |
| `_backspace (u)` | back to the start of the previous record |
| `_endfile (u)` | the file of `u` ends here |

**Units.** Unit 54 is the terminal (standard input and output). Any other unit `n` is the
file named by `$ALGC_UNITn`, else `fort.n` in the current directory, opened on first use.

**Items** are expressions (on output) or variables and elements (on input). A whole array
is all its elements.

**Edits** of the format:

| Edit | Output | Input |
|---|---|---|
| `Iw` | integer, right in `w` columns | integer from `w` columns |
| `Fw.d` | real, `d` decimals | real; without a point, the last `d` digits are the fraction |
| `Ew.d` | real as `0.ddddE±xx` | as `F` |
| `Gw.d` | as `E` | as `F` |
| `Aw` | string, right in `w` (cut to `w` if longer); `A` as is | `w` characters; `A` the rest of the record |
| `Lw` | `T` or `F` | `T`/`F` (or `.T.`), first letter counts |
| `nX` | `n` blanks | skip `n` columns |
| `nH…`, `'…'` | the text | skipped |
| `/` | end of record | next record |
| `n(…)`, `nI5` | repeat | repeat |

A field too narrow prints asterisks. Blanks in a numeric input field are ignored; a blank
field is 0. On input a width of 0 takes the next word. When the items outrun the format,
a new record starts and the format is used again. Each statement ends its record. There is
no carriage control: every column is printed.

```
_write (54, `(I5, F8.3, 2X, A, L2)') 42, 3.14159, `hi', _true;
                                        →  "   42   3.142  hi T"
_write (54, `(E12.4)') 12345.678;       →  "  0.1235E+05"
_encode (12, `(I4, A5)', s) 77, `abc';  →  s = "  77  abc   "
_decode (6, `(F6.2)', ` 12345') x;      →  x = 123.45
```

## Strings

Algol-GDR `_string` values. A string never assigned is empty.

| Algol | What |
|---|---|
| `length(s)` | number of characters |
| `s[i]` | character `i`, a string |
| `s[i:j]` | characters `i` to `j`; `s[i:i-1]` is empty; out of range is an error |
| `a + b`, … | concatenation |
| `a = b`, `a < b`, … | comparison, byte by byte (UTF-8 order = code point order) |

```
s := `hello'; print(length(s), s[2:4])   →  5 ell
```

## Done by the library for the compiler

These have no Algol name; the compiler calls them. They matter only for the errors they
report.

- **Arrays** are allocated at block entry (zeroed) and freed at block exit. `own` arrays
  live for the whole run. An array passed by value is copied. A `goto` out of a block or
  procedure frees the arrays it leaves.
- **`goto`** out of a procedure jumps back into the frame of the label. `A goto to a label
  that cannot be reached` if that frame is gone.
- **Switch**: `Switch index N out of range`.

### Checks of `--check`

With `algc --check` the code also stops on:

| Error | When |
|---|---|
| `Integer overflow` | an integer `+`, `-`, `×` overflows |
| `Division by zero` | a real `/` by 0 |
| `Real overflow` | a real result is infinite or NaN |
| `Real … too large for an integer` | a real assigned to an integer is out of range |
| `Subscript N out of bounds [lo:hi]` | an array index |
| `Procedure P called with N parameters instead of M`, `Parameter K of P is …` | actual parameters that do not match the formals in number, kind or type |

## Environment

| Variable | Use |
|---|---|
| `ALGC_UNITn` | file of unit `n` for `_write` and `_read` (default `fort.n`) |
| `ALGC_TIMEOFDAY` | value returned by `TIMEOFDAY` |
