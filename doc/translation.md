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
  returns its address. `--hook ADDR` calls `m.on_entry` there.
- `scripts/gen-passes` regenerates `src/passN_gen.cpp`. Each pass gets its own copy of the
  shared code it calls: the driver's print routine 01644–01735, the streams 27114–27244,
  the formatter 27733–30000, number to text 30001–30033.
- `src/machine.*`: memory (32K words), M1–M15, A, Y, the mode register and the modifier C,
  with the semantics of dubna's `processor.cpp` (arithmetic through `Arith`). `run()` loops:
  a jump to a native routine runs it and continues at M13.
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
  row through `Listing::row`; leaves M14 = 0. 01644 itself is translated (its split of long
  texts and its state cells 01736–01740 match the original).
- **`*70`**: drum pages and sectors into `Workspace::drum`.

## Input stream

The stream of the previous pass is written again as that pass wrote it: pass 1 forward
through 27126 from the cursor at 15403 (02001). Pages, drum zones and the cursors are then
those of the original.

## Debugging

`ALGOL_TRACE=FILE` makes `TranslatedPass` write the address, half and A before each
instruction. `scripts/trace-diff.py` compares it with a `dubna -d ir` trace of the same job
and prints where the control flow first differs. Reads of cells the C++ passes do not set
exactly (pass 1's scratch cells, the result cell of 30001) show up as differences of A.
