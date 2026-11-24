# Run-time library

The interface of `libalgol.a` (runtime/, C11): every routine the generated code calls,
declared in runtime/algol.h. doc/codegen.md has the data representation and how the code
uses them; doc/tables.md which Algol name calls which routine.

## Conventions

- The routines are plain C functions with the C calling convention of the target
  (x86_64, aarch64, riscv64). Their names start with `algol_`; the X1 library's with
  `algol_x1_`.
- Algol types in C: integer `int64_t`, real `double`, Boolean `_Bool`, bits `int64_t`,
  string `const char *` (NUL-terminated UTF-8; null is the empty string). A new string is
  a `char *` from `malloc`, never freed.
- An array is passed as its descriptor, `int64_t *`: the data pointer, the number of
  elements, the number of dimensions, then the lower and upper bound of each dimension.
  Elements are 8-byte cells, row-major. Routines that take a whole array also take the type
  of its elements (`T_INTEGER` 0, `T_REAL` 1, `T_BOOLEAN` 2, `T_STRING` 3, `T_BITS` 4).
- `main` (runtime/start.c) calls the program's `void algol_main(void)`, then `algol_stop`.
  It is alone in its file, so a C program with its own `main` can link Algol units.
- An error writes `Error: ` and the message to stderr and exits with status 1. A line of
  output in progress is ended first, so the message starts a line.
- All output goes to stdout through `algol_out` (runtime/print.c), flushed at once when
  stdout is a terminal. All input from stdin goes through runtime/input.c, so the routines
  that read it can be mixed: a byte one of them put back is the next for all.

## Program and control (control.c)

| Routine | What |
|---|---|
| `void algol_main(void)` | the program, made by `algc` |
| `_Noreturn void algol_stop(void)` | flushes stdout, exits with status 0 |
| `_Noreturn void algol_error(const char *fmt, ...)` | the message, as `printf`; exit status 1 |
| `_Noreturn void algol_goto(void **frame, int64_t index)` | a `goto` to a label of another function: stores `index` in `frame[2]`, `_longjmp` to the `jmp_buf` at `frame[1]`; an error when that is null |
| `_Noreturn void algol_switch_error(int64_t index)` | `Switch index N out of range` |

## print (print.c, array.c)

| Routine | What |
|---|---|
| `void algol_print_begin(void)` | a call of `print` starts: no blank before its first value |
| `void algol_print_integer(int64_t)` | decimal |
| `void algol_print_real(double)` | `%.13g` |
| `void algol_print_boolean(_Bool)` | `true` or `false` |
| `void algol_print_bits(int64_t)` | octal |
| `void algol_print_string(const char *)` | as it is |
| `void algol_print_array(const int64_t *desc, int64_t type)` | the elements in order, as above |
| `void algol_newline(void)`, `void algol_newpage(void)` | `\n`, `\f`; the next value gets no blank |

Each value but the first of a call is preceded by a blank. Nothing is added at the end of
the program.

## read and _read without a format (read.c)

| Routine | What |
|---|---|
| `int64_t algol_read_integer(void)` | the next word, a decimal integer |
| `double algol_read_real(void)` | the next word, a number for `strtod`; `⏨` is taken as `e` |
| `_Bool algol_read_boolean(void)` | `true` or `false`, in any case |
| `char *algol_read_string(void)` | the next word, a new string |
| `void algol_read_array(int64_t *desc, int64_t type)` | the elements in order, each by its type |

Words are separated by white space; stdout is flushed before reading. The end of the input
is `End of the input`; a word of the wrong kind is an error (`Not an integer in the input:
W`).

## Formatted input and output (format.c)

| Routine | What |
|---|---|
| `void algol_format_begin(int64_t unit, const char *format)` | `_write`, `_print`, `_punch`: a statement starts |
| `void algol_format_integer(int64_t)`, `_real(double)`, `_boolean(_Bool)`, `_string(const char *)` | an item, by the next data edit of the format |
| `void algol_format_array(const int64_t *desc, int64_t type)` | the elements, each an item |
| `void algol_format_end(void)` | the statement ends: the edits up to the next data edit, the record written |
| `void algol_encode_begin(const char *format)` | `_encode`: output into a string |
| `char *algol_encode_end(int64_t count)` | the text, `count` characters, padded with blanks or cut |
| `void algol_fread_begin(int64_t unit, const char *format)` | `_read` with a format: a statement starts |
| `void algol_decode_begin(const char *format, const char *s, int64_t count)` | `_decode`: input from the first `count` characters of `s` |
| `int64_t algol_fread_integer(void)`, `double …_real`, `_Bool …_boolean`, `char *…_string` | an item, by the next data edit |
| `void algol_fread_array(int64_t *desc, int64_t type)` | the elements, each an item |
| `void algol_fread_end(void)` | the statement ends; the next one starts a new record |
| `void algol_rewind(int64_t unit)`, `algol_backspace`, `algol_endfile` | `_rewind`, `_backspace` (to the start of the record before), `_endfile` (the file ends here) |

Edits: `Iw`, `Fw.d`, `Ew.d`, `Gw.d`, `Aw`, `Lw`, `nX`, `nH…`, `'…'`, `/`, repeat counts and
groups. Unit 54 is the terminal (stdin, stdout); unit `n` is the file `$ALGC_UNITn`, else
`fort.n`. doc/codegen.md has the details.

## IFIP (ifip.c)

| Routine | What |
|---|---|
| `int64_t algol_ininteger(int64_t channel)` | as `algol_read_integer` |
| `double algol_inreal(int64_t channel)` | as `algol_read_real` |
| `int64_t algol_insymbol(int64_t channel, const char *str)` | the next character: its place in `str` from 1, or minus its code point |
| `void algol_outinteger(int64_t channel, int64_t)` | decimal, then a blank |
| `void algol_outreal(int64_t channel, double)` | `%g` with the fewest of 15–17 digits that read back the same, then a blank |
| `void algol_outstring(int64_t channel, const char *)` | as it is |
| `void algol_outsymbol(int64_t channel, const char *str, int64_t n)` | character `n` of `str`; for `n < 0`, the code point `−n` |
| `void algol_outterminator(int64_t channel)` | a blank |
| `_Noreturn void algol_fault(const char *s, double r)` | an error: `s` and `r` |

Channel 0 is stdin, 1 stdout; another is an error.

## The X1 library (x1.c)

| Routine | What |
|---|---|
| `void algol_x1_printtext(const char *)` | `PRINTTEXT`: the string |
| `void algol_x1_prsym(int64_t code)` | `PRSYM`: the symbol of an X1 code, `⎕` for an unused one |
| `void algol_x1_space(int64_t n)`, `algol_x1_tab(void)`, `algol_x1_nlcr(void)` | `n` blanks, a tab, a new line |
| `void algol_x1_fixt(int64_t n, int64_t m, double x)` | `FIXT`: the sign, `n` digits, the point, `m` digits; a blank after |
| `void algol_x1_absfixt(int64_t n, int64_t m, double x)` | `ABSFIXT`: as `FIXT`, a blank for the sign |
| `void algol_x1_flot(int64_t n, int64_t m, double x)` | `FLOT`: `n` digits of mantissa, `m` of exponent after `⏨` |
| `double algol_x1_read(void)` | `read`: the next number; on a terminal it asks `Enter number:`, and again after a bad one |
| `int64_t algol_x1_resym(void)` | `RESYM`: the X1 code of the next character, 255 at the end |
| `int64_t algol_x1_xeen(int64_t mask)` | `XEEN`: the console switches (a line of binary digits, read once), masked |
| `double algol_x1_random(void)` | `RANDOM`: SplitMix64, in [0, 1) |
| `void algol_x1_setrandom(double seed)` | `SETRANDOM`: seeds it with the bits of `seed` |
| `double algol_x1_timeofday(void)` | `TIMEOFDAY`: local seconds since midnight, or `$ALGC_TIMEOFDAY` |

`FIXT`, `ABSFIXT` and `FLOT` round the exact decimal value half up, and print as `FLOT(13,
3, x)` when the number does not fit.

## Arithmetic (arith.c)

| Routine | What | Errors |
|---|---|---|
| `int64_t algol_idiv(int64_t a, int64_t b)` | `a ÷ b`, truncated toward zero | division by zero |
| `int64_t algol_ipow(int64_t, int64_t)` | `integer ↑ integer` | a negative exponent, `0 ↑ 0`, overflow |
| `double algol_rpow(double, int64_t)` | `real ↑ integer`; a negative exponent divides into 1 | `0 ↑ 0` |
| `double algol_power(double x, double y)` | `↑ real`: 1 for `y = 0`; an integral `y` of a negative `x` takes its sign from the parity; else `exp(y × ln x)` | `0 ↑ 0`, a negative `x` to a fractional power, 0 to a negative power |
| `int64_t algol_bits_shift(int64_t, int64_t n)` | `bits ↑ n`: shifted left by `n`, right for `n < 0` | |
| `double algol_sqrt(double)`, `double algol_ln(double)` | `sqrt`, `ln` | a negative argument; for `ln`, one not above 0 |

## Checks of --check (arith.c, check.c, array.c)

| Routine | Checks |
|---|---|
| `void algol_check_overflow(_Bool)` | integer overflow, from `llvm.s*.with.overflow` |
| `void algol_check_divisor(double)` | a real divisor of 0 |
| `double algol_check_real(double)` | a finite result; returns it |
| `void algol_check_round(double)` | a real that fits in an integer |
| `void algol_check_index(const int64_t *desc, int64_t dim, int64_t i)` | subscript `i` of dimension `dim` (from 0) in its bounds |
| `void algol_check_actuals(void **args, int64_t n, const char *spec, const char *name)` | the number of actuals (the word before `args`), and the kind and type of each from its tag, against `spec` (doc/tables.md) |

The code calls them only with `--check`.

## Strings (string.c)

| Routine | What |
|---|---|
| `int64_t algol_string_length(const char *)` | the number of characters (code points) |
| `int64_t algol_string_compare(const char *, const char *)` | −1, 0 or 1, by bytes |
| `char *algol_string_concat(const char *, const char *)` | a new string |
| `char *algol_string_slice(const char *s, int64_t first, int64_t last)` | `s[first:last]`, a new string; an index out of range is an error |
| `char *algol_string_element(const char *s, int64_t i)` | `s[i]` |

## Arrays (array.c)

| Routine | What |
|---|---|
| `void algol_array_alloc(int64_t *desc, int64_t dims)` | the element count from the bounds in `desc`; zeroed data; the array on the stack of live arrays |
| `void algol_array_free(int64_t *desc)` | frees it at block exit |
| `void algol_array_alloc_own(int64_t *desc, int64_t dims)` | an `own` array: not on the stack, never freed |
| `int64_t algol_array_mark(void *frame)` | the height of the stack; `frame` is passed so that it escapes before a `setjmp` |
| `void algol_array_unwind(int64_t mark)` | frees the arrays above `mark`: after a non-local `goto` |
| `int64_t *algol_array_copy(const int64_t *src, int64_t dims)` | a new descriptor and data with the bounds and elements of `src`: an array by value |
| `void algol_array_release(int64_t *desc)` | frees a copy and its descriptor |
| `void algol_array_bind(int64_t *desc, int64_t dims, int64_t *data)` | an array of a `_common` block: its elements at `data` |

## Environment

| Variable | Use |
|---|---|
| `ALGC_UNITn` | the file of unit `n` of the formatted statements (default `fort.n`) |
| `ALGC_TIMEOFDAY` | the value of `TIMEOFDAY`, for tests |
