# Passes translated from the overlay

Under the hybrid plan (todo1.md), passes 2–4 run first as the code of the original,
translated to C++ one instruction at a time; their routines are then replaced by hand-written
C++ under the tests.

## Pieces

- `scripts/gen-pass.py NAME RANGES…` reads overlay ALGOL from monsys.9 and writes
  `unsigned NAME_code(Machine &m, unsigned pc)`. Every word of the ranges is translated,
  data too (never run). Each half becomes one call of `Machine` (`m.xta(7, 02670)`); a
  word address is a label. Jumps with a constant target go to the label, others (through a
  register, or after UTC/WTC) through a `switch` over the labels. A jump out of the ranges
  returns its address. `--hook ADDR` calls `m.on_entry` there. `--native ADDR` makes the
  code return ADDR when it gets there (by a jump, a call or falling through), so that
  `Machine::run` calls the C++ routine registered at ADDR, which returns the address to go
  on at.
- `scripts/gen-passes` regenerates `src/passN_gen.cpp`. Each pass gets its own copy of the
  shared code it calls: the driver's print routine 01644–01735, the streams 27114–27244,
  the formatter 27733–30000, number to text 30001–30033.
- `src/machine.*`: memory (32K words), M1–M15, A, Y, the mode register and the modifier C,
  with the semantics of dubna's `processor.cpp` (arithmetic through `Arith`). `run()` loops:
  a jump to a native routine runs it and continues at the address it returns (the return
  register of its callers).
- `src/translated.*`: `TranslatedPass` sets the machine up from the `Context`, writes the
  input stream through the original stream helper, runs the pass from its entry with M13 =
  its return address in the driver, records the items written at 27114/27126/27137 and
  copies the workspace, registers and `FLGEX*` back.

## The machine state a pass expects

| Address | Contents |
|---|---|
| whole overlay 01604–30354 | `tables::image`, then the workspace over it |
| 00521 `CHECKFL*` | check mode (`*call setftn`) |
| 00523 `*REGIM//` | bit 48 `setftn:one`, bit 47 `setftn:long` |
| 00702 `FLGEX*` | number of errors |
| 00705 `NOLISFL*` | no listing |
| 01730 | nonzero: the page header with the date is printed (else 01644 calls `DATAPR*` 01524) |
| M17 | 053401, as the driver calls a pass |
| M13 | return address in the driver: 01627 after pass 2 |

Address 0 reads as zero: stores there are dropped (pass 2 stores to it, then reads it).

Native routines:
- **ASMPRINT 12000**: pops the last and the first word, takes the indent in A, and prints one
  row through `Listing::row`; leaves M14 = 0 and pops the word under them into A (the
  caller's A, as in any call with the arguments on the stack). 01644 itself is translated (its split of long
  texts and its state cells 01736–01740 match the original).
- **`*70`**: drum pages and sectors into `Workspace::drum`.

Routines of the passes rewritten in C++ (`--native` in `scripts/gen-passes`), each working on
the machine's memory and leaving its registers as the original does:

| Pass | Address | Routine |
|---|---|---|
| 2 | 17223 | `start` (src/pass2.cpp): up to the main loop at 17352 |
| 2 | 22027 | `finish`: from the first item to the return into the driver |
| 2 | 17420, 17426, 17430 | `message_name`, `message`, `after_message` |
| 2 | 17535 | `set_position`: the position text at a statement marker |
| 2 | 17371, 17377, 17434 | `inside`, `lookup`, `illegal_use` |
| 2 | 17454, 17455, 17475 | `open_scope_at`, `open_scope`, `close_scope` |
| 2 | 17511, 17523 | `end_item`, `begin_item` |
| 2 | 17605–17775 | identifiers, constants, GOTO's check, TYPE ERROR, the type matrix |
| 2 | 17777–20154 | subscripts, + − × / |
| 2 | 20221–20430 | **, boolean operators, NOT, relations, unary + − |
| 2 | 17675–17737 | assignment |
| 2 | 21366, 21461 | conditional expressions, IF |
| 2 | 17543–17601, 21466–21617 | FOR |
| 2 | 21637, 20162–20200, 21206, 17533 | SWITCH, array segments, copied items, syntax-error marker |
| 2–4 | 27733, 30001 | `format_line`, `itoa6` (src/translated.cpp), in every pass |
| 3 | 22716 | `intern_constant` (src/pass3.cpp) |

A native may run a translated routine with `Machine::call(addr, reg, ret)`: as a VJM through
`reg` at `ret`−1 would, so the registers and the cells the routine saves them in come out as
in the original; the generated dispatch returns when the routine jumps to `ret` (`m.stop`).

## Input stream

A translated pass reads the stream of the previous pass where that pass left it: in the
stream pages (06000–11777) and on drums 26 and 27, with the cursors 02000/02001. After a
translated pass they are in the `Context`; for the tests, `NAME.mem` has them. Pass 1 is
C++ and keeps its stream in a vector, so pass 2 first writes it again as the original pass 1
does: forward through 27126 from the cursor at 15403. Pages, drum zones and cursors are then
those of the original.

| Pass | Entry | Returns to | Reads | Writes |
|---|---|---|---|---|
| 2 | 17223 | 01627 | backward (27143) | backward (27114), from the cursor at 22113 |
| 3 | 22162 | 01632 | forward (27154) | forward (27126), from the cursor at 23501 |
| 4 | 23523 | 01633 | forward (27154) | forward (27126) |

Pass 4 also calls 30034–30117 (ISO name to TEXT).

## Debugging

`ALGOL_TRACE=FILE` makes `TranslatedPass` write the address, half and A before each
instruction. `scripts/trace-diff.py` compares it with a `dubna -d ir` trace of the same job
and prints where the control flow first differs. Reads of cells the C++ passes do not set
exactly (pass 1's scratch cells, the result cell of 30001) show up as differences of A.

## Coverage

`ALGOL_COVERAGE=FILE` appends the halves each translated pass ran. `scripts/coverage.py`
compiles every test program with it and lists per pass the code words never run, each with
the label before it in the Madlen source (code: the C ranges of `algol.sym`/`.tsym` without
the words of six printable characters, which are message texts, plus whatever ran). With the
41 programs of tests/algol:

| Pass | Halves of code run | Unrun |
|---|---|---|
| 2 | 1999 of 2340 (85%) | 20 pieces of 1–6 words: entries of jump tables by type and kind, short branches |
| 3 | 1044 of 1162 (89%) | |
| 4 | 1952 of 2142 (91%) | |
| shared | 279 of 350 (79%) | 27137 (backward write of a word), 27165, 27220, 01653 (no listing) |

Test programs for pass 2 (err_pass2, exprs, exprs2, externals, err_common2) were found by
compiling one statement at a time, every expression of a list in every context of a list,
and keeping the statements that ran new code.

The rewrite of an unrun piece is not checked by the tests: each pass's first task adds
programs that run it, or the piece is kept as found and marked unverified.
