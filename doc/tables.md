# Tables of the original compiler

The data areas of overlay `ALGOL`, and the tables inside the pass modules. Addresses are
octal. The details of how each pass uses them are in [passes.md](passes.md).

## Memory map of the data areas

| Area | Size | Contents |
|---|---|---|
| 01736 (`AD01736`) | 034 | listing state: line and page state of the print routine 01644; +3 = card count (`KAPT`) |
| 02000 (`AD02000`) | 0166 | work cells: +0/+1 stream read and write cursors, +2 string-pool pointer, +3 constant limit, +4 location counter (pass 4), +70/+71 the pass-1 error-resume point, … |
| 02144 … 03777 | | the name/constant table: the keyword hash table (`AD02166`, `AD02736`) filled in place by pass 1; constants grow downward from 03770 |
| 03771 (`AD03771`) | 7 | fixed constants (TRUE, 52, 51, 50, 2, 1, 0) that open every object's constant block |
| 04000 (`AD04000`) | 01754 | string pool 04000–04377 (pass 1); array records (pass 2); long names and the label table at 04400 (pass 5) |
| 04400 … 05753 | | D-list: declarations, a stack growing downward (pass 1) |
| 05754 (`AD05754`) | 024 | descriptors of the standard identifiers |
| 06000 (`AD06000`) | 04000 | stream buffers: pages 06000–07777 and 10000–11777 |
| 27245 (`AD27245`) | 0400 | the `'COMMON'` table (pass 1), then the external-symbol table (pass 4) |

The `SET` directives at the end of `algol.asm` fill `AD02000`, `AD02166`, `AD03771` and
`AD05754` before the passes run. Each pass reuses the pages for its own data (see the data
flow in [passes.md](passes.md)).

## Keyword hash table (02166…)

Each keyword is two words: the keyword in ISO code (its first 6 characters), then
`7760 0000 0000 00cc`, where cc is the internal symbol code.
- Hash: `((c1&037)<<4) | (c2&017)` of the first two characters, slot 02144 + hash, with
  linear probing upward.
- User identifiers go into the same table; a name word without a code word is an identifier.

| Code | Keyword | Code | Keyword | Code | Keyword |
|---|---|---|---|---|---|
| 001 | IMPL | 030 | THEN | 056 | REAL |
| 002 | OR | 031 | ELSE | 057 | STRING |
| 003 | AND | 032 | FOR | 060 | LABEL |
| 004 | NOT | 033 | DO | 061 | VALUE |
| 005 | LT, LESS | 034 | STEP | 062 | READ |
| 006 | LE, NOTGREATER | 035 | WHILE | 063 | PRINT |
| 007 | GE, NOTLESS | 036 | BEGIN | 064 | PUNCH |
| 010 | GT, GREATER | 037 | END | 065 | WRITE |
| 011 | EQ, EQUAL | 040 | ARRAY | 066 | DECODE |
| 012 | NE, NOTEQUAL | 041 | SWITCH | 067 | ENCODE |
| 017 | POWER | 042 | PROCEDURE | 070 | REWIND |
| 026 | GOTO | 043 | EQUIV | 071 | BACKSPACE |
| 027 | IF | 052 | UNTIL | 072 | ENDFILE |
| | | 053 | OWN | 073 | ALGOL |
| | | 054 | BOOLEAN | 074 | CODE |
| | | 055 | INTEGER | 075 | EOP |
| 076 | TRUE | 077 | FALSE | 0101 | BITS |
| 0177 | COMMENT | 0377 | COMMON | | |

Standard names have no code word, so they are identifiers: ABS, ARCTAN, ASSIGN, ATOS, COS,
ENTIER, EXP, LENGTH, LN, LOAD, LOADGO, NEWLINE, NEWPAGE, SIGN, SIN, SQRT, STOA, STOP.

The internal codes are also the codes of the stream items (see "Item format" in
[passes.md](passes.md)), where 013–016 are `+ - * /`, 020 `(` or a call, 021 `[` or a
subscripted variable, 022 `:=`, 045 `)`, 046 `]`, 047 `,`, 050 `:`, 051 `;` or a statement
marker.

## Tables inside the modules

`scripts/gen-tables.py` copies these tables, and the nonzero words of 01736–05777, from the
overlay image on the tape into `src/tables_data.cpp` (`src/tables.h`), with the original
addresses in comments.

| Address | Module | Contents |
|---|---|---|
| 15146–15413 | G14504 | symbol attribute table: for each symbol code, the dispatch indexes per syntactic context (masks 017000000, 03600, 0740000, 030000); then the M7 constants of pass 1 |
| 16514–16713 | G14504 | character class table, 128 words indexed by the 8-bit character: dispatch offset in the low 4 bits; for single-character symbols the code in bits 24–18 |
| 17106–17165 | G14504 | pieces of the syntax-error texts: `STATEMENT`, `DECLARATION`, ` OVERFLOW`, `EXPRESSION`, … |
| 17166–17202 | G14504 | error class table: class of error n = `(word[17166 + (n>>3)] >> 6·(n&7)) & 077` |
| 16506–16513 | G14504 | `WARNING SYMBOL AFTER END`, `HET ′EOP′ !` |
| 21670–21717 | G17223 | `NEXT STATEMENT IS`, `PROGRAM END`, semantic error messages (`IDENTIFIER '…' IS NOT DECLARED`, …), ISO bytes with argument escapes 0200+k |
| 21720–22025 | G17223 | dispatch table of pass 2, one word per item code 000–0105 (left half: handler, right half: type rules) |
| 22265–22272, 22301 | G22162 | classes of item codes for the first scan of a FOR loop (indexed from 22264); a mask of the allowed codes |
| 22365–22402, 22407–22421 | G22162 | rewrite handlers by item code, and their jump table |
| 23154–23163 | G22162 | index registers for loop subscripts: R5, R4, R3, R2, R8, R9, R10, R11 |
| 25747–26055 | G23523 | instruction templates of the code generator; 4444 marks the operand |
| 26056–26151 | G23523 | dispatch table of pass 4, two item codes per word, codes 000–0167 |
| 26601–26616 | G26260 | the line-table format; `PROGRAM TOO LONG` at 26605; the heading `CTPOKИ OTHOCИTEЛЬHЫE AДPECA` at 26612 |
| 30060–30117 | G30034 | ranges for converting ISO to TEXT code |
| 30124–30255 | G30120 | run-time names, 0132 words in TEXT code (`A/MINUS`, …, `A/START`, `A/SPRI`, `SIN*`, …), copied to 02400–02531; code addresses `04000+k` refer to entry k |
| 30333–30354 | G30257 | the format of `PROGRAM ДЛИHA: …` |

Message formats used by G27733: ISO bytes, 6 per word; 0200+n inserts data word n (an ISO
string ending with a zero byte); a zero byte ends the format.

## Character codes

The source arrives as 8-bit ISO (KOI-7), converted by the monitor from COSY. See
[encoding.md](encoding.md) for the codes the lexer accepts, and for how `dubna` maps Unicode
onto them.
