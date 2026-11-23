# Deviations from the Revised Report

How `algc` differs from the Revised Report on the Algorithmic Language Algol 60 (1963),
where, and why. Milestone 3 removed the others; the programs of `tests/a60/` check what
it added. The last section is how the X1 library differs from `x1sim` (Milestone 4,
`tests/x1/`).

## Representation

The reference language is printed with underlined keywords and symbols that a keyboard does
not have. `algc` reads a hardware representation instead (CLAUDE.md, doc/algol60.asdl):

- Keywords are `_word` in any case; strings are `` `…' `` or `‘…’`, nested, the inner
  quotes kept, an apostrophe `` '` `` in `` `…' `` (the Algol-GDR `''…''` and `'('…')'`
  are errors); the
  ten is `⏨`; `≤ ≥ ≠ ¬ ∧ ∨ ⊃ ≡ × ÷ ↑` have ASCII and word forms.
- A file with an underlined letter (U+0332) is in the publication form: keywords are
  underlined words (`b̲e̲g̲i̲n̲`, `g̲o̲ t̲o̲`). Both forms read the same symbols otherwise.
- Standard identifiers (`sqrt`, `print`, …) are recognized in any case; a declaration of the
  same name hides them, as in the Report. `print` is the X1 procedure in the publication
  form and Algol-GDR's `PRINT` in the `_keyword` form; `read` in an expression is X1's.

## Language

| Deviation | Where | Reason |
|---|---|---|
| A `goto` to a switch designator whose index is out of range stops the program; the Report (4.3.5) makes it a dummy statement | code generator | An error shows the bug where a silent dummy statement would hide it |
| A formal parameter without a specification takes its kind and type from its uses in the body (below), not from each actual | checker | The code needs one kind per formal; real with conversion serves integer actuals too |
| An array called by value keeps the element type of the actual, not the formal's | code generator | Accesses convert through the tag, so values are right; only a copy of another type would differ |
| The standard identifiers of Algol-GDR (`PRINT`, `READ`, `NEWLINE`, `LOAD`, …), of IFIP and of the X1 library (`PRINTTEXT`, `FLOT`, `SUM`, …) are predeclared | checker | Programs of those systems use them; a declaration hides them |
| Recursion is limited by the machine stack | — | Frames are on the stack (man or boy at k = 12 needs about 1 MB) |

## Run time

The Report leaves input and output to implementations. `algc` has the procedures of the
IFIP Modified Report and those of Algol-GDR (doc/codegen.md); channels are 0 (input) and 1
(output), units are files.

| Deviation | Where | Reason |
|---|---|---|
| Without `--check`, integer overflow wraps around and real overflow gives an infinity | code generator | Speed; `--check` stops the program instead |

## Implementation choices

Where the Report leaves a choice to the implementation, `algc` does as follows; these are
not deviations.

- `integer` is 64-bit two's complement, `real` is IEEE double.
- A real assigned to an integer rounds: ⌊x + ½⌋.
- Operands are evaluated left to right; the subscripts of the left parts of an assignment
  before the right part.
- After `for` ends by exhaustion the controlled variable keeps its last value.
- A formal parameter without a specification is a procedure when it is called, an array
  when subscripted, a switch when subscripted in a `goto`, a label when a `goto` goes to
  it, otherwise a simple variable: Boolean when used as a condition or an operand of a
  logical operator, integer when an operand of `÷`, real otherwise (an integer actual
  converted on each access). One that is only passed on takes any actual.
- An `own` array is allocated at the first entry of its block, with the bounds evaluated
  then; later entries keep it, whatever the bounds would be.
- A file of procedures alone is a unit: `algc -c` makes an object where they are global
  under their names, for `_algol:` declarations of other programs (an extension).
- `integer ↑ integer` is integer; a negative exponent is a run-time error.
- Extensions, after X1: `_then _if … _else` (the `_else` goes with the inner `_if`, with a
  warning); `÷` on real operands, rounded to integers first. A function that does not
  assign its result gets a warning, not an error.
- The IFIP procedures (`inreal`, `ininteger`, `insymbol`, `outreal`, `outinteger`,
  `outstring`, `outsymbol`, `outterminator`, `length`, `stop`, `fault`, `maxreal`,
  `minreal`, `maxint`, `epsilon`): channel 0 is standard input, 1 standard output. A
  number is written as `printf` `%g` with the fewest of 15–17 digits that read back, then
  a blank (the terminator). `insymbol` gives the place of the symbol in the string, or
  minus its code point; `outsymbol` with a negative number writes the code point.
  `maxreal`, `minreal` and `epsilon` are those of IEEE double, `maxint` 2⁶³ − 1.
- Run-time checks: always the switch and string indices, integer division by zero and
  `integer ↑ integer` overflow; with `--check` also array subscripts, integer overflow,
  real division by zero, real overflow, a real too large for an integer, and the number,
  kinds and types of the actual parameters of every procedure. Without `--check` integer
  arithmetic wraps around and reals go to infinity.
- An array called by value keeps the type of the actual's elements.
- An unsigned integer as an actual parameter is a label when the formal is specified
  `label`; otherwise (through a formal procedure, to a `_code` procedure) it is a number.
- Extensions of Algol-GDR (`_string` and `_bits` variables and arrays, substrings `s[i:j]`,
  `_code`, `_algol` and `_common`, `PRINT`, `READ`, `_print` …) are not part of the
  Report; programs that do not use them are not affected.

## The X1 library

The procedures of the Electrologica X1 (`PRINTTEXT`, `PRSYM`, `SPACE`, `TAB`, `NLCR`,
`print`, `FIXT`, `ABSFIXT`, `FLOT`, `read`, `RESYM`, `XEEN`, `SUM`, `EVEN`, `RANDOM`,
`SETRANDOM`, `TIMEOFDAY`) do what they do under `x1sim` of x1-algol-compiler, the reference
of Milestone 4 (doc/codegen.md). Every example of x1-algol-compiler compiles unchanged and
prints what `x1sim` prints; none is skipped. Where it differs:

| Difference | Reason |
|---|---|
| Integers are 64-bit, not the 27 bits of the X1 (no overflow at 2²⁶) | The host's; no example depends on it |
| Reals are IEEE double: 53 bits of mantissa, not 40, so the 13 digits of `print` may differ in the last ones (whetstone, by about 1e-10) | The host's; more exact |
| `FIXT`, `ABSFIXT`, `FLOT` round an exact half (a value exactly between two printed numbers) up; `x1sim` rounds it either way, by the error of its `long double` | The same output on every target |
| `RANDOM` is another generator (SplitMix64) than `x1sim`'s, seeded by `SETRANDOM` with the bits of the real | `x1sim`'s is the C++ library's; only its range and repeatability are promised |
| `stop` ends the program (IFIP); on the X1 it only made the next `XEEN` ask for the switches again | No example calls it; IFIP programs need theirs |
| `TIMEOFDAY` is `$ALGC_TIMEOFDAY` when that is set | Tests of programs that use the time |
| Lines are not broken at 144 positions, pages not at 60 lines (the X8 manual) | As `x1sim` |
