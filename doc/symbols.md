# Symbol names

Suggested names for the entry points and tables of the original compiler, from the analysis in
[passes.md](passes.md) and [tables.md](tables.md). The Madlen sources in `os-dubna/algol` still
use address names (`AG15414`). The names here can go into `os-dubna/algol/algol.sym`; that is
not done yet, because the `.sym` files drive `make asm`, which regenerates the sources there.

Names are at most 8 characters, the Madlen limit. Kinds: C = code, D = data.

## Driver (algol.asm)

| Address | Name | Kind | Role |
|---|---|---|---|
| 01614 | ALGOL | C | driver: banner, then the passes |
| 01644 | PRLINE | C | print a listing line (page header, date) |
| 01713 | PRNL | C | print an empty line, or flush |
| 01716 | FINISH | C | end the compilation, return to the monitor at 0425 |

## Pass 1 (ag13666, ag14504)

| Address | Name | Kind | Role |
|---|---|---|---|
| 13666 | PASS1 | C | init and the program head |
| 13742 | RETSTK | C | return through the stack |
| 14020 | BLOCK | C | block or compound statement |
| 14114 | IFSTMT | C | IF statement |
| 14137 | GOTOST | C | GOTO statement |
| 14141 | ASGNST | C | assignment or procedure statement |
| 14262 | DECLS | C | declarations |
| 14510 | PROCHEAD | C | procedure heading |
| 14655 | FORSTMT | C | FOR statement |
| 14716 | IOELEM | C | I/O list element |
| 14762 | EXPR | C | expression (recursive) |
| 15037 | DESIGN | C | designator: subscripts, parameters |
| 15102 | SETRES | C | set the error-resume point |
| 15104 | STMARK | C | emit a statement marker |
| 15114 | DPUSH | C | push onto the D-list |
| 15120 | EMIT1 | C | emit a stream item |
| 15146 | SYMATTR | D | symbol attribute table |
| 15414 | LEXER | C | next symbol into 02000 |
| 16225 | NEXTCH | C | next character; reads cards |
| 16465 | NOEOP | C | source ended without 'EOP' |
| 16514 | CHCLASS | D | character class table |
| 17015 | SYNERR | C | syntax error report and recovery |
| 17066 | EOPERR | C | `↑EOP↑ BEFORE END OF PROGRAM` |
| 17166 | ERRCLASS | D | error class table |

## Pass 2 (ag17223)

| Address | Name | Kind | Role |
|---|---|---|---|
| 17223 | PASS2 | C | entry; 'COMMON' check |
| 17352 | P2READ | C | read and split an item |
| 17377 | LOOKUP | C | identifier lookup |
| 17416 | UNDECL | C | undeclared identifier |
| 17454 | OPENSC | C | open a scope (at END) |
| 17475 | CLOSESC | C | close a scope (at BEGIN) |
| 17652 | TYPEERR | C | type error |
| 21720 | P2DISP | D | dispatch table by item code |
| 22027 | P2END | C | link array records, finish |

## Pass 3 (ag22162)

| Address | Name | Kind | Role |
|---|---|---|---|
| 22162 | PASS3 | C | entry and main loop |
| 22214 | FOLDC | C | fold a constant |
| 22232 | FORSCAN | C | first scan of a FOR loop |
| 22716 | INTERNC | C | intern a constant |
| 23231 | FOREND | C | emit a rewritten loop |
| 23343 | P3EOP | C | end of the pass |

## Pass 4 (ag23523)

| Address | Name | Kind | Role |
|---|---|---|---|
| 23523 | PASS4 | C | init, prologue |
| 23655 | EMITH | C | emit an instruction half-word |
| 23752 | P4READ | C | read and dispatch an item |
| 24324 | SUBSCR | C | subscript address, check call |
| 25525 | BLKENTRY | C | block entry: storage offsets, externals |
| 25747 | TEMPLATE | D | instruction templates |
| 26056 | P4DISP | D | dispatch table by item code |
| 26152 | P4EOP | C | end of program code |

## Passes 5 and 6 (ag26260, ag26666)

| Address | Name | Kind | Role |
|---|---|---|---|
| 26260 | PASS5 | C | address assignment |
| 26535 | P5EMIT | C | emit a half-word, count the code length |
| 26544 | TOOLONG | C | `PROGRAM TOO LONG` |
| 26552 | LINETAB | C | the line→address table |
| 26666 | PASS6 | C | object assembly |
| 27017 | OBJHDR | C | write the header, fill ENTRY* |

## Helpers

| Address | Name | Kind | Role |
|---|---|---|---|
| 27114 | SWHB | C | stream: write a half, backward |
| 27126 | SWHF | C | stream: write a half, forward |
| 27137 | SWWB | C | stream: write a word, backward |
| 27143 | SRHB | C | stream: read a half, backward |
| 27154 | SRHF | C | stream: read a half, forward |
| 27165 | SRWB | C | stream: read a word, backward |
| 27173 | SPAGE | C | stream: page I/O on drum |
| 27220 | STRPUT | C | put a word into the string pool |
| 27245 | COMTAB | D | 'COMMON' / external-symbol table |
| 27645 | CLOCK1 | C | read the clock |
| 27646 | CLOCK2 | C | read the clock (second cell) |
| 27652 | PRSRC | C | print the current source line |
| 27733 | PRFMT | C | print a formatted line |
| 30001 | ITOA6 | C | integer to 6 decimal characters |
| 30034 | ISOTEXT | C | ISO name to TEXT code |
| 30120 | RTNAMES | C | restore the run-time name table |
| 30257 | SUMMARY | C | `PROGRAM ДЛИHA: …` line |
