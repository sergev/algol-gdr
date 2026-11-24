# Algol with C and other languages

A program built by `algc` can call functions written in C, and C can call procedures
written in Algol. Any language that can make C-compatible functions works the same way:
C++, Rust, Fortran with `bind(C)`, assembly. This document shows how, with examples that
build and run.

| Direction | Algol side | Other side |
|---|---|---|
| Algol calls C | `_code: name;` | a C function taking pointers |
| Algol calls Algol in another file | `_algol: name;` | a *unit*, compiled with `algc -c` |
| C calls Algol | a unit | a call by the Algol convention |
| C implements an Algol procedure | `_algol: name;` | a C function of the Algol convention |
| Shared variables | `_common /NAME/ …` | a global `NAME` |

The language is described in [algol-manual.md](algol-manual.md) (§8.6, §8.7 for these
declarations), the procedures of the run-time library in
[runtime-library.md](runtime-library.md), its C interface in
[runtime-abi.md](runtime-abi.md), and the generated code in [codegen.md](codegen.md).

## 1. Building

`algc` compiles the Algol files and links everything it is given into one program, with
the run-time library `libalgol.a` and the math library:

```
$ cc -c clib.c                      # C to an object file
$ algc prog.alg clib.o              # compile prog.alg, link with clib.o: ./prog
$ algc -c unit.alg                  # an Algol unit to unit.o
$ algc prog.alg unit.o clib.o -o p  # all together
$ algc prog.alg -L/opt/lib -lfoo    # with a library
```

`algc` accepts `.alg`, `.a60`, `.ll`, `.s`, `.o` and `.a` files; compile other languages
to objects or static libraries first. A program can also be linked by `cc` or `clang`
directly: add `libalgol.a` (`-L` its directory, `-lalgol`) and `-lm`.

The program's `main` comes from the library: it runs the Algol program. A C program with
its own `main` may link Algol units (section 6); then the library's `main` is not used.

### Names

An external procedure has the name it has in Algol, exactly: case kept, no prefix or
suffix. Algol names have no `_`, so C helpers with `_` in their names never clash with
them. Names starting with `algol_` belong to the run-time library. Keep external names in
ASCII: other letters are allowed in Algol, but are hard to write in C.

## 2. Data types

| Algol | C | Notes |
|---|---|---|
| `_integer` | `int64_t` | |
| `_real` | `double` | |
| `_boolean` | `bool` | stored as one byte at the start of an 8-byte cell |
| `_bits` | `int64_t` | |
| `_string` | `char *` | NUL-terminated UTF-8; a null pointer is the empty string |
| array | elements of 8 bytes | row-major: the last subscript varies fastest |

Every variable and array element takes 8 bytes. Strings are never changed in place by
Algol, and never freed: a string a C function returns to Algol may come from `malloc` or
be a constant. Arrays are row-major, as in C; Fortran stores them column-major, so a
Fortran routine sees the transpose.

## 3. Algol calls C: `_code`

```
_code: _real power3, sum;      _comment two functions returning double;
_code: swap;                   _comment a function returning void;
```

`_code:` declares procedures written in C, before the program. A type before the names
gives their result; without one they return nothing. The declaration does not list
parameters: the call passes what it is given.

**Each argument is passed by its address**, as in Fortran:

| Actual parameter | C receives |
|---|---|
| a variable, or a formal parameter by name | a pointer to the variable: C can change it |
| an array element | a pointer to the element |
| an array | a pointer to its first element; the bounds are not passed |
| any other expression | a pointer to a temporary holding the value |
| a string | a pointer to the `char *` cell: `char **` |

The value of an expression keeps its own type: `power3(1.5)` passes a `double *`, but
`power3(2)` an `int64_t *`. Write real constants with a point for a `double` parameter.
Procedures, labels and switches cannot be passed to C.

The result is returned as in C: `int64_t`, `double`, `bool` or `char *`.

### Example

```c
// clib.c: C functions for an Algol program.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// _code: _real power3; — x³.
double power3(const double *x) { return *x * *x * *x; }

// _code: swap; — exchanges two integer variables.
void swap(int64_t *a, int64_t *b) { int64_t t = *a; *a = *b; *b = t; }

// _code: _real sum; — the sum of n elements of an array.
double sum(const double *v, const int64_t *n)
{
    double s = 0;
    for (int64_t i = 0; i < *n; i++)
        s += v[i];
    return s;
}

// _code: _boolean even; — an integer even?
bool even(const int64_t *n) { return *n % 2 == 0; }

// _code: _string shout; — a new string: s in upper case and "!".
char *shout(char *const *s)
{
    size_t n = strlen(*s);
    char *r  = malloc(n + 2);
    for (size_t i = 0; i < n; i++)
        r[i] = (*s)[i] >= 'a' && (*s)[i] <= 'z' ? (*s)[i] - 'a' + 'A' : (*s)[i];
    strcpy(r + n, "!");
    return r;
}

// _code: trace; — m[1:rows, 1:cols] of integers, row-major: the diagonal.
void trace(const int64_t *m, const int64_t *rows, const int64_t *cols)
{
    for (int64_t i = 0; i < *rows && i < *cols; i++)
        printf("%lld ", (long long)m[i * *cols + i]);
    printf("\n");
}
```

```
_code: _real power3, sum;
_code: _boolean even;
_code: _string shout;
_code: swap, trace;
_begin _integer i, j, p, q; _real x;
    _real _array v[1 : 4]; _integer _array m[1 : 3, 1 : 3];
    x := 2; print(power3(x), power3(1.5), NEWLINE);
    p := 1; q := 2; swap(p, q); print(p, q, NEWLINE);
    _for i := 1 _step 1 _until 4 _do v[i] := i / 2;
    print(sum(v, 4), even(p), even(7), NEWLINE);
    print(shout(`hello'), NEWLINE);
    _for i := 1 _step 1 _until 3 _do
        _for j := 1 _step 1 _until 3 _do m[i, j] := 10 × i + j;
    trace(m, 3, 3)
_end
```

```
$ cc -c clib.c
$ algc usec.alg clib.o
$ ./usec
8 3.375
2 1
5 true false
HELLO!
11 22 33
```

Algol's output and C's `printf` go through the same `stdout`, so they come out in order.

### C++, Rust, Fortran

Any function with the C calling convention and the same parameters works. In C++ declare
it `extern "C"`; in Rust `#[no_mangle] pub extern "C"`:

```cpp
// cube.cpp
#include <cmath>
#include <cstdint>
extern "C" double cuberoot(const double *x) { return std::cbrt(*x); }
```

```rust
// rmax.rs: the largest of n reals.
#[no_mangle]
pub extern "C" fn rmax(v: *const f64, n: *const i64) -> f64 {
    let s = unsafe { std::slice::from_raw_parts(v, *n as usize) };
    s.iter().cloned().fold(f64::MIN, f64::max)
}
```

```
_code: _real cuberoot, rmax;
_begin _real _array v[1 : 3];
    v[1] := 2; v[2] := 7.5; v[3] := -1;
    print(cuberoot(27.0), rmax(v, 3), NEWLINE)
_end
```

```
$ g++ -c cube.cpp
$ rustc --crate-type=staticlib -O -o librmax.a rmax.rs
$ algc langs.alg cube.o librmax.a
$ ./langs
3 7.5
```

Fortran passes arguments by reference already. A Fortran function becomes a `_code`
procedure with `bind(C, name="...")`, so that its name has no trailing underscore; its
arguments are `integer(c_int64_t)` and `real(c_double)` without `value`. Remember that
Fortran indexes arrays column-major.

## 4. Algol calls Algol: `_algol` and units

A file of procedure declarations alone, separated by `;`, is a **unit**. `algc -c` makes
an object of it, in which each procedure is a global function. Another program declares
the procedures it uses with `_algol:` and is linked with the object. Parameters work as
within one file: by value, by name, arrays, procedures, labels.

```
_comment mathunit.alg: a unit;
_real _procedure hyp(a, b); _value a, b; _real a, b;
    hyp := sqrt(a × a + b × b);

_procedure incr(n); _integer n;
    n := n + 1;

_real _procedure total(v, n); _value n; _real _array v; _integer n;
_begin _integer i; _real s; s := 0;
    _for i := 1 _step 1 _until n _do s := s + v[i];
    total := s
_end;

_real _procedure sigma(i, lo, hi, term); _value lo, hi; _integer i, lo, hi; _real term;
_begin _real s; s := 0;
    _for i := lo _step 1 _until hi _do s := s + term;
    sigma := s
_end
```

```
_algol: _real sigma, hyp;
_algol: incr;
_begin _integer k, n; _real _array a[1 : 3];
    a[1] := 1; a[2] := 2; a[3] := 3;
    n := 41; incr(n);
    print(n, hyp(3, 4), sigma(k, 1, 3, a[k] × a[k]), sigma(k, 1, 1000, 1 / k ↑ 2), NEWLINE)
_end
```

```
$ algc -c mathunit.alg
$ algc usesigma.alg mathunit.o
$ ./usesigma
42 5 14 1.643934566682
```

`sigma` is Jensen's device across files: the term `a[k] × a[k]` is evaluated in the
caller's context each time the unit uses it. A unit may also be given to `algc` as a
source file together with the program: `algc usesigma.alg mathunit.alg`.

## 5. The Algol calling convention

Every Algol procedure, in a unit or declared `_algol`, is a C function of this type:

```c
int64_t name(void *sl, void **args);
```

- `sl` is the static link, the frame of the enclosing procedure. For a procedure of a
  unit it is unused: pass `NULL`.
- `args` points to an array of *actual parameters*, three words each. The word before
  the first holds their number. A call without parameters passes `NULL`.
- The result is returned in an `int64_t`: an integer as it is, a real as the bits of the
  `double`, a Boolean as 0 or 1, a string as the pointer. A procedure without a type
  returns 0.

An actual parameter is a *thunk* and a tag:

```c
typedef struct {
    void *(*code)(void *env); // gives the address of the actual's cell
    void *env;                // passed to code
    int64_t tag;              // type + 16 × kind
} Actual;
```

To use a parameter, the procedure calls `code(env)` and reads or writes the cell at the
address it returns. It calls it again for each use: this is call by name. A parameter
called by value is read once, at the entry of the procedure.

| Type (tag & 15) | | Kind (tag >> 4) | The cell |
|---|---|---|---|
| 0 | integer | 0 a value | the value |
| 1 | real | 1 a variable | the variable: assignments go there |
| 2 | Boolean | 2 an array | the array's descriptor |
| 3 | string | 3 a procedure | a pair `{function, static link}` |
| 4 | bits | 4 a label | a label value (only Algol can use it) |
| 5 | none | 5 a switch | a switch (only Algol can use it) |

The type is the type of the cell; for an array, of its elements; for a procedure, of its
result. Where the procedure expects a real and the cell is an integer (or the reverse),
the Algol code converts at each access, by the tag.

An **array descriptor** is `int64_t` words: the pointer to the elements, the number of
elements, the number of dimensions, then the lower and upper bound of each dimension.
For `v[1 : 3]`: `{ data, 3, 1, 1, 3 }`.

With `algc --check`, an Algol procedure checks the number, kinds and types of the
actuals it gets against its formals, also when C calls it.

## 6. C calls Algol

C builds the actual parameters and calls the procedure. The simplest thunk gives back its
`env`, so `env` is the address of the cell itself. These helpers make it short:

```c
// algolcall.h: calling Algol procedures from C.
#include <stdint.h>
#include <string.h>

// An actual parameter: code(env) gives the address of its cell.
typedef struct {
    void *(*code)(void *env);
    void *env;
    int64_t tag; // type + kind
} Actual;

// Types and kinds of the tag.
enum { A_INTEGER, A_REAL, A_BOOLEAN, A_STRING, A_BITS };
enum { A_VALUE = 0 << 4, A_VARIABLE = 1 << 4, A_ARRAY = 2 << 4, A_PROCEDURE = 3 << 4 };

// The code of a cell that is env itself.
static inline void *algol_cell(void *env) { return env; }

#define ACTUAL(ptr, tag) ((Actual){ algol_cell, (void *)(ptr), (tag) })

// The result of a real function.
static inline double algol_real(int64_t r)
{
    double d;
    memcpy(&d, &r, sizeof d);
    return d;
}
```

The array of actuals must have the count before it: a struct of the count and the
actuals does that. This C program calls the procedures of `mathunit.alg` above:

```c
// caller.c
#include <stdio.h>
#include "algolcall.h"

// The procedures of mathunit.alg.
int64_t hyp(void *sl, Actual *args);
int64_t incr(void *sl, Actual *args);
int64_t total(void *sl, Actual *args);
int64_t sigma(void *sl, Actual *args);

static int64_t k;    // the controlled variable of sigma
static double value; // the cell of the term

// The term 1/k², evaluated at each use, as Algol does by name.
static void *term(void *env)
{
    (void)env;
    value = 1.0 / (double)(k * k);
    return &value;
}

int main(void)
{
    // hyp(3, 4): two reals by value.
    double a = 3, b = 4;
    struct { int64_t n; Actual a[2]; } h = { 2, { ACTUAL(&a, A_REAL), ACTUAL(&b, A_REAL) } };
    printf("hyp = %g\n", algol_real(hyp(NULL, h.a)));

    // incr(n): an integer variable by name.
    int64_t n = 41;
    struct { int64_t n; Actual a[1]; } i = { 1, { ACTUAL(&n, A_VARIABLE | A_INTEGER) } };
    incr(NULL, i.a);
    printf("n = %lld\n", (long long)n);

    // total(v, 3): an array v[1:3], by its descriptor.
    double data[3] = { 1.5, 2.5, 3.0 };
    int64_t desc[5] = { (int64_t)(intptr_t)data, 3, 1, 1, 3 }; // data, count, dims, lo, hi
    int64_t three = 3;
    struct { int64_t n; Actual a[2]; } t = { 2, { ACTUAL(desc, A_ARRAY | A_REAL),
                                                  ACTUAL(&three, A_INTEGER) } };
    printf("total = %g\n", algol_real(total(NULL, t.a)));

    // sigma(k, 1, 1000, 1/k²): Jensen's device, the term a C thunk.
    int64_t lo = 1, hi = 1000;
    struct { int64_t n; Actual a[4]; } s = { 4, { ACTUAL(&k, A_VARIABLE | A_INTEGER),
                                                  ACTUAL(&lo, A_INTEGER), ACTUAL(&hi, A_INTEGER),
                                                  { term, NULL, A_VALUE | A_REAL } } };
    printf("sigma = %.10f\n", algol_real(sigma(NULL, s.a)));
    return 0;
}
```

```
$ cc -c caller.c
$ algc caller.o mathunit.alg -o caller
$ ./caller
hyp = 5
n = 42
total = 7
sigma = 1.6439345667
```

The C program has its own `main`, so the library's is not linked. The unit still needs
`libalgol.a` for its run-time routines; `algc` adds it, or add `-lalgol -lm` when linking
with `cc`.

In `sigma`, `i` is a C variable by name: the Algol loop assigns to it through the cell, and
the thunk `term` reads it each time Algol evaluates the term.

## 7. C implements an Algol procedure

A C function of the Algol convention can be declared `_algol` in a program. It gets the
actuals as thunks and can use them by name: assign to a variable of the caller, or
evaluate an expression of the caller again and again. This one integrates by Simpson's
rule, `integrate(x, a, b, fx)`, where `fx` is an expression in `x`:

```c
// simpson.c: an _algol procedure written in C.
// _real _procedure integrate(x, a, b, fx): ∫ fx dx from a to b, x and fx by name.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *(*code)(void *env);
    void *env;
    int64_t tag;
} Actual;

enum { T_REAL = 1 };

// The address of the cell of actual k: evaluates it again at each call.
static void *cell(Actual *args, int k) { return args[k].code(args[k].env); }

// A real actual, converted when the actual is an integer.
static double get(Actual *args, int k)
{
    void *c = cell(args, k);
    if ((args[k].tag & 15) == T_REAL)
        return *(double *)c;
    return (double)*(int64_t *)c;
}

int64_t integrate(void *sl, Actual *args)
{
    (void)sl;
    if (((int64_t *)args)[-1] != 4 || (args[0].tag & 15) != T_REAL || args[0].tag >> 4 != 1) {
        fprintf(stderr, "integrate(x, a, b, fx): x must be a real variable\n");
        exit(1);
    }
    double a = get(args, 1), b = get(args, 2), s = 0;
    int n = 100;
    double h = (b - a) / n;
    for (int i = 0; i <= n; i++) {
        *(double *)cell(args, 0) = a + i * h;          // x := a + i h
        double w = i == 0 || i == n ? 1 : i % 2 ? 4 : 2;
        s += w * get(args, 3);                         // fx, evaluated again
    }
    double r = s * h / 3;
    int64_t result;
    memcpy(&result, &r, sizeof r);
    return result;
}
```

```
_algol: _real integrate;
_begin _real x;
    print(integrate(x, 0, 1, x × x), integrate(x, 0, 3.14159265358979, sin(x)), NEWLINE)
_end
```

```
$ cc -c simpson.c
$ algc integ.alg simpson.o
$ ./integ
0.3333333333333 2.000000010825
```

The function checks what it gets: the number of actuals is the word before `args`, and
the tags say what each actual is. `a` and `b` are integer constants in the first call, so
`get` converts them by their tag. The cell of a value is valid only until the next call
of its thunk.

## 8. Shared variables: `_common`

A `_common` block (an Algol-GDR extension) puts variables and arrays of the program into
one global array of 8-byte cells, under the block's name. C declares the same global:

```c
// shared.c
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// _common /PARAMS/ n, scale, verbose, v(3): four cells of 8 bytes, then v.
extern struct {
    int64_t n;
    double scale;
    bool verbose; // one byte in an 8-byte cell
    char pad[7];
    double v[3];
} PARAMS;

// _code: rescale; — multiplies v by scale.
void rescale(void)
{
    for (int64_t i = 0; i < PARAMS.n; i++)
        PARAMS.v[i] *= PARAMS.scale;
    if (PARAMS.verbose)
        printf("rescaled %lld elements\n", (long long)PARAMS.n);
}
```

```
_code: rescale;
_begin
    _common /PARAMS/ n, scale, verbose, v(3);
    _integer n; _real scale; _boolean verbose; _real _array v[1 : 3];
    n := 3; scale := 10; verbose := _true;
    v[1] := 1; v[2] := 2.5; v[3] := -1;
    rescale;
    print(v, NEWLINE)
_end
```

```
$ cc -c shared.c
$ algc common.alg shared.o
$ ./common
rescaled 3 elements
10 25 -10
```

`_common /NAME/` lists the block's members in order, with the sizes of the arrays in
parentheses; the members are declared after it as usual. The sizes must be constants. The
global has common linkage: several Algol files and C may all declare it.

## 9. Rules and limits

- **Errors end the program.** A run-time error in Algol code, even one called from C,
  prints its message and exits with status 1. C cannot catch it.
- **`goto` does not cross C.** A `goto` out of an Algol procedure to a label of an outer
  one uses `longjmp`. If C frames are between, they are left without cleanup; avoid it,
  and never pass labels to C.
- **Strings** passed to Algol must stay valid as long as Algol uses them: Algol keeps the
  pointer, not a copy. Algol never frees strings, nor changes them in place.
- **Input and output** are the C library's `stdin` and `stdout`: Algol and C output come
  out in order. Algol's input routines may have read ahead a character; mixing them with
  C input on the same stream may lose it.
- **One thread.** The run-time library keeps state (the output line, the live arrays,
  the input) in global variables.
- **Recursion** through C works, within the limits of the machine stack.

## See also

- [algol-manual.md](algol-manual.md) §8.6–8.7: `_code`, `_algol`, `_common` and units in
  the language.
- [runtime-library.md](runtime-library.md): the procedures a program can call.
- [runtime-abi.md](runtime-abi.md): the C functions of `libalgol.a`, if C needs them.
- [codegen.md](codegen.md): how procedures, thunks, frames and arrays are laid out.
- [tables.md](tables.md): the type and kind codes of the tags.
