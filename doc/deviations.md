# Deviations from the Revised Report

How `algc` differs from the Revised Report on the Algorithmic Language Algol 60 (1963):
where each difference is, and the task of [todo3.md](../todo3.md) that removes it. A task
deletes its lines when it is done; what remains at the end of Milestone 3 is the list of
documented deviations.

## Representation

The reference language is printed with underlined keywords and symbols that a keyboard does
not have. `algc` reads a hardware representation instead (CLAUDE.md, doc/algol60.asdl):

- Keywords are `_word` in any case; strings are `''…''` or `'('…')'` and do not nest; the
  ten is `⏨`; `≤ ≥ ≠ ¬ ∧ ∨ ⊃ ≡ × ↑` have ASCII and word forms. Publication-style input
  (underlined keywords, `‘…’` strings, nesting) is **F9**.
- The program may end with `_eop`, as on the original.
- Standard identifiers (`sqrt`, `print`, …) are recognized in any case; a declaration of the
  same name hides them, as in the Report.

## Language

| Deviation | Where | Task |
|---|---|---|
| No `own` | parser | F5 |
| The controlled variable of `for` must be a simple variable | parser, checker | F6 |
| Arrays of the blocks left by a non-local `goto` (through `longjmp`) are not freed | code generator | F7 |

## Program structure

| Deviation | Where | Task |
|---|---|---|
| A unit of procedures alone (`_procedure p …; _eop`) cannot be linked with another Algol unit | code generator, driver | F8 |

## Run time

The Report leaves input and output to implementations; the IFIP modified report defines a
set of standard procedures.

| Deviation | Where | Task |
|---|---|---|
| No IFIP I/O (`inreal`, `outreal`, `outstring`, …), nor `maxreal`, `minreal`, `maxint`, `epsilon`, `stop`, `fault` | checker, runtime | F11 |
| Algol-GDR I/O: `_read` ignores its format; `_decode`, `_encode`, `_rewind`, `_backspace`, `_endfile` not supported; the unit number is ignored | code generator, runtime | F12 |
| Integer overflow wraps around; division by zero and real overflow are not reported; string and switch indices are checked only in part; calls through formal procedures are not checked against the procedure | code generator, runtime | F13 |
| Array subscripts are checked only with `--check` | code generator | F13 |

## Messages

Messages give the line but not the column: **F10**.

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
- An array called by value keeps the type of the actual's elements.
- An unsigned integer as an actual parameter is a label when the formal is specified
  `label`; otherwise (through a formal procedure, to a `_code` procedure) it is a number.
- Extensions of Algol-GDR (`_string` and `_bits` variables and arrays, substrings `s[i:j]`,
  `_code`, `_algol` and `_common`, `PRINT`, `READ`, `_print` …) are not part of the
  Report; programs that do not use them are not affected.
