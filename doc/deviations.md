# Deviations from the Revised Report

How `algc` differs from the Revised Report on the Algorithmic Language Algol 60 (1963):
where each difference is, and the task of [todo3.md](../todo3.md) that removes it. A task
deletes its lines when it is done; what remains at the end of Milestone 3 is the list of
documented deviations.

## Representation

The reference language is printed with underlined keywords and symbols that a keyboard does
not have. `algc` reads a hardware representation instead (CLAUDE.md, doc/algol60.asdl):

- Keywords are `_word` in any case; strings are `''…''` or `'('…')'` and do not nest; the
  ten is `⏨`; `≤ ≥ ≠ ¬ ∧ ∨ ⊃ ≡ × ÷ ↑` have ASCII and word forms.
- A file with an underlined letter (U+0332) is in the publication form: keywords are
  underlined words (`b̲e̲g̲i̲n̲`, `g̲o̲ t̲o̲`), strings `` `…' `` or `‘…’`, nested, the inner
  quotes kept. Both forms read the same symbols otherwise.
- The program may end with `_eop`, as on the original.
- Standard identifiers (`sqrt`, `print`, …) are recognized in any case; a declaration of the
  same name hides them, as in the Report.

## Language

| Deviation | Where | Task |
|---|---|---|

## Run time

The Report leaves input and output to implementations; the IFIP modified report defines a
set of standard procedures.

| Deviation | Where | Task |
|---|---|---|
| Integer overflow wraps around; division by zero and real overflow are not reported; string and switch indices are checked only in part; calls through formal procedures are not checked against the procedure | code generator, runtime | F13 |
| Array subscripts are checked only with `--check` | code generator | F13 |

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
- An array called by value keeps the type of the actual's elements.
- An unsigned integer as an actual parameter is a label when the formal is specified
  `label`; otherwise (through a formal procedure, to a `_code` procedure) it is a number.
- Extensions of Algol-GDR (`_string` and `_bits` variables and arrays, substrings `s[i:j]`,
  `_code`, `_algol` and `_common`, `PRINT`, `READ`, `_print` …) are not part of the
  Report; programs that do not use them are not affected.
