# Pass 2 on a machine

Pass 2 is C++ routines that work on a BESM-6 state (`Machine`), as the passes of the
overlay did. Passes 3 and 4 have left it (todo1.md): they work on `Context` alone
(`Pass3`, `Pass4`: handlers chained by the continuation addresses of the original, kept on
their own stack; streams as vectors).

## Pieces

- `src/machine.*`: memory (32K words), M1–M15, A, Y, the mode register and the modifier C,
  with the semantics of dubna's `processor.cpp` (arithmetic through `Arith`). Each routine is
  a native registered at its original address; `run()` calls the native at pc and goes on
  at the address it returns (the return register of its callers). `call(addr, reg, ret)`
  runs a routine as a VJM through `reg` at `ret`−1 would.
- `src/translated.*`: `TranslatedPass` sets the machine up from the `Context`, writes the
  input stream through the stream module, runs the pass from its entry with M13 = its return
  address in the driver, records the items written at 27114/27126/27137 and copies the
  workspace, registers and `FLGEX*` back. Its natives are the routines the passes share.

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

Shared natives (src/translated.cpp), instruction by instruction where their state matters:

| Address | Routine |
|---|---|
| 01644 | `print_text`: a text, split after 10 words; state cells 01736–01740 |
| 12000 | ASMPRINT: pops the last and the first word, takes the indent in A, prints one row through `Listing::row`; leaves M14 = 0 and pops the word under them into A |
| 27114–27213 | the stream module: `write_back`, `write_forward`, `write_word_back`, `read_back`, `read_forward`, `read_word_back`, `page_io` |
| 27733 | `format_line` |
| 30001 | `itoa6` |

`*70` moves drum pages and sectors into `Workspace::drum`.

Routines of the passes, each working on the machine's memory and leaving its registers as
the original does:

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
| 2 | 17544–17574 | procedure declarations, NO ASSIGNMENT TO |
| 2 | 20445–20565 | I/O statements, string slices |
| 2 | 20571–21342 | calls, arguments, thunks, VALUE, standard and non-ALGOL procedures |


Natives write memory through `Machine::store`, which drops stores to address 0 as the
machine does: a descriptor address of 0 (after a thunk) is common, and address 0 must keep
reading as zero.

A native may run another with `Machine::call(addr, reg, ret)`: as a VJM through `reg` at
`ret`−1 would, so the registers and the cells the routine saves them in come out as in the
original; `run()` returns when the routine jumps to `ret` (`m.stop`).

## Input stream

A pass reads the stream of the previous pass where that pass left it: in the
stream pages (06000–11777) and on drums 26 and 27, with the cursors 02000/02001. After a
pass they are in the `Context`; for the tests, `NAME.mem` has them. Pass 1 is
C++ and keeps its stream in a vector, so pass 2 first writes it again as the original pass 1
does: forward through 27126 from the cursor at 15403. Pages, drum zones and cursors are then
those of the original.

| Pass | Entry | Returns to | Reads | Writes |
|---|---|---|---|---|
| 2 | 17223 | 01627 | backward (27143) | backward (27114), from the cursor at 22113 |

Pass 4 also calls 30034–30117 (ISO name to TEXT).
