# The X1 Algol programs

Programs in the publication form for the library of the Electrologica X1, as
`x1sim` of x1-algol-compiler runs them. `NAME.out` is the output of `x1sim NAME.a60`
(frozen); `run_test` compiles and runs them on x86_64, aarch64 and riscv64.

| Program | What |
|---|---|
| output | `PRINTTEXT`, `PRSYM`, `SPACE`, `TAB`, `NLCR`, `print`, `FIXT`, `ABSFIXT`, `FLOT`: their corner cases |

The numbers of `output` are exact in the 40-bit mantissa of an X1 real, and none is an
exact half at the last printed digit: there `x1sim` rounds either way, by the error of
its `long double`, and algc rounds half up.
