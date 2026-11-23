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

In the C++ `Workspace` holds 01736–05777 and 27245–27644. The streams are vectors; pass 4
keeps its copy of the names at 06000 itself. What the original keeps on the scratch drums is
in `Context`: `saved_names` (drum 26 zone 0), `constants` (drum 27 zone 33, from its end)
and `strings` (drum 27 zones 34…).

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

The C++ passes hold these as typed tables, with the original addresses only in their
comments: the column C++ says where.

| Address | Module | Contents | C++ |
|---|---|---|---|
| 15146–15413 | G14504 | symbol attribute table: for each symbol code, the dispatch indexes per syntactic context (masks 017000000, 03600, 0740000, 030000); then the M7 constants of pass 1 | `SYMBOL_ATTRS` (pass1_parser.cpp); the constants as named words |
| 16045–16071 | G14504 | the seven `'COMMON'` error messages, 3 words each (`HET KOCOЙ ЧEPTЫ`, `ИMЯ COMMON > 6 CИM`, …) | `COMMON_ERRORS` (pass1_lexer.cpp) |
| 16514–16713 | G14504 | character class table, 128 words indexed by the 8-bit character: dispatch offset in the low 4 bits; for single-character symbols the code in bits 24–18 | `CHAR_CLASSES` (pass1_lexer.cpp) |
| 17106–17165 | G14504 | pieces of the syntax-error texts: `STATEMENT`, `DECLARATION`, ` OVERFLOW`, `EXPRESSION`, … | `error::TEXT` (pass1_parser.cpp) |
| 17166–17202 | G14504 | error class table: class of error n = `(word[17166 + (n>>3)] >> 6·(n&7)) & 077` | `error::TEXT` |
| 16506–16513 | G14504 | `WARNING SYMBOL AFTER END`, `HET ′EOP′ !` | literals in pass 1 |
| 21670–21717 | G17223 | `NEXT STATEMENT IS`, `PROGRAM END`, semantic error messages (`IDENTIFIER '…' IS NOT DECLARED`, …), ISO bytes with argument escapes 0200+k | `Format` strings (pass2.cpp) |
| 21720–22025 | G17223 | dispatch table of pass 2, one word per item code 000–0105 (left half: handler, right half: type rules) | `CODES`, `TYPE_OF`, `KIND_OF`, `FORMAL_TYPE_OF`, `FORMAL_CLASS` (pass2.cpp) |
| 22265–22272, 22301 | G22162 | classes of item codes for the first scan of a FOR loop (indexed from 22264); a mask of the allowed codes | `FOR_CLASS` (pass3.cpp) |
| 22365–22402, 22407–22421 | G22162 | rewrite handlers by item code, and their jump table | `PARSE_CLASS`, the handlers in `classify` (pass3.cpp) |
| 23154–23163 | G22162 | index registers for loop subscripts: R5, R4, R3, R2, R8, R9, R10, R11 | `REGS` (pass3.cpp) |
| 25747–26055 | G23523 | instruction templates of the code generator; 4444 marks the operand | `Template`, `TEMPLATE_TEXT`, `PAIRS` (pass4.cpp), also the templates in the code |
| 26056–26151 | G23523 | dispatch table of pass 4, two item codes per word, codes 000–0167 | `HANDLERS` (pass4.cpp) |
| 26601–26616 | G26260 | the line-table format; `PROGRAM TOO LONG` at 26605; the heading `CTPOKИ OTHOCИTEЛЬHЫE AДPECA` at 26612 | literals in pass 5 |
| 30060–30117 | G30034 | ranges for converting ISO to TEXT code | `TEXT_CODES` (pass4.cpp) |
| 30124–30255 | G30120 | run-time names, 0132 words in TEXT code (`A/MINUS`, …, `A/START`, `A/SPRI`, `SIN*`, …), copied to 02400–02531; code addresses `04000+k` refer to entry k | `RUNTIME_NAMES` (pass5.cpp) |
| 30333–30354 | G30257 | the format of `PROGRAM ДЛИHA: …` | `summary_line` (pass6.cpp) |

Message formats used by G27733: ISO bytes, 6 per word; 0200+n inserts data word n (an ISO
string ending with a zero byte); a zero byte ends the format. In C++ they are UTF-8 strings
with the escapes as octal bytes (`"IDENTIFIER ′\200′ IS NOT DECLARED\202"`), made into
words by `iso_words` (`src/card.h`).

The data set before the passes run (01736–05777: the cells of 02000, the name table, the
preset constants, the standard identifiers) is built by `Workspace::reset`
(`src/workspace.cpp`) from the keywords in the order of their words in the original.

## Character codes

The source arrives as 8-bit ISO (KOI-7), converted by the monitor from COSY. See
[encoding.md](encoding.md) for the codes the lexer accepts, and for how `dubna` maps Unicode
onto them.
