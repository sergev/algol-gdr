# Source and text encoding

How `algc` reads the characters of a program, and what text a compiled program reads and
writes. The syntax built from these symbols is [algol60.asdl](algol60.asdl); how the
original BESM-6 compiler read KOI-7 cards is in this file on the `besm6` branch.

## The source file

- The file is UTF-8 (`src/source.cpp`). A bad sequence stops the compiler:
  `FILE:LINE: invalid UTF-8 sequence`.
- Lines end with LF or CR LF and have no length limit. The lexer works on code points, one
  line at a time; `′` (U+2032) is read as `'`.
- Blanks (space, tab, CR, FF) and line ends separate nothing outside strings and keywords
  of the `_word` form: `noun phrase` is the identifier `nounphrase`, `1 000` is 1000.
- Messages give `file:line:column:`; the column counts code points from 1, a combining
  underline among them.

## Two forms

A file with an underlined letter anywhere (U+0332, combining low line) is in the
publication form; otherwise it is in the `_keyword` form. The file name does not matter
(`.alg` and `.a60` are both Algol), and the form changes only how keywords are written:

| | `_keyword` form | Publication form |
|---|---|---|
| Keyword | `_begin`, `_BEGIN`, `_Begin`: `_` and letters and digits up to a blank or other character | underlined letters: `b̲e̲g̲i̲n̲`; `g̲o̲ t̲o̲` is one keyword; a doubled underline is accepted |
| `print` | Algol-GDR's `PRINT` | the X1 procedure |

Both forms take the same keywords, in any case:

`algol and array backspace begin bits boolean code comment common decode div do else encode
end endfile eq equal equiv false for ge goto greater gt if impl integer label le less lt
ne not notequal notgreater notless or own power print procedure punch read real rewind step
string switch then true until value while write`

A keyword in quotes, as the original wrote it (`'BEGIN'`), is an error: `Write a keyword as
_BEGIN, not in quotes`.

## Letters, digits, identifiers

- Letters: Latin (`A`–`Z`, `a`–`z`, and U+00C0–U+024F but `×` and `÷`), Greek and
  Cyrillic (U+0370–U+04FF). Digits: `0`–`9`.
- An identifier is a letter and letters and digits, of any length, kept as written and
  case-sensitive: `Count` and `count` differ.
- Standard identifiers (`sqrt`, `PRINT`, `outreal`, `PRINTTEXT`, …) are recognized in any
  case; a declaration of the same name hides them.

## Operators and delimiters

| Symbol | Also written |
|---|---|
| `×` | `*` |
| `/` | |
| `÷` | `_div` |
| `↑` | `^`, `**`, `_power` |
| `<` | `_lt`, `_less` |
| `≤` | `⩽`, `_le`, `_notgreater` |
| `=` | `_eq`, `_equal` |
| `≠` | `_ne`, `_notequal` |
| `≥` | `⩾`, `_ge`, `_notless` |
| `>` | `_gt`, `_greater` |
| `¬` | `_not` |
| `∧` | `&`, `_and` |
| `∨` | `_or` |
| `⊃` | `_impl` |
| `≡` | `_equiv` |
| `[` `]` | `(/` `/)` |
| `;` | `.,` |
| `+ - ( ) , : :=` | |

Any other character outside a string or comment is an error: `Illegal character 'X'`.

## Numbers

- Integers: digits, up to 2⁶³ − 1. Reals: digits with `.` and a fraction, and an exponent
  after `⏨` (U+23E8): `1.5`, `.5`, `1⏨3`, `⏨-2` (that is 10⁻²). The result is an IEEE
  double; a constant that overflows it is an error.
- A number ending in `.,` is followed by `;`.
- Octal constants: `'777'`, up to 16 octal digits, a value of type `_bits`.

## Strings

- A string is in `` `…' `` or in `‘…’` (U+2018, U+2019). 
  Quotes of the same kind nest, and the inner ones
  belong to the string: `` `Hello, `Algol'' `` is ``Hello, `Algol'``.
- A lone apostrophe in `` `…' `` is `` '` ``.
  Or the whole string in `‘…’`, where `'` is a plain character: `‘it's’`.
- Inside a string every character counts, blanks and line ends too; the text is kept as
  UTF-8, in its case.

## Comments

- `_comment` (`c̲o̲m̲m̲e̲n̲t̲`) after `;` or `_begin` skips up to the next `;` (or `.,`).
- After `_end`, the text up to `;`, `_end` or `_else` is a comment. Letters and digits pass
  quietly; anything else gets one warning per comment.
- The end of the file ends the program.

## The listing

`algc --list` prints the banner `A L G O L - Г Д P` and `(7.01.82)` of the original, then
each source line as read, UTF-8, after its number: `%6d.` and five blanks.

## Text at run time

All text a program reads and writes is UTF-8 (runtime/):

- A string is a NUL-terminated UTF-8 C string; its indices (`s[i]`, `s[i:j]`, `length`)
  count code points from 1.
- `PRINT` (Algol-GDR) writes fields of the original's widths, strings as they are
  (runtime/print.c); `READ` reads words separated by white space (runtime/read.c).
- The IFIP `outsymbol` and `insymbol` work in code points: a symbol's place in the string,
  or minus its code point.
- `_write`, `_print`, `_read`, `_encode`, `_decode` with a Fortran format
  (runtime/format.c): records are lines; an `A` field takes characters. A format's
  `'…'` edit is written with `` '` ``: `` `(I3, '` items'`)' ``, or the format in `‘…’`.
- The X1 library works in X1 symbol codes (x1-algol-compiler/doc/algol-encoding.md): `PRSYM`
  writes a code as its UTF-8 symbol (`⎕` for an unused one), `RESYM` reads a UTF-8 symbol
  and gives its code, 255 at the end of the input; `read` takes `⏨` as the exponent
  (runtime/x1.c).

doc/codegen.md has the details of each.

## Differences from the original

The original read KOI-7 cards: columns 1–72 only, upper case only, names by their first 6
characters, keywords in quotes (`'BEGIN'`), strings in `''…''`, and of the symbols above only
`≠` and `⏨` (the rest by their word forms). `algc` keeps names and strings as written, so the
tests where case changes the output have a `NAME.native.out`.
