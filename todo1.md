# Milestone 1 — C++ rewrite, BESM-6 object output

Goal (from [Roadmap.md](Roadmap.md)): our compiler produces, from each program in
`tests/algol/`, the same object (`.std`) as the original. Done when `compile_test` and
`run_test` pass for every program, with none skipped.

What we know about the original is in `doc/`:
- [passes.md](doc/passes.md): the six passes, their streams and data;
- [tables.md](doc/tables.md): the tables;
- [runtime-abi.md](doc/runtime-abi.md): the run-time calls and code patterns;
- [stdarray.md](doc/stdarray.md): the object format;
- [encoding.md](doc/encoding.md): the input.

## Approach

**A faithful rewrite, pass by pass (A7.1).** Each pass of the original becomes one C++
module with the same algorithm and the same intermediate stream, so we can compare outputs
pass by pass. The drum, the pages and the backward writing are not reproduced, because they
only work around memory limits. Kept exactly is everything that reaches the object: the order
of constants, the slot numbers where they matter, frame offsets, register choice, symbol rows.

| Module | Original | Contents |
|---|---|---|
| `src/item.h` | — | `Item` (code 7 bits, attribute 5 bits, value 12 bits ↔ 24-bit half-word); `Stream` = `std::vector<Item>` |
| `src/workspace.*` | 02000–05777 | the shared tables as 48-bit words at their original addresses: name/constant table (02144–03777), string pool, D-list, standard identifiers |
| `src/tables.*` | data of the image | keyword table, character classes, symbol attributes, error classes, messages, run-time names, pass-2/4 dispatch tables, code templates; **generated from the overlay image**, not typed by hand |
| `src/listing.*` | ASMPRINT, 01644, G27733, G30001 | the listing: numbered lines, page header, formatted lines |
| `src/pass1_lexer.*`, `src/pass1_parser.*` | G14504 (lexer), G13666 + G14504 | cards → Stream 1, tables, syntax errors |
| `src/pass2.*` | G17223 | Stream 1 (backward) → Stream 2, semantic errors |
| `src/pass3.*` | G22162 | Stream 2 → Stream 3 (skipped with `--check`) |
| `src/pass4.*` | G23523 | Stream 3 → instruction half-words + pseudo-items |
| `src/pass5.*` | G26260 | addresses, symbol rows, line table |
| `src/pass6.*` | G26666, G30257 | `StdArray` and the summary line |

**Checking each pass on its own (A7.2).** Capture the stream the original writes at the end
of each pass (`dubna -d m` traces of the stream helpers' writes; the prototype is the passes
3–4 analysis script) and store it as expected output, like `.std`. Each C++ pass can then be
developed and tested with the original's input stream, in any order:
- pass 1 by its output stream and the `.lst` listing up to the line table;
- passes 2–5 by their output streams;
- pass 6 by the `.std` file.

Data that is not in the stream can be compared by dumping memory at the pass boundary:
- the name/constant table and the D-list after pass 1;
- the array records after pass 2;
- the symbols and labels after pass 5.

## Tasks

### B2. Pass 1 (largest risk: the listing must match)

- **B2.1** Card reader: UTF-8 → the 8-bit codes of the original (`dubna`'s mapping, plus the
  Unicode operators of [encoding.md](doc/encoding.md)), 72 columns, upper case.
- **B2.2** Lexer `AG15414`: names (6 characters, hashing into the table), keywords, numbers
  (the BESM-6 formats, overflow errors), strings, comments, END comments.
- **B2.3** Parser: blocks, statements, declarations, expressions, FOR, procedure heads;
  statement markers; the D-list.
- **B2.4** Errors `AG17015`: numbers, classes, recovery; `↑EOP↑ …`, `HET ′EOP′ !`.
- **B2.5** Listing: numbered lines, two-line split for the narrow printer, the banner.
- Done when stream 1, the tables and the listing match for all programs, the `err_*`
  programs included.

### B3. Pass 2

- **B3.1** Scopes and identifier binding; `TWICE DECLARED`, `IS NOT DECLARED`.
- **B3.2** Type checking by recursive descent over the reversed stream; the messages in
  reverse order with `NEXT STATEMENT IS l/s`.
- **B3.3** The output stream; array records; `* * INCORRECT ALGOL PROGRAM * *`.
- Decode the rest of the pass-2 output codes along the way (an open point in passes.md).

### B4. Pass 3

- **B4.1** Constant folding, merging of procedure jumps.
- **B4.2** Strength reduction of subscripts in FOR loops: register choice, preamble,
  increments.

### B5. Pass 4 (the largest pass; many handlers still unknown)

- **B5.1** The template interpreter and the dispatch by item code (generated tables).
- **B5.2** Handlers group by group, each tested by stream 4 on the feature programs: arithmetic
  and constants, assignment, booleans, IF/goto/labels, FOR, blocks and frames, arrays,
  procedures and parameters, switches, I/O and strings, `'CODE'`/`'ALGOL'` externals.
- **B5.3** `--check`: the `A/SUBCH` calls; expected outputs for a `*check` variant of the
  array programs.

### B6. Pass 5

- **B6.1** Labels, run-time names → symbol rows (0430/0630/0400), alignment, `PROGRAM TOO LONG`.
- **B6.2** The line→address table in the listing.

### B7. Pass 6 and integration

- **B7.1** Assembly of the object into `StdArray`; the summary line `PROGRAM ДЛИHA: …`
  (`BPEMЯ` printed as 0.00).
- **B7.2** The whole pipeline: `compile_test` and `run_test` pass for every program.
- **B7.3** `--full`: the object listing of `A/PRINTM` (its modules are not analysed yet; do
  this last, or move it to a later milestone).

## Order

With the captured streams and `pass_test`, each pass can be written and tested on its own.
Suggested order: B7.1 and B6 (small, and they give whole objects from captured streams
early), then B2, B3, B4, B5, and B7.2–B7.3 last.
