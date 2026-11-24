This directory holds Algol 60 programs compiled by `algc`: the examples of
x1-algol-compiler (in the publication form,
with the library of the Electrologica X1), and a few programs in algc's own form (`_begin`):
the Revised Report, separate compilation, Algol-GDR input and output.

Compile a program into an executable and run it:

    $ algc hanoi.a60 -o hanoi
    $ ./hanoi

`NAME.in` is the input of a program, `NAME.out` its expected output (the X1 programs: the
output of `x1sim`); the tests of algc run them all (`tests/run_test.cpp`).

X1 programs:

  * [beer.a60](beer.a60)
  * [bulls_and_cows.a60](bulls_and_cows.a60)
  * [disarium.a60](disarium.a60)
  * [hanoi.a60](hanoi.a60)
  * [hello.a60](hello.a60)
  * [lisp.a60](lisp.a60)
  * [magic_square.a60](magic_square.a60)
  * [man_or_boy.a60](man_or_boy.a60)
  * [mersenne.a60](mersenne.a60)
  * [outer_planets.a60](outer_planets.a60)
  * [palindromic_primes.a60](palindromic_primes.a60)
  * [pentomino.a60](pentomino.a60)
  * [perfect_numbers.a60](perfect_numbers.a60)
  * [quinio.a60](quinio.a60)
  * [sieve.a60](sieve.a60)
  * [sqrt_test.a60](sqrt_test.a60)
  * [steady_squares.a60](steady_squares.a60)
  * [sum.a60](sum.a60)
  * [time.a60](time.a60)
  * [whetstone.a60](whetstone.a60)

Algol 60 and Algol-GDR:

  * [revised_report.a60](revised_report.a60)
  * [classics.alg](classics.alg)
  * [parameters.alg](parameters.alg)
  * [units.alg](units.alg), [units.unit.alg](units.unit.alg)
  * [gdr_io.alg](gdr_io.alg)

# beer.a60

"99 Bottles of Beer on the Wall", written for the a60 interpreter by Erik Schoenfelder.

    $ algc beer.a60 -o beer && ./beer
    99 bottles of beer on the wall, 99 bottles of beer
    take one down and pass it around, 98 bottles of beer on the wall.
    98 bottles of beer on the wall, 98 bottles of beer
    take one down and pass it around, 97 bottles of beer on the wall.
    ...
    1 bottle of beer on the wall, 1 bottle of beer
    take one down and pass it around, no more bottles of beer on the wall.

# bulls_and_cows.a60

The game [bulls and cows](https://en.wikipedia.org/wiki/Bulls_and_cows). The secret is
random; the test fixes it by the time of day (`ALGC_TIMEOFDAY`, in
[bulls_and_cows.env](bulls_and_cows.env)).

    $ algc bulls_and_cows.a60 -o bulls_and_cows && ./bulls_and_cows
    Welcome to bulls and cows!

    I choose a number made of 4 digits (from 0 to 9) without repetitions.
    You enter a number of 4 digits, and I say you how many of them
    are in my secret number but in wrong position (cows),
    and how many are in the right position (bulls).

    Guess a number.
    1234
    You scored 0 bulls and 3 cows.
    ...
    Guess a number.
    4329
    You scored 4 bulls and 0 cows.
    Correct. That took you 4 guesses.

# disarium.a60

Disarium numbers: the sum of the digits raised to the powers of their positions is the
number itself, as 135 = 1<sup>1</sup> + 3<sup>2</sup> + 5<sup>3</sup>.

    $ algc disarium.a60 -o disarium && ./disarium
     0 1 2 3 4 5 6 7 8 9 89 135 175 518 598 1306 1676 2427

# hanoi.a60

The [Tower of Hanoi](https://en.wikipedia.org/wiki/Tower_of_Hanoi), three disks.

    $ algc hanoi.a60 -o hanoi && ./hanoi
    move 1 --> 3
    move 1 --> 2
    move 3 --> 2
    move 1 --> 3
    move 2 --> 1
    move 2 --> 3
    move 1 --> 3

# hello.a60

Print a text, with quotes inside.

    $ algc hello.a60 -o hello && ./hello
    Hello `Algol'

# lisp.a60

A Lisp interpreter by F.E.J. Kruseman Aretz, March 28, 1988, from the article
["A comparison between the ALGOL 60 implementations on the Electrologica X1 and the Electrologica X8"](https://ir.cwi.nl/pub/13677).
Its input, a Lisp program, solves the
[missionaries and cannibals problem](https://en.wikipedia.org/wiki/Missionaries_and_cannibals_problem);
see also [On deriving a Lisp program from its specification](https://core.ac.uk/download/pdf/82584195.pdf).

The run is on the page [lisp.md](lisp.md).

# magic_square.a60

A magic square 8×8.

    $ algc magic_square.a60 -o magic_square && ./magic_square
    magic square, n = 8

      1  2 62 61 60 59  7  8
      9 10 54 53 52 51 15 16
     48 47 19 20 21 22 42 41
     40 39 27 28 29 30 34 33
     32 31 35 36 37 38 26 25
     24 23 43 44 45 46 18 17
     49 50 14 13 12 11 55 56
     57 58  6  5  4  3 63 64

    magic constant = 260

# man_or_boy.a60

Donald Knuth's [man or boy test](https://en.wikipedia.org/wiki/Man_or_boy_test), k = 10.

    $ algc man_or_boy.a60 -o man_or_boy && ./man_or_boy
    -67

# mersenne.a60

Mersenne primes: 2<sup>N</sup>-1 that are prime.

    $ algc mersenne.a60 -o mersenne && ./mersenne
    Searching to M(19) for Mersenne primes
    M(3) : 7
    M(5) : 31
    M(7) : 127
    M(13) : 8191
    M(17) : 131071
    M(19) : 524287

# outer_planets.a60

The orbits of the outer planets of the solar system, from the
[PhD thesis](https://ir.cwi.nl/pub/27822/27822.pdf) of Zonneveld; the source is from
[The Dijkstra–Zonneveld ALGOL 60 compiler for the Electrologica X1](https://gtoal.com/languages/algol60/x1algol/Electrologica_X1.pdf)
by F.E.J. Kruseman Aretz. It reads its data with `read` and sums with `SUM`.

The run is on the page [outer_planets.md](outer_planets.md).

# palindromic_primes.a60

Primes that read the same both ways.

    $ algc palindromic_primes.a60 -o palindromic_primes && ./palindromic_primes
    Computing primes up to 100000...
    Palindromic primes: 2 3 5 7 11 101 131 151 181 191 313 353 373 383 727 757 ...

# pentomino.a60

The [pentomino puzzle](https://en.wikipedia.org/wiki/Pentomino#Games), from the article
["A comparison between the ALGOL 60 implementations on the Electrologica X1 and the Electrologica X8"](https://ir.cwi.nl/pub/13677).

The solutions for the boxes 6×10, 5×12, 4×15 and 3×20 are on the page
[pentomino.md](pentomino.md).

# perfect_numbers.a60

Perfect numbers: equal to the sum of their divisors.

    $ algc perfect_numbers.a60 -o perfect_numbers && ./perfect_numbers
    Searching up to 10000 for perfect numbers: 6 28 496 8128
    4 were found.

# quinio.a60

Quinio, a variant of [Gomoku](https://en.wikipedia.org/wiki/Gomoku): five in a row against
the computer, on a 19×19 board. By L. Meertens, September 1964, for the Electrologica X1;
from the [CWI Software Archive](https://github.com/cwi-software-archive/Algol60/tree/main/Quinio),
read from paper tape. It speaks Dutch.

A game is on the page [quinio.md](quinio.md).

# sieve.a60

The sieve of Eratosthenes, the primes below a million. It would not fit in the 32 kilowords of
the X1.

    $ algc sieve.a60 -o sieve && ./sieve
    Computing primes.............
    Number of primes below 1000 000: 78498

# sqrt_test.a60

Checks that `sqrt` is exact on squares of integers, to a million. From the article
["Design and correctness proof of an emulation of the floating-point operations of the Electrologica X8. A case study"](https://pure.tue.nl/ws/portalfiles/portal/3023549/674735.pdf)
by F.E.J. Kruseman Aretz.

    $ algc sqrt_test.a60 -o sqrt_test && ./sqrt_test
      100000 passed
      200000 passed
    ...
     1000000 passed

# steady_squares.a60

Steady squares: the square ends with the number itself.

    $ algc steady_squares.a60 -o steady_squares && ./steady_squares
    Searching up to 10000 for steady squares:
     1 5 6 25 76 376 625

# sum.a60

The X1 library procedure `SUM`:
[Jensen's device](https://en.wikipedia.org/wiki/Jensen%27s_device).

    $ algc sum.a60 -o sum && ./sum
    1100

# time.a60

The X1 library procedure `TIMEOFDAY`: the local time, in milliseconds.

    $ algc time.a60 -o time && ./time
     19 : 30 : 45.500

# whetstone.a60

One pass of the [Whetstone benchmark](https://en.wikipedia.org/wiki/Whetstone_(benchmark)).
X1 reals have 40 bits of mantissa, doubles 53: the last digits may differ from `x1sim`.

    $ algc whetstone.a60 -o whetstone && ./whetstone
    0 0 0 1 -1 -1 -1
    120 140 120 -0.0683421986223 -0.462637656321 -0.7297183878991 -1.123979070167
    140 120 120 -0.05533645260607 -0.447436563164 -0.7109733896477 -1.103098057683
    ...

# revised_report.a60

The procedures of the [Revised Report](https://www.masswerk.at/algol60/report.htm),
section 5.4.2 — Spur, Transpose, Step, Absmax, Innerproduct, euler — and the for statement
of 4.6.2, as the Report prints them, with calls that check their results.

    $ algc revised_report.a60 -o revised_report && ./revised_report
    66 31 13 1 1 0 0 3.3000000000000003 1 1 220 euler ln 2 ok 1 2 3 7 11 14 20

# classics.alg

Ackermann's function, Euclid's algorithm, the eight queens, the towers of Hanoi, the sieve
of Eratosthenes.

    $ algc classics.alg -o classics && ./classics
    9 61 21 92 1023 25

# parameters.alg

Parameters as the Revised Report has them (section 4.7): Jensen's device, name parameters
of other types, formal procedures, formals without specifications, arrays and labels by
value.

    $ algc parameters.alg -o parameters && ./parameters
    2.083333333333 30
    2 3
    ...
    done

# units.alg

Separate compilation: `units.alg` calls the procedures of
[units.unit.alg](units.unit.alg), declared `_algol:`, compiled alone and linked in. See
[doc/interoperability.md](../doc/interoperability.md).

    $ algc -c units.unit.alg
    $ algc units.alg units.unit.o -o units && ./units
    7 21
    55

# gdr_io.alg

The input and output of Algol-GDR: `_write` and `_read` with a format on a unit, `_rewind`,
`_backspace`, `_endfile`, `_encode`, `_decode`. Unit 7 is a file, named by `ALGC_UNIT7`;
unit 54 the terminal.

    $ algc gdr_io.alg -o gdr_io
    $ ALGC_UNIT7=unit7 ./gdr_io < gdr_io.in
    42 3.25 true 1 2 3
    2
      77  abc    |
    77
    123.45
      9 |
    5 17
