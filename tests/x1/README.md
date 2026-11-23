# The X1 Algol programs

Programs in the publication form for the library of the Electrologica X1:

- the examples of x1-algol-compiler (`examples/`, revision 8e19077), copied unchanged;
- `output`, `input`, `helpers`: the X1 library procedures and their corner cases.

`NAME.out` is the output of `x1sim NAME.a60 < NAME.in` (frozen), made by
`goldens.sh [EXAMPLES [X1SIM]]`, which also copies the examples and takes their inputs from
the `NAME.sh` scripts; `pentominoWxH.in` are inputs of `pentomino.a60`. `quinio.in` (a game
the computer wins) and `bulls_and_cows.in` are written here. `time` (the time of day) and
`bulls_and_cows` (its secret is random) have no `x1sim` output.

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

The numbers of `output` are exact in the 40-bit mantissa of an X1 real, and none is an
exact half at the last printed digit: there `x1sim` rounds either way, by the error of
its `long double`, and algc rounds half up.
