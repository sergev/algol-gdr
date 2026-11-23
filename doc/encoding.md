# Source encoding

> History: this describes the original compiler and its C++ rewrite on the `besm6` branch.
> `algc` on `main` reads UTF-8 directly: see the source language in CLAUDE.md.

How source text reaches the original compiler, what it accepts, and what our compiler will
accept in UTF-8.

## The path of a source line

1. **The job file** is UTF-8. `dubna` reads it line by line (`Machine::load_script`).
2. **KOI-7.** `utf8_to_koi7` (`dubna/encoding.cpp`) converts each line:
   - lower-case Latin letters become upper case;
   - Cyrillic letters that look like Latin ones become those (`А`→`A`, `В`→`B`, `Е`→`E`, …),
     and the others get their KOI-7 codes;
   - characters it does not know are dropped.
3. **COSY.** The line is packed in COSY form and stored on drum 1, where the monitor's card
   reader finds it.
4. **Card.** The line is treated as a punched card: only **columns 1–72** belong to the
   program, and columns 73–80 are ignored. Text past column 72 is lost without a warning.
   Check: a comment whose `;` fell in column 73 swallowed the declarations after it.
5. **Listing.** The compiler prints keywords with `′` in place of `'`, and letters in upper
   case: `′BEGIN′`, `PRINT(′′HELLO, WORLD!′′)`. String constants keep only upper case, so the
   output of `print(''Hello, World!'')` is `HELLO, WORLD!`.

## What the original compiler accepts

Keywords are words in quotes: `'BEGIN'`, `'INTEGER'`, `'POWER'`, … The full list, with
internal codes, is in the hash table at 02166 (see [passes.md](passes.md), data set up by the
driver). It is checked on the first 6 characters (`'NOTGREATER'` → `NOTGRE`).

Symbols, tested by compiling `PRINT(…)` under `dubna`:

| Symbol | Original Algol-GDR | Use instead |
| --- | --- | --- |
| `+ - * / < > = ( ) [ ] , ; :` and `:=` | accepted | |
| `≠` | accepted | `'NE'`, `'NOTEQUAL'` |
| `⏨` | accepted: exponent, `1.5⏨-3` | (`@` becomes `°`, which is rejected) |
| `×` | `ERROR 01 SYMBOL` | `*` |
| `÷` | read as `≥`: `dubna` gives it code 032, which the lexer reads as `≥` (`7 ÷ 2` printed `TRUE`) | — |
| `↑` | `ERROR 01 SYMBOL` | `'POWER'` |
| `≤ ≥` | `ERROR 01 SYMBOL` | `'LE'`, `'GE'` (`'NOTGREATER'`, `'NOTLESS'`) |
| `¬ ∧ ∨ ⊃ ≡` | `ERROR 01 SYMBOL` | `'NOT'`, `'AND'`, `'OR'`, `'IMPL'`, `'EQUIV'` |

Other word operators: `'LT'`, `'LESS'`, `'GT'`, `'GREATER'`, `'EQ'`, `'EQUAL'`.

**Why most symbols fail.** The lexer itself accepts single-character operators at these
KOI-7 codes (pass 1 analysis, character table at 16514):

| Code | 020 | 021 | 022 | 023 | 024 | 030 | 031 | 032 |
|---|---|---|---|---|---|---|---|---|
| Operator | `∨` | `∧` | `⊃` | `¬` | `≡` | `≠` | `≤` | `≥` |

`dubna` gives the Unicode symbols different codes (`∨`→036, `∧`→`&`, `≤`→016, …), so only
`≠` arrives where the lexer expects it. And `‘` (U+2018), which `dubna` maps to 020, does work as `∨`: `A ‘ B` gave TRUE, `B ‘ B` FALSE.
The two tables disagree; our compiler simply accepts the Unicode symbols.

Strings: `''text''` or `'('text')'`.

Limits found:
- `1.0⏨20` gives `ERROR 11 REAL OVERFLOW`, because the BESM-6 range ends near 10¹⁹.
- `'OWN'` is in the keyword table, but gives `ERROR 34 STATEMENT` wherever it is used.

## Our compiler

`src/card.cpp` turns each source line into a card exactly as `dubna` does (80 KOI-7
characters, blank-padded, six per word, like 02010… after `READ*`; characters `dubna` does
not know are dropped). Strings, comments and the listing are then those of the original:
`''A∧B''` prints `A∧B`, and `@` is the letter 0100 (printed `°`), not an exponent.

The lexer (`Pass1::unicode_operator`) takes the codes `dubna` gives `∧ ∨ ⊃ ¬ ≡ ≤ ≥ × ↑`
(`&`, 036, 034, 037, 035, 016, 017, 006, 026) as those operators, only where the original
gives `ERROR 01 SYMBOL`: programs the original accepts are compiled the same.

Only columns 1–72 count, as in the original. To produce the same object code as the original, the
reader maps every form to the same internal symbol:

| Internal symbol | Accepted spellings |
| --- | --- |
| multiply | `*`, `×` |
| power | `'POWER'`, `↑` |
| ≤ ≥ ≠ | `'LE'`, `'NOTGREATER'`, `≤` · `'GE'`, `'NOTLESS'`, `≥` · `'NE'`, `'NOTEQUAL'`, `≠` |
| < > = | `<`, `'LT'`, `'LESS'` · `>`, `'GT'`, `'GREATER'` · `=`, `'EQ'`, `'EQUAL'` |
| logical | `'NOT'` `¬` · `'AND'` `∧` · `'OR'` `∨` · `'IMPL'` `⊃` · `'EQUIV'` `≡` |
| exponent | `⏨` |
| quote | `'` and `′` (the listing form) |

Settled since:
- `÷` (and `_div`) is integer division, as in Algol 60. No integer division operator of
  the original has been found (the keyword table has none, and `/` gives a real).
- Lower case is kept: the original makes everything upper case, strings included; `algc`
  keeps identifiers and strings as written (the tests where this changes the output have
  a `NAME.native.out`).

## The listing

The compiler prints ISO (KOI-7); the line printer gets GOST, and `dubna` writes UTF-8. The
mapping, measured by compiling a comment with every code, is in `src/listing.cpp`. Most codes
print as themselves (upper-case Latin, digits, punctuation); some differ: `'` → `′`,
`#` → `≠`, `$` → `◇`, `&` → `∧`, `?` → `↑`, `@` → `°`, `\` → `‾`, `^` → `|`; codes 0140–0176
are Cyrillic (with Latin look-alikes: `а` prints as `A`); 020–037 are `‘ ― ↑ ⏨ ≠ ° ÷ ’ ⊃ ≡ ∨
¬`. Codes 001, 002, 015, 021–024 and 0177 print as nothing, and 003 ends the line.
