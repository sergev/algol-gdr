# The algc Algol manual

`algc` compiles Algol 60 programs into native programs for x86_64, ARM64 and RISC-V
machines. This manual describes the language as `algc` accepts it, and how to use the
compiler.

The language is Algol 60 of the *Revised Report on the Algorithmic Language Algol 60*
(1963). The sections below follow the order of the Report. `algc` adds:

- the extensions of Algol-GDR, the Algol of the BESM-6: string and bits variables,
  formatted input and output, external procedures;
- the procedures of the Electrologica X1 library, so that the programs of
  [x1-algol-compiler](https://github.com/sergev/x1-algol-compiler) run unchanged;
- the input and output procedures of the IFIP report.

The procedures a program can call (`print`, `sqrt`, `outreal`, `FIXT`, …) are described in
[runtime-library.md](runtime-library.md). Where `algc` differs from the Report is listed in
[deviations.md](deviations.md).

Contents:

1. [A first program](#1-a-first-program)
2. [Running algc](#2-running-algc)
3. [Program structure](#3-program-structure)
4. [Basic symbols](#4-basic-symbols)
5. [Values and types](#5-values-and-types)
6. [Expressions](#6-expressions)
7. [Statements](#7-statements)
8. [Declarations](#8-declarations)
9. [Input and output](#9-input-and-output)
10. [Formatted input and output](#10-formatted-input-and-output)
11. [Run time](#11-run-time)
12. [Implementation notes](#12-implementation-notes)
13. [Appendix: reference tables](#13-appendix-reference-tables)
14. [Appendix: example programs](#14-appendix-example-programs)

## 1. A first program

Put this in a file `hello.alg`:

```algol
_begin
    print(`Hello, World!', NEWLINE)
_end
```

Compile it and run it:

```
$ algc hello.alg
$ ./hello
Hello, World!
```

`algc hello.alg` makes the program `hello` in the current directory. `print` writes its
arguments; `NEWLINE` ends the line.

### Two ways to write a program

Algol 60 was printed with keywords in bold or underlined: **begin**, **end**. A keyboard
has no such letters, so `algc` reads two forms of source text:

- **The `_keyword` form**, usually in `.alg` files. A keyword is an underscore and a
  word: `_begin`, `_integer`, `_if`.
- **The publication form**, usually in `.a60` files. A keyword is underlined with the
  Unicode combining low line (U+0332) after each letter: `b̲e̲g̲i̲n̲`, `i̲n̲t̲e̲g̲e̲r̲`. This is
  how the programs of x1-algol-compiler are written.

A file with an underlined letter anywhere is in the publication form; otherwise it is in
the `_keyword` form. The file name does not matter. The same program in the publication
form:

```algol
b̲e̲g̲i̲n̲
    print(`Hello, World!', NEWLINE)
e̲n̲d̲
```

Everything else is the same in both forms. The examples in this manual use the `_keyword`
form.

## 2. Running algc

```
algc [options...] files...
```

### Input files

| Extension | What |
|---|---|
| `.alg`, `.a60` | Algol source |
| `.ll` | LLVM IR |
| `.s` | assembly |
| `.o`, `.a` | object files and libraries, for instance procedures written in C |

Each Algol file is compiled; then all files are linked with the run-time library
`libalgol.a` and the math library into one program. Its name is the first Algol file's
without the extension, or the name given by `-o`. Compile a C file to an object first
(`cc -c f.c`), then give `algc` the `.o`.

### Options

| Option | Meaning |
|---|---|
| `-o FILE` | name of the output file |
| `-c` | compile each Algol file to an object file `NAME.o`; no link |
| `-S` | compile to assembly, `NAME.s` |
| `--emit-llvm` | compile to LLVM IR, `NAME.ll` |
| `--syntax-only` | only check the program: report errors, make nothing |
| `--list` | print a listing of the program on standard output |
| `--dump-ast` | print the checked program (the AST) in YAML on standard output |
| `-O0`, `-O1`, `-O2`, `-O3` | optimization level; `-O1` by default |
| `-g` | debug information |
| `-L DIR`, `-l LIB` | a library directory, a library to link with |
| `--target=TRIPLE` | make a program for another machine, as for clang: `aarch64-linux-gnu`, `riscv64-linux-gnu` |
| `--save-temps` | keep the intermediate `.ll` files |
| `-v` | show the commands run |
| `--check` | add run-time checks (section 11) |
| `-h`, `--help` | list the options |
| `-V`, `--version` | print the version |

Environment variables: `ALGC_CLANG` is the clang to run (default `clang`), `ALGC_RUNTIME`
the directory of `libalgol.a`.

Examples:

```
$ algc prog.alg                        # makes ./prog
$ algc -O2 -o fast prog.alg            # optimized, named fast
$ algc --check prog.alg                # with run-time checks
$ algc --syntax-only prog.alg          # only the messages
$ algc --list prog.alg > prog.lst      # a listing, and the program
$ algc -c lib.alg                      # lib.o, to link with other programs
$ algc main.alg lib.o util.o -o main   # link Algol and C objects
$ algc --target=aarch64-linux-gnu prog.alg   # an ARM64 program
```

`algc` uses clang to make the machine code, so clang must be installed. For another
target, clang needs that target's system root (`/usr/aarch64-linux-gnu`), and `algc` its
run-time library, which the build makes for each system root it finds.

### Messages

Errors and warnings go to standard error, one per line, with the file, the line and the
column:

```
prog.alg:2:8: error: Type error in the value of ':=': string instead of integer
prog.alg:3:3: error: Undeclared identifier 'j'
prog.alg:4:19: warning: A conditional statement after _then: _else goes with the inner _if
```

The compiler reports all the errors it can find in one run. A program with an error gets
no output file, and `algc` exits with status 1. Warnings do not stop the compilation.

### The listing

`--list` prints the program with line numbers, as the Algol-GDR compiler did:

```
A L G O L - Г Д P
    (7.01.82)

     1.     _begin
     2.         print(`Hello, World!', NEWLINE)
     3.     _end
```

## 3. Program structure

A program is a *block* or a *compound statement*:

- a **compound statement** is a list of statements between `_begin` and `_end`,
  separated by `;`;
- a **block** is the same, with declarations first.

```algol
_begin
    _comment A block: declarations, then statements;
    _integer i, n;
    _real sum;
    n := 10; sum := 0;
    _for i := 1 _step 1 _until n _do sum := sum + 1 / i;
    print(`H(10) =', sum, NEWLINE)
_end
```

Output:

```text
H(10) = 2.928968253968
```

Blocks nest. A name declared in a block is known in that block and the blocks inside it,
unless an inner block declares the same name again. Labels are local to their block too.

The program ends at the end of the file. Before the program there may be declarations of
external procedures and `_common` blocks (section 8.6). A file with only procedure
declarations is a *unit*, compiled separately with `-c` (section 8.7).

### Comments

- `_comment` followed by any text up to the next `;` is a comment. It may stand after
  `_begin` or after `;`.
- After `_end`, the text up to the next `;`, `_end` or `_else` is a comment. Words and
  numbers there pass quietly; other characters get a warning, because they are often a
  forgotten `;`.

```
_begin
    _comment Count the days;
    …
_end of the program
```

## 4. Basic symbols

### Letters, digits, blanks

- Letters: `A`–`Z`, `a`–`z`, the accented Latin letters, Greek and Cyrillic. Digits: `0`–`9`.
- Blanks and line ends separate nothing, except in strings and after a keyword of the
  `_keyword` form. So `noun phrase` is the identifier `nounphrase`, and `1 000 000` is the
  number 1000000. A forgotten `;` at the end of a line can join two lines into one
  statement:

  ```
  _begin _integer i
      i := 1
  _end
  ```

  gives `Expected ';', found ':='`: the declaration declares `ii`.

### Identifiers

An identifier is a letter followed by letters and digits, of any length. Upper and lower
case differ: `Count` and `count` are two names. `_` is not part of an identifier.

```
x   Sum   n1   matrix2by2   Länge   скорость   α
```

The **standard identifiers** (`sqrt`, `print`, `NEWLINE`, `PRINTTEXT`, …) need no
declaration. Each is recognized only in its documented case: the functions of the Report
and the IFIP procedures in lower case (`sqrt`, `outreal`); those of Algol-GDR and the X1
in upper case (`NEWLINE`, `PRINTTEXT`); and `print`, `read`. A program may declare any of
these names for itself; then it means the program's. A wrong case gets a hint:

```
prog.alg:3:13: error: Undeclared identifier 'SQRT' (the standard one is 'sqrt')
```

### Keywords

In the `_keyword` form a keyword is `_` and a word, in any case: `_begin`, `_BEGIN` and
`_Begin` are the same. A blank or any character other than a letter or a digit ends it:
`_real_array` is two keywords, and `_integer i` needs its blank (`_integeri` is an unknown
keyword).

In the publication form each letter of the keyword is underlined: `b̲e̲g̲i̲n̲`. `g̲o̲ t̲o̲`, with
the blank, is the keyword `goto`.

The keywords:

| Keywords | Section |
|---|---|
| `begin end comment` | 3 |
| `integer real boolean string bits array own switch procedure value label` | 5, 8 |
| `true false` | 5 |
| `if then else for do step until while goto` | 6, 7 |
| `and or not impl equiv` | 6.4 |
| `lt less le notgreater eq equal ne notequal ge notless gt greater` | 6.3 |
| `div power` | 6.2 |
| `code algol common` | 8.6 |
| `read print write punch encode decode rewind backspace endfile` | 10 |

The original Algol-GDR wrote keywords in apostrophes, `'BEGIN'`. `algc` does not accept
that: `Write a keyword as _BEGIN, not in quotes`.

### Operators and delimiters

Most operators have a mathematical symbol, an ASCII replacement and a word:

| Symbol | Also written | Meaning |
|---|---|---|
| `+` `-` | | add, subtract, sign |
| `×` | `*` | multiply |
| `/` | | divide |
| `÷` | `_div` | integer divide |
| `↑` | `^`, `**`, `_power` | power |
| `<` | `_lt`, `_less` | less |
| `≤` | `⩽`, `_le`, `_notgreater` | less or equal |
| `=` | `_eq`, `_equal` | equal |
| `≠` | `_ne`, `_notequal` | not equal |
| `≥` | `⩾`, `_ge`, `_notless` | greater or equal |
| `>` | `_gt`, `_greater` | greater |
| `¬` | `_not` | not |
| `∧` | `&`, `_and` | and |
| `∨` | `_or` | or |
| `⊃` | `_impl` | implies |
| `≡` | `_equiv` | equivalent |
| `:=` | | assignment |
| `[` `]` | `(/` `/)` | subscripts, array bounds |
| `;` | `.,` | separator |
| `( ) , :` | | |
| `⏨` | | the base ten of a number: `1⏨6` |

Any other character outside a string or comment is an error: `Illegal character '#'`.

## 5. Values and types

| Type | Values | Constants |
|---|---|---|
| `_integer` | whole numbers, 64-bit: −2⁶³ … 2⁶³ − 1 | `0`, `42`, `1 000 000` |
| `_real` | IEEE double: about 16 digits, up to 1.8 × 10³⁰⁸ | `3.14`, `.5`, `1⏨6`, `2.5⏨-3`, `⏨-2` |
| `_boolean` | true or false | `_true`, `_false` |
| `_string` | text, any length (Algol-GDR) | `` `Hello' `` |
| `_bits` | a 64-bit word, written in octal (Algol-GDR) | `'777'` |

### Numbers

An integer is a sequence of digits. A real has a decimal point, an exponent, or both:
`1.5`, `.5`, `1⏨3` (1000), `⏨-2` (0.01), `6.02⏨23`. The exponent sign `⏨` is U+23E8. A
number has no sign; `-5` is the operator `-` applied to `5`. A constant too large for its
type is an error.

### Strings

A string is in a backquote and an apostrophe, `` `…' ``, or in the typographic quotes
`‘…’`. Inside, every character counts, blanks and line ends too. Quotes nest: a string may
contain a quoted string, and the inner quotes are part of the text.

```algol
_begin
    print(`Say `hello' to Algol', NEWLINE);
    print(`it'`s', ‘it's’, NEWLINE)
_end
```

Output:

```text
Say `hello' to Algol
it's it's
```

An apostrophe alone inside `` `…' `` would end the string, so it is written `` '` ``: an
apostrophe and a backquote, as if the string ended and started again. In `‘…’` the
apostrophe `'` is an ordinary character.

The string forms of older systems are errors, so that their text is not taken for a
program: `''text''` and `'('text')'` of Algol-GDR, and `"text"`:

```
prog.alg:1:14: error: Strings in '' are not supported: write `text'
```

Strings are UTF-8. Their length and the positions in them count characters, not bytes.

### Octal constants

`'777'` is a `_bits` constant: up to 16 octal digits in apostrophes.

## 6. Expressions

An expression computes a value: arithmetic (integer or real), Boolean, string, bits, or a
label (a *designational* expression).

### 6.1 Variables and function designators

- A **simple variable** is its identifier: `x`.
- A **subscripted variable** is an element of an array: `a[i]`, `m[i + 1, j]`. A subscript
  is an arithmetic expression; a real one is rounded to an integer.
- A **function designator** calls a function procedure: `sqrt(x)`, `max(a, b)`, or `f` for
  a function without parameters.

### 6.2 Arithmetic expressions

| Operator | Operands | Result |
|---|---|---|
| `+` `-` `×` | integer and integer | integer |
| `+` `-` `×` | real and any number | real |
| `/` | any numbers | real, always: `7 / 2` is `3.5` |
| `÷` | integers | integer, the quotient truncated toward zero: `7 ÷ 2` is `3`, `-7 ÷ 2` is `-3` |
| `↑` | integer ↑ integer | integer; a negative exponent is an error |
| `↑` | real ↑ integer | real; repeated multiplication, a negative exponent divides into 1 |
| `↑` | any ↑ real | real: `exp(y × ln x)`; `x ↑ 0` is 1 |
| `+` `-` (prefix) | a number | the same type |

Priority, from the highest: `↑`, then `×` `/` `÷`, then `+` `-`. Operators of the same
priority go from left to right: `2 ↑ 3 ↑ 2` is `(2 ↑ 3) ↑ 2` = 64, `a - b - c` is
`(a - b) - c`. Parentheses change the order.

`÷` on real operands rounds them to integers first (an extension, after the X1):
`7.5 ÷ 2` is `8 ÷ 2` = 4.

Where a real meets an integer, the integer is converted to real. Where a real is assigned
to an integer variable or passed to an integer value parameter, it is **rounded**: ⌊x + ½⌋,
so 2.5 gives 3 and −2.5 gives −2. `entier(x)` truncates down instead: `entier(-2.5)` is −3.

```algol
_begin _integer i;
    print(7 ÷ 2, -7 ÷ 2, 7 / 2, 2 ↑ 10, 2.0 ↑ 10, 2 ↑ 0.5, NEWLINE);
    i := 2.5; print(i, `');
    i := -2.5; print(i, `');
    print(entier(-2.5), 7.5 ÷ 2, NEWLINE)
_end
```

Output:

```text
3 -3 3.5 1024 1024 1.414213562373
3 -2 -3 4
```

The standard functions of the Report, of one number:

| Function | Value |
|---|---|
| `abs(x)` | absolute value, real |
| `sign(x)` | −1, 0 or 1, integer |
| `sqrt(x)` | square root; an error for x < 0 |
| `sin(x)`, `cos(x)`, `arctan(x)` | in radians |
| `ln(x)` | natural logarithm; an error for x ≤ 0 |
| `exp(x)` | eˣ |
| `entier(x)` | the largest integer ≤ x |

and the constants `maxint` (2⁶³ − 1), `maxreal`, `minreal` and `epsilon` (those of
IEEE double), from the IFIP report.

### 6.3 Relations

`<`, `≤`, `=`, `≠`, `≥`, `>` compare two numbers (an integer with a real too) or two
strings, and give a Boolean. Strings compare character by character, by code point:
`` `abc' < `abd' ``. Bits compare with `=` and `≠` only.

Relations have a lower priority than arithmetic: `a + 1 < b × 2` compares the sums.

### 6.4 Boolean expressions

| Operator | Meaning |
|---|---|
| `¬ a` | not |
| `a ∧ b` | and |
| `a ∨ b` | or |
| `a ⊃ b` | a implies b: false only when a is true and b false |
| `a ≡ b` | a equals b |

Priority, from the highest: arithmetic, relations, `¬`, `∧`, `∨`, `⊃`, `≡`. So
`¬ a = b` is `¬ (a = b)`, and `x > 0 ∧ y > 0 ∨ z` is `((x > 0) ∧ (y > 0)) ∨ z`.

Both operands of `∧` and `∨` are always evaluated.

### 6.5 String expressions

| Expression | Value |
|---|---|
| `a + b` | `a` and `b` joined |
| `s[i]` | character `i` of `s`, from 1, as a string |
| `s[i : j]` | characters `i` to `j`; `s[i : i - 1]` is empty |
| `length(s)` | the number of characters |

A position out of range is an error. A string variable never assigned is empty.

```algol
_begin _string s, t;
    s := `abc'; t := s + `def';
    print(t, length(t), t[2], t[2:4], s < t, NEWLINE)
_end
```

Output:

```text
abcdef 6 b bcd true
```

### 6.6 Bits expressions

`_bits` values (Algol-GDR) are words of 64 bits. `∧`, `∨` and `¬` work on each bit;
`b ↑ n` shifts left by `n` bits, right when `n` is negative; `=` and `≠` compare.

```algol
_begin _bits b;
    b := '17' ∧ '7';
    print(b, '17' ∨ '60', '1' ↑ 3, b = '7', NEWLINE)
_end
```

Output:

```text
7 77 10 true
```

### 6.7 Conditional expressions

`_if` *condition* `_then` *expression* `_else` *expression* chooses a value:

```
max := _if a > b _then a _else b;
y := 1 / (_if x = 0 _then 1 _else x)
```

When one branch is integer and the other real, the value is real. A conditional
expression as an operand must be in parentheses.

### 6.8 Designational expressions

A designational expression gives a label, for `_goto`: a label `L`, a switch element
`S[i]` (section 8.3), or a conditional `_if c _then L1 _else S[2]`.

## 7. Statements

Statements are separated by `;`. Any statement may have labels before it.

### 7.1 Assignment

```
x := 1;
a[i] := b[i] + 1;
i := j := k := 0
```

The value on the right is assigned to each variable on the left. All left parts must have
the same type (`Left parts of different types` otherwise). An integer value is converted
for a real variable, a real one rounded for an integer variable. The subscripts on the
left are evaluated first, from left to right, then the value.

### 7.2 Labels and goto

A label is an identifier, or an unsigned integer, followed by `:`. The labels of a block
are known in the whole block. `_goto L` continues at the statement labeled `L`.

A `_goto` may leave blocks and procedures: the arrays of the blocks it leaves are freed,
and the procedures it leaves return. It cannot enter a block from outside.

```algol
_begin _integer i;
    i := 0;
L:  i := i + 1;
    _if i < 5 _then _goto L;
    _goto 10;
    print(`not reached');
10: print(i, NEWLINE)
_end
```

Output:

```text
5
```

### 7.3 Dummy statement

An empty statement does nothing. It lets a label stand before `_end`:
`… ; _goto done; … ; done: _end`.

### 7.4 Conditional statements

```
_if x > 0 _then y := sqrt(x);
_if x > 0 _then y := sqrt(x) _else y := 0
```

`_then` may not be followed by another `_if` statement in the Report. `algc` accepts it
(as the X1 did) with a warning, and takes the `_else` with the inner `_if`:

```
prog.alg:4:19: warning: A conditional statement after _then: _else goes with the inner _if
```

Write `_then _begin _if … _end _else` to give the `_else` to the outer `_if`.

### 7.5 For statements

A `_for` statement repeats a statement for each value of a list:

```
_for i := 1 _step 1 _until n _do s := s + a[i];
_for x := 0.5, 1, 2, 4 _do print(x);
_for i := 1, i × 2 _while i < 100 _do print(i);
_for i := 1, 2, 10 _step 10 _until 50, 100 _do print(i)
```

The elements of the list:

- `e` — one value;
- `a _step b _until c` — `a`, `a + b`, `a + 2b`, … while not past `c` (for a negative `b`,
  down to `c`). `b` and `c` are evaluated again at each step, as the Report says;
- `e _while b` — the value of `e`, evaluated again before each repetition, while `b` is
  true.

The body may change the variable. After the loop ends by itself, the variable keeps the
value that ended it: 6 after `_for i := 1 _step 1 _until 5`. A `_goto` out of the loop
keeps the value at that moment.

```algol
_begin _integer i;
    _for i := 1, 2, 5 _step 5 _until 20, 100 _do print(i, `');
    NEWLINE;
    _for i := 1, i + i _while i < 50 _do print(i, `');
    NEWLINE;
    _for i := 1 _step 1 _until 5 _do ;
    print(`after the loop:', i, NEWLINE)
_end
```

Output:

```text
1 2 5 10 15 20 100 
1 2 4 8 16 32 
after the loop: 6
```

### 7.6 Procedure statements

A procedure statement calls a procedure: `p`, `p(x, y)`. Section 8.4 describes procedures
and their parameters. A function procedure may also be called as a statement; its value
is not used.

## 8. Declarations

Declarations are at the start of a block. A name is declared once per block.

### 8.1 Type declarations

```
_integer i, j, n;
_real x, y;
_boolean done;
_string name;
_bits mask;
_own _integer count
```

A variable starts as zero (`0`, `0.0`, `_false`, the empty string). An `_own` variable
keeps its value from one execution of its block to the next.

```algol
_begin _integer _procedure counter;
    _begin _own _integer n; n := n + 1; counter := n _end;
    counter; counter;
    print(counter, NEWLINE)
_end
```

Output:

```text
3
```

### 8.2 Arrays

```
_array a[1 : 10];                    _comment real elements;
_integer _array m[1 : n, 0 : n], t[-5 : 5];
_boolean _array flags[0 : 99];
_string _array words[1 : 20];
_own _real _array table[1 : 100]
```

`_array` alone is `_real _array`. Each pair of bounds gives the lowest and the highest
subscript of a dimension; arrays may have any number of dimensions. The bounds are
expressions, evaluated when the block is entered, so an array can take its size from a
variable of an outer block or a parameter. The elements start as zero. An array is freed
when its block ends.

An `_own` array is allocated at the first entry of its block, with the bounds of that
moment, and keeps its elements after.

```algol
_begin
    _procedure table(n); _value n; _integer n;
    _begin _integer _array sq[1 : n]; _integer i;
        _for i := 1 _step 1 _until n _do sq[i] := i × i;
        print(sq, NEWLINE)
    _end;
    table(3); table(5)
_end
```

Output:

```text
1 4 9
1 4 9 16 25
```

Without `--check` a subscript out of its bounds is not detected; with it, the program
stops: `Subscript 4 out of bounds [1:3]`.

### 8.3 Switches

A switch is a list of designational expressions, numbered from 1. `_goto S[i]` goes to
the `i`-th. An index out of range stops the program.

```algol
_begin _integer k;
    _switch S := L1, L2, _if k > 0 _then L1 _else L3;
    k := 2; _goto S[k];
L1: print(`L1', NEWLINE); _goto done;
L2: print(`L2', NEWLINE); k := 0; _goto S[3];
L3: print(`L3', NEWLINE);
done:
_end
```

Output:

```text
L2
L3
```

### 8.4 Procedures

A procedure declaration gives the heading and the body:

```
_procedure name(formal parameters);
    _value the parameters called by value;
    specifications of the parameters;
    body
```

The body is one statement, often a block. A **function procedure** has a type before
`_procedure`, and assigns its value to its own name in the body:

```
_real _procedure max(a, b); _value a, b; _real a, b;
    max := _if a > b _then a _else b;

_integer _procedure fact(n); _value n; _integer n;
    fact := _if n = 0 _then 1 _else n × fact(n - 1)
```

A function that never assigns its value gets a warning. Procedures may call themselves
(recursion) and each other, and may be declared inside other procedures.

#### Specifications

Each formal parameter may be specified:

| Specifier | The parameter is |
|---|---|
| `_integer`, `_real`, `_boolean`, `_string`, `_bits` | a simple value of that type |
| `_array`, `_integer _array`, … | an array (`_array` alone: real) |
| `_procedure`, `_real _procedure`, … | a procedure, or a function of that type |
| `_label` | a label |
| `_switch` | a switch |

A parameter called by value must be specified. A parameter without a specification takes
its kind from its use in the body: called, it is a procedure; subscripted, an array;
used as a condition, a Boolean; and so on (doc/deviations.md has the rules).

#### Parameters by value and by name

A parameter listed after `_value` is **called by value**: the actual parameter is
evaluated once, at the call, and the procedure works on a local copy. An array by value
is copied.

Any other parameter is **called by name**: each use of the formal in the body evaluates
the actual parameter again, in the place of the call. Assigning to the formal assigns to
the actual variable.

```algol
_begin _integer p, q;
    _procedure swap(a, b); _integer a, b;
    _begin _integer t; t := a; a := b; b := t _end;
    p := 1; q := 2; swap(p, q);
    print(p, q, NEWLINE)
_end
```

Output:

```text
2 1
```

Because a name parameter is evaluated at each use, an expression can depend on another
parameter. This is *Jensen's device*:

```algol
_begin _integer i; _real _array a[1 : 5];
    _real _procedure sum(k, lo, hi, term); _value lo, hi;
        _integer k, lo, hi; _real term;
    _begin _real s; s := 0;
        _for k := lo _step 1 _until hi _do s := s + term;
        sum := s
    _end;
    _for i := 1 _step 1 _until 5 _do a[i] := i;
    print(sum(i, 1, 5, a[i] × a[i]), sum(i, 1, 10, 1 / i), NEWLINE)
_end
```

Output:

```text
55 2.928968253968
```

#### Procedures, labels and switches as parameters

A procedure can be passed to another procedure and called there. A label passed as a
parameter lets the procedure jump out to the caller's label.

```algol
_begin _integer _array a[1 : 3];
    _real _procedure sq(y); _value y; _real y; sq := y × y;
    _procedure apply(f, x); _real _procedure f; _real x;
        print(f(x), NEWLINE);
    _procedure check(v, error); _value v; _integer _array v; _label error;
    _begin v[1] := 99; print(v[1], a[1], NEWLINE); _goto error _end;
    a[1] := 1;
    apply(sq, 3);
    check(a, out);
    print(`not reached');
out: print(`out', NEWLINE)
_end
```

Output:

```text
9
99 1
out
```

The actual parameters of a call must match the formals in number, kind and type.
The compiler checks this where it knows the procedure; with `--check` every call is
checked at run time too.

### 8.5 Procedures without parameters and functions as variables

A function without parameters is called by its name alone: `x := random_value`. Each use
calls it again.

### 8.6 External procedures and common blocks (Algol-GDR)

Before the program, a file may declare procedures that are defined elsewhere:

```
_code: _real hypot2;       _comment a procedure written in C;
_code: twice;
_algol: _real half;        _comment a procedure of another Algol unit;
```

**`_code` procedures** are C functions (or of any language with the C calling
convention). Each argument is passed by its address: a variable or an array element as a
pointer to it, an expression as a pointer to a temporary holding its value, an array as a
pointer to its elements (8 bytes each, row-major), a string as a `char *`. The C types are
`int64_t` for integer, `double` for real, `bool` for Boolean and `char *` for string. The
function returns the type declared in Algol, or `void`.

```c
// hyp.c
#include <math.h>
#include <stdint.h>
double hypot2(const double *a, const double *b) { return sqrt(*a * *a + *b * *b); }
void twice(int64_t *n) { *n *= 2; }
```

```
_code: _real hypot2;
_code: twice;
_begin _integer n;
    n := 21; twice(n);
    print(hypot2(3.0, 4.0), n, NEWLINE)
_end
```

```
$ cc -c hyp.c
$ algc ext.alg hyp.o
$ ./ext
5 42
```

An argument is passed in its own type: `hypot2(3, 4)` would pass two integers to a C
function that reads doubles. Write `3.0`, or pass real variables.

**`_algol` procedures** are compiled separately in Algol: section 8.7.

**`_common` blocks** share variables with C (or Fortran) code by name:

```
_common /ABC/ x, y(10), z(2, 3);
_real x; _array y[1 : 10]; _integer _array z[1 : 2, 1 : 3];
```

`_common /NAME/` lists the variables and arrays of a global block `NAME`, with the sizes
of the arrays; they are declared after it, as usual. C sees them as
`int64_t NAME[n]` (for this one, `x`, then the 10 elements of `y`, then the 6 of `z`).

### 8.7 Separate compilation

A file of procedure declarations alone, separated by `;`, is a **unit**. `algc -c`
compiles it to an object file in which each procedure is a global function under its own
name. A program declares the procedures it uses with `_algol:`, and is linked with the
object:

```
_comment lib.alg: a unit;
_real _procedure half(x); _value x; _real x;
    half := x / 2;
```

```
_comment main.alg;
_algol: _real half;
_begin print(half(5), NEWLINE) _end
```

```
$ algc -c lib.alg
$ algc main.alg lib.o
$ ./main
2.5
```

The procedures of a unit may call each other. A unit can also be given to `algc` as a
source file together with the program: `algc main.alg lib.alg`.

## 9. Input and output

The Report leaves input and output to the implementation. `algc` has four sets of
procedures; a program may use any of them, and mix them. All output goes to standard
output, all input comes from standard input, through one stream each.
[runtime-library.md](runtime-library.md) describes each procedure in full.

### 9.1 print and read

`print` writes numbers, strings, Booleans, bits and whole arrays, with one blank between
the values of one call. It does not end the line; `NEWLINE` does, as an argument or as a
statement of its own. `NEWPAGE` writes a form feed.

| Value | Printed as |
|---|---|
| integer | `42` |
| real | 13 significant digits: `0.3333333333333`, `1e+18` |
| Boolean | `true`, `false` |
| bits | octal: `777` |
| string | as it is |
| array | its elements in order, the last subscript fastest |

Two calls are not separated: `print(1); print(2)` writes `12`. To separate them, end a
call with an empty string, which adds the blank before it: `print(1, `'); print(2)` writes
`1 2`. Or end the line: `print(1, NEWLINE)`.

`read` reads input. As a function, `x := read` reads the next number. As a statement,
`read(i, x, s, a)` reads each variable, element or whole array by its type: words
separated by blanks or line ends.

```algol
_begin _integer n; _real x; _string word; _integer _array a[1 : 3];
    read(n, x, word, a);
    print(n, x, word, a, NEWLINE);
    x := read;
    print(x × 2, NEWLINE)
_end
```

Input:

```text
7 1.5⏨2 hello
1 2 3
0.25
```

Output:

```text
7 150 hello 1 2 3
0.5
```

### 9.2 The IFIP procedures

The procedures of the IFIP report, with a channel number first: 0 is standard input, 1
standard output.

```algol
_begin _integer n; _real x;
    ininteger(0, n); inreal(0, x);
    outstring(1, `n = '); outinteger(1, n);
    outstring(1, `x = '); outreal(1, x × 3);
    outsymbol(1, `abc', 3); outsymbol(1, `', -10)
_end
```

Input:

```text
12 0.1
```

Output:

```text
n = 12 x = 0.30000000000000004 c
```

`outinteger` and `outreal` end each number with a blank; `outreal` writes the fewest
digits that read back as the same number. `insymbol(c, s, i)` reads a character and
gives its position in `s`; `outsymbol(c, s, n)` writes character `n` of `s`, or the
character with code point −n for a negative `n` (`-10` is a line end). `stop` ends the
program; `fault(s, x)` stops it with a message.

### 9.3 The X1 library

The procedures of the Electrologica X1: `PRINTTEXT(s)` writes a string; `NLCR` a line
end; `SPACE(n)` blanks; `TAB` a tab; `FIXT(n, m, x)` and `ABSFIXT` a number in fixed
point, with `n` digits before the point and `m` after; `FLOT(n, m, x)` in floating point;
`PRSYM(k)` and `RESYM` a symbol by its X1 code. `SUM(i, a, b, x)`, `EVEN(n)`, `RANDOM`,
`SETRANDOM` and `TIMEOFDAY` are functions.

```algol
b̲e̲g̲i̲n̲ i̲n̲t̲e̲g̲e̲r̲ i; r̲e̲a̲l̲ x;
    f̲o̲r̲ i := 1 s̲t̲e̲p̲ 1 u̲n̲t̲i̲l̲ 3 d̲o̲
    b̲e̲g̲i̲n̲ x := sqrt(i);
        PRINTTEXT(`sqrt'); FIXT(2, 0, i); FIXT(1, 5, x); FLOT(6, 2, x); NLCR
    e̲n̲d̲;
    print(SUM(i, 1, 10, i), EVEN(3)); NLCR
e̲n̲d̲
```

Output:

```text
sqrt+ 1 +1.00000 +.100000⏨+ 1 
sqrt+ 2 +1.41421 +.141421⏨+ 1 
sqrt+ 3 +1.73205 +.173205⏨+ 1 
55 -1
```

## 10. Formatted input and output

Algol-GDR has statements that write and read with a *format*, as Fortran does. They are
convenient for tables and for files of fixed columns.

### 10.1 The statements

| Statement | What |
|---|---|
| `_print (f) items` | write to standard output |
| `_punch (f) items` | the same |
| `_write (u, f) items` | write to unit `u` |
| `_read (u, f) items` | read from unit `u` |
| `_read (u) items` | read from unit `u` without a format: words, as `read(…)` |
| `_encode (n, f, s) items` | write into the string variable `s`, `n` characters |
| `_decode (n, f, s) items` | read from the first `n` characters of the string `s` |
| `_rewind (u)` | back to the start of the file of unit `u` |
| `_backspace (u)` | back to the start of the record before |
| `_endfile (u)` | the file of unit `u` ends here |

The format `f` is a string expression. With one parameter the parentheses may be
dropped: `_print `(I5)' n`.

**Items** are separated by commas. For output an item is an expression; for input a
variable or an array element. A whole array is all its elements, the last subscript
fastest. An item may be a `_for` clause and an item, as in a `_for` statement:

```
_print (`(I4)') _for i := 1 _step 1 _until 3 _do i × i
```

**Units** are numbered. Unit 54 is the terminal: standard input and output. Any other
unit `n` is a file: the file named by the environment variable `ALGC_UNITn` if it is set,
else `fort.n` in the current directory. The file is opened at its first use; a file first
written is emptied.

### 10.2 Records

The text is divided into **records**: lines. Each statement starts a new record and ends
its last one. A `/` in the format ends a record and starts the next. When the items
outrun the format, the format starts again from the beginning, on a new record. When the
format reaches a data edit and no items are left, the statement ends.

### 10.3 Formats

A format is a list of edits, separated by commas, in parentheses:
`` `(I5, F8.2, 2X, A)' ``.

| Edit | Output | Input |
|---|---|---|
| `Iw` | an integer, right-justified in `w` columns | an integer from `w` columns |
| `Fw.d` | a real with `d` digits after the point, in `w` columns | a real from `w` columns; without a point in the field, the last `d` digits are the fraction |
| `Ew.d` | a real as `0.ddddE+xx`, `d` digits, in `w` columns | as `F` |
| `Gw.d` | as `E` | as `F` |
| `Aw` | a string, right-justified in `w` columns, cut to `w` if longer; `A` alone: the string as it is | `w` characters; `A` alone: the rest of the record |
| `Lw` | a Boolean as `T` or `F`, right-justified | `T` or `F` (or `.T.`), the first letter counts |
| `nX` | `n` blanks | skip `n` columns |
| `nHtext` | the `n` characters after `H` | skip them |
| `'text'` | the text | skip it |
| `/` | end the record | go to the next record |

- A **repeat count** before an edit repeats it: `3I4` is `I4, I4, I4`. Before a group in
  parentheses it repeats the group: `2(I3, 1X)`.
- A number that does not fit in its field is written as asterisks.
- On input, blanks in a numeric field are ignored, and an empty field is 0. A width of 0
  (`I0`, `F0.0`) takes the next word of the record, whatever its length.
- There is no carriage control: the first column is printed like the others.
- The format is a string, so an apostrophe in a `'text'` edit is written `` '` ``:
  `` `(I3, '` items'`)' `` is the format `(I3, ' items')`. Or the whole format can be
  in `‘…’`: `‘(I3, ' items')’`.

### 10.4 Examples

```algol
_begin _integer i; _real _array x[1 : 4];
    _for i := 1 _step 1 _until 4 _do x[i] := i / 3;
    _print (`(5HX =  , 4F8.3)') x;
    _print (`(3I3)') 1, 2, 3, 4, 5, 6, 7;
    _print (`(I3, E12.4, G12.4, L3, A6, A)') -5, 12345.678, 0.001, _false, `ab', `cdef';
    _print (`(I2)') 123;
    _print (`(I4)') _for i := 1 _step 1 _until 3 _do i × i;
    _write (54, `(A, I3)') `Sum:', 6
_end
```

Output:

```text
X =     0.333   0.667   1.000   1.333
  1  2  3
  4  5  6
  7
 -5  0.1235E+05  0.1000E-02  F    abcdef
**
   1
   4
   9
Sum:  6
```

Reading fixed columns, and words with width 0:

```algol
_begin _integer i, j; _real x, y; _string s; _boolean b;
    _read (54, `(I3, F5.2, A4, L2)') i, x, s, b;
    print(i, x, s, b, NEWLINE);
    _read (54, `(I0, F0.0, A)') j, y, s;
    print(j, y, s, NEWLINE)
_end
```

Input:

```text
 42  314 abc T
7  2.5 rest of line
```

Output:

```text
42 3.14  abc true
7 2.5  rest of line
```

A file written and read back. Without `ALGC_UNIT8` set, unit 8 is the file `fort.8`:

```algol
_begin _integer i, n; _real x;
    _for i := 1 _step 1 _until 3 _do _write (8, `(I3, F6.1)') i, i / 2;
    _rewind (8);
    _for i := 1 _step 1 _until 3 _do
    _begin _read (8, `(I3, F6.1)') n, x; print(n, x, NEWLINE) _end
_end
```

Output:

```text
1 0.5
2 1
3 1.5
```

`_encode` and `_decode` work on a string as on a one-record file:

```algol
_begin _string s; _integer i; _real x;
    _encode (12, `(I4, A5)', s) 77, `abc';
    print(s, `|', NEWLINE);
    _decode (6, `(F6.2)', ` 12345') x;
    _decode (4, `(I4)', s) i;
    print(x, i, NEWLINE)
_end
```

Output:

```text
  77  abc    |
123.45 77
```

## 11. Run time

### Errors

An error at run time stops the program: the line in progress is ended, the message goes
to standard error after `Error: `, and the exit status is 1.

```
Error: Integer division by zero
```

These are always checked:

| Message | When |
|---|---|
| `Integer division by zero` | `÷` by 0 |
| `Negative exponent N of an integer`, `0 ↑ 0`, `Integer overflow` | `integer ↑ integer` |
| `Square root of a negative number X`, `Logarithm of a number not above 0: X` | `sqrt`, `ln` |
| `Switch index N out of range` | `_goto S[i]` |
| `String index [I:J] out of range 1..N` | `s[i]`, `s[i : j]` |
| `A goto to a label that cannot be reached` | a `_goto` to a label whose procedure has returned |
| `End of the input`, `Not an integer in the input: W`, … | `read`, `_read` |

`algc --check` adds checks that cost time:

| Message | When |
|---|---|
| `Integer overflow` | an integer `+`, `-` or `×` overflows |
| `Division by zero` | a real `/` by 0 |
| `Real overflow` | a real result is infinite |
| `Real X too large for an integer` | a real rounded to an integer does not fit |
| `Subscript N out of bounds [L:H]` | an array subscript |
| `Procedure P called with N parameters instead of M`, `Parameter K of P is …` | the actual parameters do not match the formals |

Without `--check`, integer arithmetic wraps around, real overflow gives an infinity, and
a subscript out of bounds reads or writes outside the array.

### Recursion

Procedure frames are on the machine stack, so the depth of recursion is limited by its
size (usually 8 MB; `ulimit -s` changes it). Knuth's *man or boy* test at k = 12 needs
about 1 MB.

### The end of the program

The program ends after its last statement, or at `stop` (`STOP`). Output is flushed, the
exit status is 0. No line end is added: end the last line with `NEWLINE`.

## 12. Implementation notes

Where the Report leaves a choice to the implementation, `algc` does as follows:

- `_integer` is 64-bit two's complement, `_real` IEEE double.
- A real assigned to an integer is rounded: ⌊x + ½⌋.
- Operands are evaluated from left to right; in an assignment the subscripts of the left
  parts before the value.
- After a `_for` loop ends by itself, the variable keeps the value that ended it.
- An `_own` array is allocated at the first entry of its block.

Where it differs from the Report:

- A `_goto` to a switch element whose index is out of range stops the program; the
  Report makes it a dummy statement.
- Unspecified formal parameters take one kind and type from their use in the body.
- Recursion is limited by the machine stack.

Differences from the systems `algc` follows:

- From Algol-GDR: the source is UTF-8, in upper and lower case, with names of any length
  and lines of any length. Keywords are `_begin`, not `'BEGIN'`. Strings are `` `…' ``,
  not `''…''`. `PRINT` and `READ` are replaced by `print` and `read`; standard names are
  in their documented case. The end of the file ends the program.
- From the X1: integers are 64-bit and reals double; `print` writes numbers with a blank
  between them, not in the X1's fixed fields; `stop` ends the program.

[deviations.md](deviations.md) lists all the differences, with their reasons.

## 13. Appendix: reference tables

### Priority of operators

From the highest:

| Priority | Operators |
|---|---|
| 1 | `↑` |
| 2 | `×` `/` `÷` |
| 3 | `+` `-` |
| 4 | `<` `≤` `=` `≠` `≥` `>` |
| 5 | `¬` |
| 6 | `∧` |
| 7 | `∨` |
| 8 | `⊃` |
| 9 | `≡` |

All binary operators of one priority go from left to right.

### Standard identifiers

| Group | Names |
|---|---|
| Functions of the Report | `abs sign sqrt sin cos arctan ln exp entier` |
| IFIP | `ininteger inreal insymbol outinteger outreal outstring outsymbol outterminator length stop fault maxreal minreal maxint epsilon` |
| Input and output | `print read NEWLINE NEWPAGE` (also `NEWLIN`, `NEWPAG`), `STOP` |
| X1 library | `PRINTTEXT PRSYM SPACE TAB NLCR FIXT ABSFIXT FLOT RESYM XEEN SUM EVEN RANDOM SETRANDOM TIMEOFDAY` |
| Algol-GDR, not supported | `LOAD LOADGO ASSIGN ATOS STOA` |

## 14. Appendix: example programs

### Euclid's algorithm

```algol
_begin
    _integer _procedure gcd(a, b); _value a, b; _integer a, b;
        gcd := _if b = 0 _then a _else gcd(b, a - a ÷ b × b);
    print(gcd(1071, 462), gcd(17, 5), NEWLINE)
_end
```

Output:

```text
21 1
```

### The sieve of Eratosthenes

```algol
_begin _integer n, i, k;
    n := 50;
    _begin _boolean _array composite[2 : n];
        _for i := 2 _step 1 _until n _do
            _if ¬ composite[i] _then
            _begin
                print(i, `');
                _for k := i × i _step i _until n _do composite[k] := _true
            _end
    _end;
    NEWLINE
_end
```

Output:

```text
2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 
```

### Man or boy

Knuth's test of name parameters and recursion, as he wrote it:

```algol
_begin
    _real _procedure A(k, x1, x2, x3, x4, x5); _value k; _integer k;
    _begin _real _procedure B;
        _begin k := k - 1; B := A := A(k, B, x1, x2, x3, x4) _end;
        _if k ≤ 0 _then A := x4 + x5 _else B
    _end;
    print(A(10, 1, -1, -1, 1, 0), NEWLINE)
_end
```

Output:

```text
-67
```

### A table of squares and roots

```algol
_begin _integer i;
    _print (`(4H   N, 8H  SQUARE, 10H      ROOT)');
    _for i := 1 _step 1 _until 5 _do
        _print (`(I4, I8, F10.4)') i, i × i, sqrt(i)
_end
```

Output:

```text
   N  SQUARE      ROOT
   1       1    1.0000
   2       4    1.4142
   3       9    1.7321
   4      16    2.0000
   5      25    2.2361
```

More programs are in the `tests/` directory of the source: `tests/a60/` (Algol 60 of the
Report: Jensen's device, the procedures of the Report, eight queens), `tests/x1/` (the
programs of x1-algol-compiler: a Lisp interpreter, the pentomino puzzle, Whetstone), and
`tests/algol/` (the test programs of Algol-GDR).
