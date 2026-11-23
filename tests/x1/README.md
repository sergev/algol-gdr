# The X1 Algol programs

Programs in the publication form for the library of the Electrologica X1:

- the examples of x1-algol-compiler (`examples/`, revision 8e19077), copied unchanged;
- `output`, `input`, `helpers`: the X1 library procedures and their corner cases.

`NAME.out` is the output of `x1sim NAME.a60 < NAME.in` (frozen), made by
`goldens.sh [EXAMPLES [X1SIM]]`, which also copies the examples and takes their inputs from
the `NAME.sh` scripts; `pentominoWxH.in` are inputs of `pentomino.a60`. `quinio.in` (a game
the computer wins) and `bulls_and_cows.in` are written here. `time` (the time of day) and
`bulls_and_cows` (its secret is random) have no `x1sim` output: they run with the time
fixed by `NAME.env` (`ALGC_TIMEOFDAY`), and `NAME.native.out` is algc's output, checked by
hand (`bulls_and_cows.in` wins against that secret).

`run_test` compares the output line by line: the same, or with the same blanks and the
numbers equal to 1e-9 relative. Only `whetstone` needs that: X1 reals have 40 bits of
mantissa, doubles 53, and the difference grows to about 1e-10 over its loops. No example
depends on the 27-bit integers of the X1.

| Program | What |
|---|---|
| output | `PRINTTEXT`, `PRSYM`, `SPACE`, `TAB`, `NLCR`, `print`, `FIXT`, `ABSFIXT`, `FLOT`: their corner cases |
| input | `read`, `RESYM`, `XEEN` |
| helpers | `SUM` (nested, empty, its bound once, an element as counter), `EVEN`, `RANDOM` and `SETRANDOM` (repeatable, in [0, 1)), `TIMEOFDAY` (in a day) |
| beer, disarium, hanoi, hello, magic_square, mersenne, palindromic_primes, perfect_numbers, steady_squares, sum | small programs |
| encoding | the X1 symbol codes, by `PRSYM` |
| quine | prints its own source (but the underlines) |
| man_or_boy | Knuth's test, k = 10 |
| sieve | primes below a million |
| sqrt_test | `sqrt` exact on integers to a million |
| whetstone | one pass of the Whetstone benchmark |
| outer_planets | orbits of the outer planets (Zonneveld), `read` and `SUM` |
| lisp | a Lisp interpreter (Kruseman Aretz) solving missionaries and cannibals |
| pentomino | the pentomino puzzle, 6×10, 5×12, 4×15, 3×20 |
| quinio | five in a row against the computer (Meertens, 1964) |
| bulls_and_cows, time | interactive, random; the time of day |

Times in ms, on an i9-9900K: `x1sim` against algc at `-O1` and `-O2` (each program
gives the same output at both). None needs more than the default stack; `man_or_boy` is
k = 10.

| Program | x1sim | -O1 | -O2 |
|---|---:|---:|---:|
| man_or_boy | 14 | 2 | 2 |
| lisp | 4805 | 15 | 16 |
| pentomino6x10 | 1265 | 2 | 3 |
| pentomino5x12 | 1598 | 3 | 3 |
| pentomino4x15 | 3718 | 4 | 4 |
| pentomino3x20 | 8949 | 7 | 7 |
| sieve | 8185 | 6 | 6 |
| sqrt_test | 6058 | 10 | 10 |
| whetstone | 234 | 2 | 2 |
| palindromic_primes | 3083 | 5 | 5 |

The numbers of `output` are exact in the 40-bit mantissa of an X1 real, and none is an
exact half at the last printed digit: there `x1sim` rounds either way, by the error of
its `long double`, and algc rounds half up.
