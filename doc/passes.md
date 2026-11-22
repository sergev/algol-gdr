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
| `CHECKFL*` | `*CHECK` | skip G22162; the code gets run-time checks (more externals) |
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

## What each pass does

From the traces (A6.1) and the experiments above. A6.3 adds the details.

| Module | Calls | Runs on Hello, World | Known so far |
| --- | --- | --- | --- |
| G13666 | G14504, G27645, G27220, G27126 | 160 + 1971 (G14504) | pass 1: reads the source card by card, prints the numbered listing; syntax errors `ERROR nn` come out after their line |
| G17223 | G27126, G26666, G30001 | 3933 | pass 2: declarations and identifiers; prints `IDENTIFIER … IS NOT DECLARED`, `TYPE ERROR …` after the listing; sets `FLGEX*` |
| G22162 | G27126 | 185 | pass 3: skipped in `*CHECK` mode |
| G23523 | G27126, G30034 | 1105 | pass 4 |
| G26260 | G27126, G30001, G30120, G27733, ASMPRINT | 2299 | pass 5: prints the table `CTPOKИ OTHOCИTEЛЬHЫE AДPECA` (line → addresses)? |
| G26666 | G27126, G30257 | 804 | pass 6; G30257 then prints `PROGRAM ДЛИHA: …` and punches the object? |
| G27126 | — | 1727 in 141 calls | helper used by all passes (reading or writing intermediate data?) |
| G27645, G30001, G30034, G27733 | | | helpers |

`scripts/trace-compile` sometimes reports a single instruction "in G22162" before the driver
starts. It is monitor code that happens to match the image at that address, so ignore it.
