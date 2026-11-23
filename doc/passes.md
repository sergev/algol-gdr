# Passes of the original compiler

The root overlay `ALGOL` (loaded at 01604, entry 01614) is a driver that calls the passes one
after another. Sources: `os-dubna/algol/*.asm` (copy in `tmp/asm-sources/`). Addresses are
octal. The order of modules at run time comes from `scripts/trace-compile`
([traces.md](traces.md)).

## Driver (`algol.asm`, 01614–01735)

```
01614  if not NOLISFL*: print banner "A L G O L - Г Д P", "(7.01.82)"
01625  call AG13666                      pass 1
       call AG17223                      pass 2
       if FLGEX*  ≠ 0: goto finish       errors found: no more passes
       if CHECKFL* = 0: call AG22162     pass 3 (skipped in *CHECK mode)
01632  call AG23523                      pass 4
01633  call AG26260                      pass 5
       call AG26666                      pass 6 (calls AG30257 at its end)
       if FULISFL* = 0: goto finish
       flush the listing (G44504), load overlay "A/PRINTM" (LOADCALL), return to 0425
finish (01716):
       clear the line buffer state, flush the listing (G44504), return to the monitor at 0425
```

The flags are cells of the monitor:

| Flag | Set by | Effect in the driver |
| --- | --- | --- |
| `NOLISFL*` | `*NO LIST` | no banner; passes check it too (G13666, G26260) |
| `FULISFL*` | `*FULL` | after the passes, load `A/PRINTM` to list the object code |
| `CHECKFL*` (0521) | `*CHECK`; also `*call setftn` | skip G22162; the code gets run-time checks (more externals) |
| `FLGEX*` | the compiler: G14504, G17223, G26260 | errors; stop after pass 2. The monitor adds it to `ERROFLG*` and keeps no object |

Checked with `hello` and `test3`:
- `*NO LIST`: the listing has only the line `PROGRAM ДЛИHA: …`, and the object is the same.
- `*CHECK`: the listing is the same, and the object differs. For test3: 144 words of code instead
  of 137, and one more external.

### Listing output

`ASMPRINT` (shared with the assembler, at 12000) prints the listing. The driver wraps it:
- **01644 (`*0030B`)**: print a line. Its arguments are on the stack (address, length). At
  the start of a page it prints the header with the date (`DATAPR*`, `*REGIM//`). The cells
  `AD01736+0…3` hold its state.
- **01713**: print an empty line, or flush.
- `G44504` is a second entry of ASMPRINT. It ends the printout and returns to the address in
  register 13 (0425, the monitor).

### Data set up by the driver

The `SET` directives at the end of `algol.asm` fill tables before the passes run:
- `AD02000`: small tables, `V1/*`.
- `AD02166…`: the keyword hash table, `V2/*`. Each entry is a pair: the word in ISO code (6
  characters), then `7760 0000 0000 00cc`, where `cc` is the internal code of the symbol, for
  example `BEGIN` = 036, `END` = 037, `'COMMENT'` = 0177. Standard function names
  (`SQRT`, `SIN`, …) have no code word.
- `AD03771`: 7 constants, `V4/0`, the same words that open the constants of every object
  (`0020…`, `6400…0064`, `…0063`, `…0062`, `…02`, `…01`, `…00`).
- `AD05754`: 20 words, `V6/0`.

See [tables.md](tables.md) (A6.4) for the meaning of the tables.

## Overview

Notation: an address `*nnnnB` in a module is the module start + nnnn. Line numbers refer to
`tmp/asm-sources/agNNNNN.asm`. **V** = verified (trace with `dubna -d ime`, a memory dump, or
an experiment with a test program); **I** = inferred from the code. The analysis scripts and
the traces are kept outside the repository.

| Pass | Module | Role |
|---|---|---|
| 1 | G13666 + G14504 | reads cards; lexes; checks syntax by recursive descent; builds the name, constant and string tables and the declaration list; writes a postfix stream |
| 2 | G17223 | reads the stream backwards; scopes, identifier binding, type checking; semantic diagnostics; writes a new stream |
| 3 | G22162 | optimizer: folds constants, merges jumps around procedures, turns array accesses in FOR loops into index-register steps; skipped with `*CHECK` |
| 4 | G23523 | code generator: emits instruction half-words with symbolic addresses and pseudo-items |
| 5 | G26260 | assigns addresses: labels, external names, symbol rows; prints the line table; saves the constants |
| 6 | G26666 | assembles the object backwards on drum 26 and hands it to the monitor; then G30257 prints `PROGRAM ДЛИHA: …` |

### Data flow between passes (V, trace of a 500-line program)

| Pass | Reads | Writes |
|---|---|---|
| 1 | cards | stream forward (buffer 06000, drum 26 zones 2..); strings to drum 27 zone 34 |
| 2 | pass-1 stream backward | saves page 02000–03777 to drum 26 zone 0; stream backward (buffer 10000, drum 27 zones 33..30) |
| 3 | drum 27, forward | stream forward to drum 26 zones 10.. |
| 4 | drum 26 zone 0 into 06000; drum 26 zones 10.. | stream forward (buffer 06000, drum 26 zones 2..) |
| 5 | drum 26 zone 0 into 04000; pass-4 stream | constants page to drum 27 zone 33; stream forward (buffer 10000, drum 26 zones 2..) |
| 6 | constants from drum 27 zones 34, 33; pass-5 stream backward | the object, backward (buffer 06000, drum 26 zones N..3) |

A stream that fits in one page needs no drum I/O at all (Hello, World).

**The compiler never punches.** ALGOL has no `*71` (punch extracode). Pass 6 fills the
monitor table `ENTRY*` (0626–0631):
- 0626 = `PROGRAM`;
- 0627 = `2000000000000000`;
- 0630 = 0 (end of list);
- 0631 = `0010 0000 0026 0003`, a `*70` word that reads drum 26 zone 3.

After the return to 0425, the monitor (`STAR*`, `CATLG`) reads the object from drum 26
zone 3, and then punches it or stores it in the library. So the entry pair of the standard
array is written by the monitor. (V)

## G27126 — streams on drum (shared helper)

Real extent: code 27114–27213, constants 27214–27244. The Madlen split puts parts of it in
ag26666.asm (lines 299–322, entries 27114 and 27123) and ag27220.asm (masks). (V)

**Cursors.** `D02000` is the read cursor and `D02001` the write cursor (V):

| Bits | Field |
|---|---|
| 48 | half flag: 0 = left half next, 1 = right half next |
| 1–15 | address in the buffer |
| 16 | carry catch for the zone step |
| 17–21 | drum zone |
| 22–27 | drum number (026 or 027) |
| 31–33 | buffer page: 2 = 04000, 3 = 06000, 4 = 10000 |

**Entries.** Call with `13 VJM`, value in and out in A. r11 and r12 are kept, r14 is
clobbered. (V)

| Entry | Operation |
|---|---|
| 27114 | write a half, backward |
| 27126 | write a half, forward |
| 27137 | write a word, backward |
| 27143 | read a half, backward (needs one priming read whose value is dropped) |
| 27154 | read a half, forward |
| 27165 | read a word, backward |
| 27173 | page I/O (internal), op in r14: 0 read fwd, 1 write fwd, 2 read back, 3 write back |

**Page I/O.** When the low 10 bits of the address wrap, the cursor steps a zone, and a
1024-word page moves with `*70` at 27211. (V)

**G27220** (27220–27230): `M[D02002++] = A`, the string pool. Every 256 words it writes the
sector to drum 27 zone 34. (V)

## Pass 1: G13666, G14504

### Output (V unless noted)

1. **The stream.** 24-bit postfix items in 06000…, written through `AG15120` → G27126,
   pointer `D02001` starting at 06001.
2. **The name/constant table** at 02144–03777. This is the keyword hash table
   (`AD02166`…), filled in place.
3. **The string pool** at 04000–04377, 6 ISO characters per word, ending with a zero byte;
   written through G27220.
4. **The D-list.** Declarations as a stack growing down from 05753 (in M3) toward 04400. The
   standard identifiers are at 05754–05777.

### Item format

| Bits | Field |
|---|---|
| 24–13 | operand: a table slot or a D-list address |
| 12–8 | count n |
| 7–1 | code |

| Item | Meaning |
|---|---|
| `0102` | first item of the program |
| `L<<12 \| n<<7 \| 051` | statement marker: line L, the n-th statement on the line |
| `s<<12 \| 000` | identifier in name slot s (the word at 02144+s) |
| `s<<12 \| 024 / 023 / 0105` | integer / real / octal constant at 02144+s |
| `k<<12 \| 025` | string at word 04000+(k−01634) of the pool |
| 001–017, 043 | operators IMPL OR AND NOT < ≤ ≥ > = ≠ + − × / ↑ ≡ |
| 076 / 077 | TRUE / FALSE |
| `n<<7 \| 022` | `:=` with n left parts |
| `a<<12 \| n<<7 \| 021` | subscripted variable a with n subscripts |
| `p<<12 \| n<<7 \| 020` | call of p with n arguments |
| 030 / 031, 054 / 053 | THEN / ELSE; end of IF…ELSE / end of IF without ELSE (053 I) |
| 026 | GOTO |
| 055, `v\|032`, 034, 035, 056, `…\|033`, `d\|052` | FOR: start, control variable, step-until, while, plain element, DO, end |
| 036 / `d<<12 \| 037` | block begin (only for a block with declarations) / block end, d = its D-list address |
| `d<<12 \| 060` | label definition |
| `d<<12 \| n<<7 \| 040` | array segment of n arrays; bound expressions follow, then `dims<<7 \| 0104` |
| 042, `d\|061`, `d\|062` | procedure, VALUE, end of procedure body |
| 044 | syntax-error marker |

**D-list words** (V):
- The type is in the top bits, the name slot in the low 12 bits.
- INTEGER `2044 4000…`, REAL `2024 4000…`, BOOLEAN `2104 4000…`, LABEL `2010 4000…`.
- A procedure without a type: `2002 0000…`.
- Arrays: REAL ARRAY `2021 0000…`, with the dimensions in the info field.
- A VALUE formal adds 04000 to the top 12 bits.

### Card input and characters (V)

- **Reading.** The monitor's `READ*` reads each card into 02010… in 8-bit ISO (KOI-7). The
  listing prints the card through `AG27652`, which uses G30001 for the line number.
- **Counters.** Line counter 02060; card count `AD01736+3` (`KAPT:`).
- **Columns.** Only 12 words, 72 columns, are scanned. Columns 73–80 (02024/02025) go only
  to the listing.
- **Blanks.** Words of six blanks are skipped, except inside strings.
- **Source end.** A `*END F` card, or no more cards, before `'EOP'` prints `HET ′EOP′ !`.

### Lexer `AG15414` (V)

- **Dispatch.** A 128-entry character table at 16514 gives each character a dispatch class.
  Letters (Latin, Cyrillic, lower case) start identifiers.
- **`'` (quote).** Followed by a letter: a keyword. By `'`: a string `''…''`. By `(`: a
  string `'(…)'`. By a digit: an octal constant.
- **Two-character symbols.** `:=` 022, `**` 017, `(/` = `[`, `/)` = `]`.
- **Single-character operators at KOI-7 codes 020–032:** ∨ ∧ ⊃ ¬ ≡ ≠ ≤ ≥ (see
  [encoding.md](encoding.md)).
- **Names.**
  - Only the first 6 characters count, so `LONGNAME1` and `LONGNAME2` are the same name.
  - Hash `((c1&037)<<4)|(c2&017)`, linear probing upward from 02144.
  - A keyword is a name word followed by `7760 0000 0000 00cc`. `cc` = 0177 is `'COMMENT'`,
    allowed only after `;` or `'BEGIN'` (otherwise ERROR 06). `cc` = 0377 is `'COMMON'`.
  - Standard names (`PRINT`, `SIN`) have no code word: they are plain identifiers.
- **Text after END.** It is skipped up to `;`, END, ELSE or EOP. A non-letter in it prints
  `WARNING SYMBOL AFTER END` once.
- **Numbers.**
  - Integers are unnormalized floats with exponent 2^40. Above 2^40−1: ERROR 10.
  - Reals: `1.0⏨19` gives ERROR 11.
  - Equal constants share one slot. New constants are allocated downward from 03770; the
    presets are 0, 1, 2… at 03771–03777.
- **Strings.** 6 ISO characters per word, ending with a zero byte; each word goes out
  through G27220. Inside a string, `'ooo'` (one to three octal digits) is the character with
  that code, and blanks are kept. G27220 stores at 04000–04377 and writes each full sector to
  drum 27 zone 34; 02050 counts the words (`01634 + n` in bits 24–13).
- **Character classes.** The table at 16514 has several fields per character, each a
  dispatch: bits 4–1 the first character of a symbol (letter, quote, `.`, `:`, `*`, digit,
  `(`, `/`, `"`, a one-character symbol with its code in bits 24–18, invalid, `,`); bits 6–5
  the character after a quote (quote, other, letter, digit); bit 7 letter or digit; bits 10–8
  the text after END; bit 11 not a letter; bits 13–12 in a number (other, `⏨`, `.`, digit);
  bits 15–14 the sign of an exponent; bit 16 octal digit.
- **Number conversion.** Digits accumulate as floating point (`×10 + d`, mode R = 2) up to
  2^40; later digits only change the scale. A real is then multiplied by 1.25^k and 8^k (the
  exponent field) or divided by 10 k times; `src/pass1_lexer.cpp` follows it step by step.
- **`.,` is `;`.** After a number, `.,` ends it and the next `,` reads as `;` (02047).

### Errors `AG17015` (V)

1. **Count.** `FLGEX*` += 1. At 30 errors it prints `30 ERRORS , COMPILATION INTERRUPTED`
   and stops.
2. **Message.** It prints `ERROR nn CLASS`, with the number (from M8) in decimal. The class
   of error n is `(word[17166 + (n>>3)] >> 6·(n&7)) & 077`, an index into the text pieces at
   17106–17165.
3. **Recovery.** It skips to `;` or END, emits item 044, and resumes at the point saved by
   `AG15102`. On `'EOP'` inside the program: `↑EOP↑ BEFORE END OF PROGRAM`.

| n | Class |
|---|---|
| 1, 2, 4, 5, 6 | SYMBOL (bad character, bad or missing quote, unknown keyword, misplaced COMMENT) |
| 3, 7, 12, 13 | ILIST / CLIST / SLIST / DLIST OVERFLOW |
| 8 | STRING |
| 9, 14, 15 | NUMBER |
| 10, 11 | INTEGER / REAL OVERFLOW |
| 19–27 | EXPRESSION |
| 30, 34, 46 | STATEMENT (34: `'OWN'`, or a declaration among statements) |
| 31–33 | IF STATEMENT |
| 35–39 | FOR STATEMENT |
| 40, 47, 54–56 | DECLARATION |
| 48–53 | ARRAY DECLARATION |
| 57–60 | SWITCH DECLARATION |
| 70–84 | PROCEDURE HEAD |
| 90–94 | I/O STATEMENT |
| 95 | PROGRAM HEAD |
| 99 | PROGRAM END |

### The parser in C++

`src/pass1_parser.cpp` keeps the control flow of the original: one function, labels named
after the addresses, an explicit stack for M17 and a dispatch for the return addresses kept
on it. After a syntax error, SYNERR resumes at the address in 02106 with the stack cut to
the depth in 02107; AG15102 sets both and every block saves them.

### Main routines

| Address | Role |
|---|---|
| 13666 | init: start timer (G27645), banner, cursors, emit `0102` |
| 14020 | block / compound statement |
| 14114, 14137, 14141 | IF, GOTO, assignment or call |
| 14262 | declarations (`'COMMON'` through `AD27245`) |
| 14510 | procedure heading |
| 14655 | FOR statement |
| 14762, 15037 | expressions (recursive), designators |
| 15102, 15104, 15114, 15120 | set the resume point, emit a statement marker, push onto the D-list, emit an item |
| 15414 | lexer |
| 17015, 17066 | errors |
| 27645 / 27646 | read the clock for `BPEMЯ` |
| 27652 | print the current source line |

## Pass 2: G17223

It reads the pass-1 stream **backwards** (last item first) and writes its own stream
backwards too, through 27114, so pass 3 reads it forwards. (V)

### Identifier table

- The word at 02144+slot.
- Pass 2 first saves 02000–03777 (the names) to drum 26 zone 0. It then reuses each word: 0
  = undeclared, 1 = undeclared and already reported, otherwise the address of the visible
  descriptor.
- To print a name it swaps the saved page back in. (V)

### Descriptors (D-list words, V for the values, I for the meaning of the bits)

| Bits | Meaning |
|---|---|
| 47 | this is a descriptor |
| 46 | formal parameter |
| 48+47+46 | formal parameter called by name |
| 48 | also set by pass 2 as a "used/assigned" mark |
| 44 / 43 / 42 / 41 / 40 | string / boolean / integer / real / label |
| 39 / 38 / 37 | simple / procedure / array (a switch is 40+37) |
| 36 | simple variable or procedure without parameters |
| 16 up | number of parameters or dimensions |
| 32..21 | the shadowed table entry while the block is open; the array record at the end of the pass |
| 12..1 | name slot |

A block header has bit 48 set, and its low 12 bits give the end address.

### Scopes and checks

- **Scopes.** The stream is reversed, so END (037) opens a scope and BEGIN (036) closes it.
  Opening a scope also checks for TWICE DECLARED.
- **Type checking.**
  - Recursive descent over the reversed postfix, driven by a dispatch table at 21720 with
    one word per input code.
  - The type comes back in M5 (0 none, 1 real, 2 integer, 3 boolean), the kind in M6
    (0 simple, 1 array or switch, 2 procedure with parameters, 3 procedure without).
  - One message per subtree; sibling subtrees each report. An undeclared identifier is
    reported once, at its last use.
- **End of the pass.** It links array descriptors to the array records in `AD04000`, and
  prints `* * INCORRECT ALGOL PROGRAM * *` if `FLGEX*` is set.

### Messages (V)

Pass 2 prints its messages as it processes the stream from end to start, so they come out in
reverse order.
- **Position.** `NEXT STATEMENT IS l/s` is the last statement marker (051) read. Reading
  backwards, that is the start of the statement after the error.
- **Expansion.** The texts are ISO bytes; a byte 0200+k inserts argument k. G27733 expands
  and prints them.

| Address | Text |
|---|---|
| 21676 | `IDENTIFIER '…' IS NOT DECLARED` |
| 21704 | `ILLEGAL USE OF IDENTIFIER '…'` |
| 21712 | `TWICE DECLARED IDENTIFIER '…'` |
| 17655 | `TYPE ERROR IN CONNECTION WITH …` (`'+'`, `'**'`, `':='`, `'IF'`, `'FOR'`, `SWITCH`, `INDEX`, `GOTO`, `BOOLEAN OP.`, `RELATION`) |
| 17563 | `NO ASSIGNMENT TO '…'` |
| *0534B | `ILLEGAL LEFT PART OR INPUT ELEMENT` |
| *1214B, *1257B | `ILLEGAL ARGUMENT OF I/O STATEMENT`, `ILLEGAL INPUT OF '…'` |
| *1520B | `NO ACTUAL-FORMAL CORRESPONDENCE BETWEEN '…' AND '…'` |
| *1731B | `LABEL AS ARGUMENT OF NON-ALGOL CALL` |
| *2007B, *2044B, *2063B | `ILLEGAL VALUE PARAMETER`, `ILLEGAL ARGUMENT OF LENGTH`, `… OF PRINT` |
| 22051 | `* * INCORRECT ALGOL PROGRAM * *` |
| (I) | two KOI-7 messages about inconsistent `'COMMON'` declarations |

Quirk: using an array formal parameter inside a procedure body gave `ILLEGAL USE … NEXT
STATEMENT IS 304/ 2` with a bogus line number (not investigated).

### Output stream (V, partly decoded)

- Identifiers become typed operations carrying the descriptor address.
- int→real conversions (064) are inserted.
- Examples: `X := 2` → `100 X`, `064`, `023 slot`; a variable's value → `017 desc`; a call →
  `110`, `120`, the arguments, `020 desc`.

## G30001 — integer to text (V)

- Call: `VJM 30001`, number in A.
- Result: 6 ISO characters, right-justified with leading spaces, the last 6 decimal digits
  (in C, `"%6u"` of `(n & 06777777) % 1000000`).
- Clobbers M14 and cells 025–027.

## Pass 3: G22162 — optimizer (V unless noted)

It copies the pass-2 stream item by item and makes four rewrites. They were found by diffing
what pass 4 reads with and without `*CHECK` (test3, proc, for, and scratch programs).

1. **Integer unary minus of a constant** (`026` + constant) becomes a new constant marked
   negative (attribute `0200`).
   - Examples: `I:=-3`, `A[-1]`, `STEP -1`. Knowing the step's sign at compile time gives a
     shorter FOR loop.
   - Real minus (047) is not folded.
2. **int→real conversion of a constant** (`064` + constant) becomes a real constant: `X:=2`
   loads a 2.0 directly, while without pass 3 the code is `XTA int; A+X 0`.
3. **Adjacent procedure declarations.** A `146` followed only by line markers and then
   `145` is deleted, so one jump skips several procedure bodies (proc.alg: 19 UJ with
   `*CHECK`, 13 without).
4. **Array accesses linear in the control variable of a FOR loop.**
   - Each `A[c*I+d]` gets an index register R, loaded before the loop with the address of
     `A[start]`.
   - Accesses become `d.R163` (read) or `d.R164` (store), and `(c*step*stride).R165` steps R
     at the end of the body. Example: `A[I]:=I` → `5 VTM base; … 5 ATX 0; 5 UTM 1`.
   - Registers are taken in the order R5, R4, R3, R2, R8, R9, R10, R11 (table at 23154).
   - The loop's descriptor gets flags for pass 4 (040000, plus 010000 or 020000 for step 1
     or not).
   - To rewrite a loop, pass 3 saves page 10000 to drum 26 zone 3, writes the new loop into
     10000, copies it back over the loop, and restores the page.

Codes that only pass 3 creates: `X.0162` (load a register with an element address, followed
by `0000.R000`), `163`, `164`, `165`, and `167 … 166` (preamble of an inner loop).

**Why `*CHECK` skips it (I).** In check mode pass 4 emits a call of `A/SUBCH` after every
subscript computation. An access through a stepped register has no subscript left to check.

**Main routines:**

| Address | Role |
|---|---|
| 22162 | init; R1 = 5 − procedure depth |
| 22167 | main loop: read an item and dispatch (146, 075, 055, 042/062, 026/064, else copy) |
| 22214 | fold a constant |
| 22232 | FOR, first scan: classify each item (3-bit table at 22264); give up on statements it cannot handle |
| 22327, 22352 | set up the rewrite; rewrite scan (handler table 22365, jumps at 22407) |
| 22716 | intern a constant: search 02145–03777 for an equal word, else allocate downward |
| 23231 | end of FOR: emit the preamble, the loop and the increments |
| 23343 | EOP |

**Constants table.** Index i is at 02144+i. It grows downward from index 1624 (03770); 1625–
1633 are the fixed constants (1631 = 2, 1632 = 1, 1633 = 0). No error messages: when it
cannot optimize, it copies the loop unchanged (I).

## Pass 4: G23523 — code generator

**Output (V).** Instruction half-words: register (4 bits), the long-format bit, the opcode,
and the address. The address is one of:
- `4xxx`: the constant or external area, renumbered by pass 5 (`4014` → `74001`);
- `1xxxx`: a label reference;
- anything else: an offset from M6 (the frame) or M7 (the constants).

Example: `3641.0000` → `7 VTM <length>`, `6710.4014` → `13 VJM 74001`,
`7650.0006` → `15 UTM 6`.

Pseudo-items, consumed by pass 5:

| Item | Meaning |
|---|---|
| `0330.n` | source line n starts here |
| `0200.0000` | start a new word |
| `0211.n` | define label n |
| `0301.n`, `0271.n` | UJ / U1A to label n |

**Check mode (V).** `CHECKFL*` is read only in the subscript code (line 650, about 24263):
when set, it emits `13 VJM` to `A/SUBCH` after each subscript computation.

**Input.** The pass-3 stream (the pass-2 stream in `*CHECK` mode), read through the page at
10000.

**Memory.**
- At start it reads drum 26 zone 0 (the names) into 06000. Only four names are used before
  the page is overwritten: `ASSIGN`, `ATOS`, `LENGTH`, `STOA`, which G30034 turns into
  external names. (V)
- `AD27245` (256 words): the external-symbol table.
- `AD02000+4` is the location counter.

**Main routines (I unless noted):**

| Address | Role |
|---|---|
| 23523 | init; read zone 0; outer block; prologue `7 VTM`, `VJM A/START`, `15 UTM frame` |
| 23752 | read an item and dispatch: R1 = code, R2 = attribute, R3 = value; jump table at 26056, two codes per word (V) |
| 23655 | emit one half-word, handling alignment and pseudo-items |
| 23712, 23734 | template interpreter; templates at 25747…; 4444 marks the operand, taken from the operand stack |
| 24324 | address of a subscripted variable, plus the check call |
| 25525 | block entry: assign storage offsets; enter external and 'CODE' names |
| 26152 | EOP: `UJ A/STOP`, `0200.0000`, `0211.0000` |

Most handlers (codes 001–167) are known only by the code they serve. No error messages (I).

## G30034 — ISO name to TEXT (V for Latin)

- Converts 6 eight-bit characters into up to 8 six-bit TEXT characters, left-justified.
  Latin letters map to their code − 040.
- Example: `ASSIGN` → `4163 6351 4756 0000`.
- Called by pass 4 (25653).

## Pass 5: G26260 (V)

`src/pass5.cpp` reproduces the streams, line tables and (with pass 6) the objects of all
tests.

### Start

1. Unless `NOLISFL*`: an empty line (G01713), the heading `CTPOKИ OTHOCИTEЛЬHЫE AДPECA`.
2. Read the page saved by pass 2 (drum 26 zone 0) into 04000.
3. Constants: their number is `01633 − M[02003]`, stored in 02055. Copy them from 03777
   downward to 05777 downward, and write 04000–05777 to drum 27 zone 33.
4. Call G30120 (restore the run-time name table at 02400) and clear the label table
   04400–05776.
5. Write a 0 half before the stream proper (at the end of the previous page). The first
   read of the pass-4 stream is a priming read; its value is dropped.

### Main loop

Halves are counted in M3; 02004 is 1 at a word start; 02054 = M3/2, the code length so far.
Per half `h` of the pass-4 stream, by `h & 03700000`:

| Half | Meaning | Done |
|---|---|---|
| `0330nnnn` | source line n starts | line table cell (only with a listing) |
| `0200nnnn` | align | at a word start dropped, else written as `0220nnnn` (UTC) |
| `0211nnnn` | label n defined | at a word start written as is, taking no space; else written as `0201nnnn`, filling the right half. Then `M[04400+n]` = address `^ 02000000` (an address ≥ 040000 also gets a row `addr ^ 041000000`). If the entry had bit 48, it is cleared instead |
| `h & 02004000 = 0`, or `h & 02074000 = 02000000` | an instruction | copied |
| `h & 02007700 = 07700`, or with 02000000 and `h & 07700 = 07700` | absolute value | a row `value ^ 040000000` (type 0400), shared by equal values; the address becomes the row (`^ 01000000` without 02000000; with it, `^ 070000` first) |
| with 02000000 and `h & 070000` ≠ 0 | reference to label n | if defined (v = `M[04400+n]`): `40000 \| v`, or `70000 + (v>>24)` when v has 040000; if not, left for pass 6. Labels above the end label get an entry of bit 48 on a forward reference, and theirs is cleared on a backward one |
| other | run-time name `M[02400 + (h & 07777) − 04000]` | on first use a row: a short name `name ^ 043000000`; a long one goes to the queue at 04000 with a 0 row; a `*NAME` found in the 'COMMON' table at 27246 (3 words per entry) gets type 047 (or 067 if long) with its address. The entry is replaced by the row number; the address field becomes the row (`^ 070000` with 02000000) |

The pass ends at the definition of the end label (02020, `1nnnn`): it prints the last row of
the line table and an empty line, and leaves the next row number in M5 for pass 6. The rest
of the stream is not read.

### Line table

Rows of 8 lines: `%4d - ` and a 5-character cell per line, the octal code length (02054) at
the line's marker, blank if none. A marker of a line past the row prints the row first.
Trailing blanks are not printed.

### Error

`PROGRAM TOO LONG` when the code reaches 034000 words, or (from pass 6) when cmd + const ≥
0100000. It sets `FLGEX*` and no object is made (I, not triggered).

### Empty lines

G01713 prints an empty line, but the printer shows it only with `*call setftn:one` (seen in
the tests with a `.pre`).

## Pass 6: G26666 (V)

It writes the standard array **backwards** with 27137 (words) and 27114 (halves). Reversing
its writes gives the object after the entry pair. `src/pass6.cpp` reproduces all test objects.

Inputs: the number of symbol rows in register M5 (left by pass 5), the cells 02005 (module
name), 02137 (a second name), 02050 (string pool end << 12), 02054 (code length), 02055
(number of constants), the rows at 03000, the long names at 04000, the label table at 04400,
drum 27 zones 33 (04000–05777 as pass 5 left it) and 34 (strings).

1. Row 0 = the module name: 02137 if nonzero, else 02005.
2. Each zero row (a long-name placeholder) becomes `(04000+rows+k)<<24 | 0630`, k counting
   the long names. A row of type `067x` also takes the next long name: its ref is ORed with
   `04000+rows+k`.
3. Constants = 02055 + string words (`(02050>>12) − 01634`), stored back in 02055. If code +
   constants ≥ 0100000: `PROGRAM TOO LONG` (G26544).
4. Write the long names (04000+k−1, k = n…1), the rows (backward), and the constants: the last
   `const` words of zone 33 followed by the string words of zone 34, read backward.
5. Read the pass-5 stream backwards and write halves, a right half first. Per half `h`:
   - `h & 03700000 = 02000000` (align): write `02200000` (UTC 0);
   - `0211nnnn` (label n): `M[04400+n]` = the words still to write, i.e. the label's address
     (the original stores with `ATX 074400` and C = `h & 077777`);
   - other halves with bits `02000000` and `030000` and without `040000`: a reference to
     label n. With v = `M[04400+n]`: if `v & 040000` = 0, the address becomes `40000 + v`;
     otherwise it is `70000 + r`, where row r equals `v ^ 041000000`;
   - anything else: copied.
   It stops when the code length is written; earlier items are not read.
6. Write the 10-word header backward: head 1, sym rows−1, 0, 0, long n, cmd, 0, const, 0, 0.
7. Fill `ENTRY*`: (row 0, `2000000000000000`), and with 02137 ≠ 0 also (02005, same), then 0.
   The monitor punches these as the entry pairs before the object. Then call G30257.

## G30257 — summary line (V)

- Prints `PROGRAM ДЛИHA: <cmd+const decimal> <octal>B BPEMЯ: s.t CEK. KAPT: n`
  (`%5u %05oB`, time `%5.2f`, cards `%4u` from 01741). With 02006 ≠ 0 the name is taken
  from it instead of `PROGRAM` (I, not seen in the tests).
- The time is the difference of the clock readings at the start of pass 1 and here
  (G27645).
- `KAPT` is the card count.

## G30120 — run-time name table (V)

- Copies 0132 words (30124–30255) to 02400–02531: the TEXT names `A/START`, `A/SPRI`,
  `SIN*`, …
- Pass-4 code refers to entry k as address `04000+k`.
- Pass 5 overwrites each used entry with its row number, which is why the table is restored
  for every compilation.

## G27733 — formatted line (V)

- Call: push the format address, push the data address, `VJM`.
- Format bytes: 001–0177 a literal character; 0200+n inserts data word n (an ISO string of
  up to 6 characters, ending with a zero byte); 000 ends the format.
- Prints through `A01644`.

## G27645

`27645`/`27646` read a clock (`*50 070100`) into 02141/02140; `27652` prints a source line.

## Notes

`scripts/trace-compile` sometimes reports a single instruction "in G22162" before the driver
starts. It is monitor code that happens to match the image at that address, so ignore it.
