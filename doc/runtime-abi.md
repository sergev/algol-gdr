# Run-time interface and generated code

How code compiled by Algol-GDR (7.01.82) talks to its run-time library, and the patterns the
compiler emits. A rewrite must emit the same code.

## Sources and notation

- Run-time library: `os-dubna/besmmons/system1.txt` (S1) and `system2.txt` (S2).
- Examples: `tests/algol/*.std`, decoded with `algc --dump`. Register numbers are
  decimal, addresses octal. `@n` is an address relative to the module, `74nnn` is a
  description-table row, and `4nnn` is a short reference to a row.
- **V**: verified by a run, a trace or an object listing. **I**: inferred from the run-time
  source.

---

## A. Run-time interface

### A.0 General conventions

**Value formats** (V)
- Integer: an unnormalized real with exponent field 0150, so the 41-bit two's-complement
  mantissa is the integer. `6400000000000005` = 5, `6437777777777777` = −1.
- Real: a normalized BESM-6 float. `4050000000000000` = 1.0, `4010000000000000` = 0.5.
- Boolean: FALSE = 0, TRUE = `0020000000000000` (bit 41). Any non-zero word counts as true.
- String: the 15-bit address of its text, or a *short string* when
  `A & 7760000000000000 ≠ 0`.

**Mode register.** R = 6 at the entry and exit of every routine and at every statement
boundary (Madlen §14.1). Integer `+` and `−` run with R = 3. (V)

**Linkage** (V)
- Most routines are called with `13 VJM` and return with `13 UJ`.
- `A/AND`, `A/NOT`, `A/EQUIV`, `A/RTOI`, the `A/PARxxx` routines and `A/ARRD/x` are called
  with `14 VJM`.
- A call returns to the next word, so a `VJM` in a left half has `UTC 0` (`02200000`) in its
  right half.

**Operands.** One argument goes in A. With two, the left one is pushed with `XTS` and the
right one is in A. Routines pop their stack operands. (V)

**`A/I0` and `A/MINUS` are data words**, not routines (S2:4876, set at S2:4911):
`A/I0` = `6400000000000000`, `A/MINUS` = `0020000000000000`. Code reads them through a short
reference such as `A+X 4003`. (V)

**External names.** Names of at most 4 characters get symbol type 0430; longer names get
0630 plus a long-name row. Rows are added in the order of first reference in the code. (V)

### A.1 Table

| Routine | Call | In | Out | Notes |
|---|---|---|---|---|
| A/START | `7 VTM consts; 13 VJM` | M7 | M6 = M15 = frame base | the first instruction (V) |
| A/STOP | UJ | – | – | flushes the print line, then STOP* (V) |
| A/IPRI | 13 | A = int | – | 15-column field (V) |
| A/RPRI | 13 | A = real | – | 20-column field (V) |
| A/BPRI | 13 | A = bool | – | `TRUE`/`FALSE` (V) |
| A/SPRI | 13 | A = string | – | no padding (V) |
| A/LPRI | 13 | A | – | octal; not used by the tests (I) |
| A/NLINE, A/NPAGE | 13 | – | – | new line, new page (V) |
| A/AND, A/EQUIV | 14 | stack = left, A = right | A | (V) |
| A/NOT | 14 | A | A ∈ {0, TRUE} | (V) |
| A/IMPL | **13** | stack = left, A = right | A | copies M13 to M14 itself (V) |
| A/RTOI | 14 | A = real | A = int, ⌊x + ½⌋ | real-to-integer assignment (V) |
| ENTIER* | 13 | A = real | A = int, ⌊x⌋ | (V) |
| A/SIGN | 13 | A | A = −1, 0 or 1 | (V) |
| A/RHOCHI | 13 | stack = real base, A = int exponent | A = real | (V) |
| A/IHOCHI | 13 | stack = int base, A = int exponent | A = int | uses cell 021 (V) |
| POWER | 13 | stack = base, A = real exponent | A = real | extracodes *55/*56 (V) |
| SQRT*, SIN*, COS*, ARCTAN*, LN*, EXP* | 13 | A | A = real | (V) |
| ABS | inline `AMX 0` | | | (V) |
| A/ARRD/K, A/ARRD/V | 14 | stack = bounds; M11, M12, M13 | descriptors | (V) |
| A/SUBCH | 13 | A = descriptor, M12 = address | – | bounds check (I) |
| A/OPTLIN | 13 | M12, M14, M13 | steps in cells 020+r | (V/I) |
| A/VLM1 | 13 | stack = init, A = end, M14 = exit | M11 = count | (V) |
| A/CALLP | 13 | A = s, M14 = entry | A = result | procedure with parameters (V) |
| A/CALLS | 13 | M14 = entry | A = result | procedure without parameters (V) |
| A/PROCRT | UJ | A = result | – | return from a procedure or thunk (V) |
| A/PARVAL | 14 | [M15−1] = s, A = value | A = s | (V) |
| A/PARVAR, PARCON, PARSTR, PARPAR, PAREXP, PARLAB | 14, then inline half `r UTC a` | A = s | A = s | (V) |
| LENGTH | 13 | A = string | A = int | (V) |
| A/SEQS, SNES, SGTS, SLTS, SGES, SLES | 13 | stack = left, A = right | A ∈ {0, TRUE} | string relations (V) |
| A/SPLUSS | 13 | stack = left, A = right | A = string | concatenation in the heap (V) |
| A/SLICE | 13 | stack = i1, i2; A = string | A = string | `S[i]` passes i1 = i2 (V) |
| 'CODE' procedures | 13 | argument addresses pushed with ITS, the last in A | A | Madlen §14.2 (V) |

### A.2 Notes per routine

**A/START** (S1:2609, V)
- Sets bit 48 of cell 044, which marks an Algol program.
- Takes the stack base from cell 017.
- Stores `(ret<<24)|oldsp` at [M6−2].
- Sets M15 = M6 = base + 2 (01742 in the trace).

**A/STOP** (S1:2587). If `STOPFLG*` is set, it flushes the print line first. (V/I)

**Printing** (A/DRUCK, S1:1006–1300, V)
- Each entry saves M8–M13 on the stack, so the argument is at [M15−7].
- The column counter REKZ starts at −126.
- If the field does not fit (17 columns for an integer, 24 for a real, 6 for a boolean),
  the line is printed first.
- `A/SPRI` prints 8-bit bytes from the high end of each word, 6 per word, up to the first
  zero byte.

**Booleans** (S1:509–534, V)
- AND: a right operand of 0 gives 0; otherwise the left operand.
- EQUIV: a non-zero right operand gives the left operand; otherwise NOT left.
- NOT: 0 gives TRUE, anything else gives 0.
- IMPL: a right operand of 0 gives NOT left; otherwise TRUE.

**A/RTOI** is `A+X 0.5; NTR 3; A+X A/I0; NTR 2; 14 UJ`. **ENTIER*** is the same without the
0.5, and returns with `13 UJ`; ENTIER(−2.5) = −3. (V)

**A/SIGN** (S1:578) does `A+X 0; NTR 7`. It returns `A/I0` for zero, otherwise ±1. (V)

**A/RHOCHI, A/IHOCHI** (S1:547/569). RHOCHI multiplies repeatedly, and for a negative
exponent divides into 1.0. IHOCHI keeps its return address in cell 021 and converts the
result to an integer. (V)

**POWER** (S2:5267, V/I)
- `x↑0` = 1.0, and `0↑0` is an error.
- An integral exponent with a negative base gives |a|^b with the sign from the parity of b.
- Otherwise the result is `exp(b·ln a)`.

**Standard functions** (S2:7489, V). Each entry is one word, `14 VTM k; UJ *50`, with
`*50: A+X 0; 14 *50; 13 UJ`. The codes are SQRT 0, SIN 1, COS 2, ARCTAN 3, LN 5, EXP 6.

**A/ARRD/K, A/ARRD/V** (S1:594–700, V)
- Inputs:
  - the bound pairs pushed in declaration order, with the last upper bound in A;
  - M11 = number of dimensions;
  - M12 = number of arrays with these bounds;
  - M13 = address of the first descriptor.
- K: the top-level address vector is already reserved in the frame. V: everything goes on
  the stack.
- Descriptor word: bits 1–15 the virtual origin, bits 16–30 the element count, bits 31–45
  the start address.
- Example: A[1:5] gives `0022300000502227`.

**A/SUBCH** (S2:2695, V). The subscript check: start ≤ M12 < start + count, otherwise
error 4. Pass 4 emits a call after each subscript computation in `*CHECK` mode (and in
some cases without it, as in quinio).

**A/OPTLIN** (S1:1676, V/I). Strength reduction of subscripts that are linear in the loop
variable.
- The first call pushes the index registers.
- The second call stores the step of each register in cell 020 + r.
- The loop body then steps a register with `WTC 032; 10 UTM 0`.

**A/VLM1** (S1:2718, V). Sets M11 = init − end, and the body repeats with `11 VLM`. It is
used when the loop variable only indexes arrays, and it does not update the loop variable.

**Procedure calls** (S1:720–870, V)
- CALLP does `ATI 15`, then falls into CALLS.
- CALLS pushes the return address and M1–M7 (8 words), then `14 UJ`.
- PROCRT pops them and returns with `13 UJ`, with the result in A.

**Parameter cells** (V). Each actual parameter is a 2-word cell at the top of the stack:

| Routine | cell[0] | cell[1] |
|---|---|---|
| PARVAL | `11 UJ 0 / 0` | the value |
| PARVAR | `UTC 0 / 12 VTM addr` | `12 XTA 0 / 13 UJ` |
| PARCON | `12 VTM 66666 / 11 VTM addr` | `11 XTA 0 / 13 UJ` |
| PARSTR | `UTC 0 / 14 VTM addr` | `ITA 14 / 13 UJ` |
| PAREXP, PARLAB | `11 VTM irp / 12 VTM s` | `14 VTM body / UJ PARBEG` |
| PARPAR | a copy of the 2 words at M[r] + a | |

`addr` = M[r] + a, taken from the inline half-word after the call; PARCON and PARSTR always
use M7 as the base. PARBEG (S1:746) saves the registers, restores the caller's environment
registers, and jumps into the thunk body, which ends with `UJ A/PROCRT`.

**'CODE' procedures** (V). Each argument is pushed with `15 ATX 0`. Its address is formed
through a type-0400 description-table row holding `=77777` (`15 UTC 74nnn; 14 VTM 0`) and
pushed with `ITS 14`. Then `13 VJM`. Why 77777 needs its own row is not known.

---

## B. Generated code

### B.1 Registers (V)

| Register | Use |
|---|---|
| M7 | constant-pool base |
| M6 | main program frame (level 0) |
| M5, M4, … | procedure frame at level 1, 2, … (M(6−level)) |
| M8–M10 | scratch, e.g. array indexing |
| M8 | switch index |
| M11 | VLM loop counter; the link in PARBEG and in the value-parameter prologue |
| M12 | address delivered by a thunk |
| M13, M14 | call links |
| M15 | stack pointer, grows upward |

Blocks get no frame of their own: their locals live in the frame of the enclosing procedure.

### B.2 Program (V)

```
 7 VTM 4cccc ; 13 VJM A/START
15 UTM n                       ; the whole main frame, all blocks included
   …
 6 UTC n ; 15 VTM 0            ; only if the main program declares arrays
   UJ A/STOP
```

### B.3 Constant pool (V; I where noted)

From M7 upward:
1. numeric constants, in reverse order of first occurrence;
2. the fixed block: `0`, `0020…` (TRUE), integers 52, 51, 50, 2, 1, 0;
3. strings, in source order.

- The fixed block is the table `AD03771` set up by the driver, and literals share it: TRUE,
  1, 2 and 0 are read from there.
- 50–52 are used only when the source contains those literals.
- Constants that code generation itself creates (raw loop steps, folded negatives) come
  below the literals; their exact order is only partly understood (I).
- Constants are converted to the target type at compile time.
- Strings are 8-bit ASCII, 6 characters per word, ending with a zero byte. A string of
  exactly 6k characters gets an extra zero word.

### B.4 Variables (V)

- Within a declaration group, offsets descend: the last declared variable gets offset 0.
- Inner blocks lie above outer ones in the frame.
- A block with arrays keeps a stack mark (`ITA 15; 6 ATX m`).
- Hidden cells (such as a for-loop's return address) also live in the frame.

### B.5 Arithmetic (V)

| Operation | Code |
|---|---|
| integer + / − | `NTR 3; A+X; NTR 6` |
| integer × | `A*X; NTR 3; A+X A/I0; NTR 6` |
| `/` (always real) | both operands get `A+X 0`, then `15 STX 1; 15 A/X 2` |
| unary minus | `AVX A/MINUS` |
| int → real | `A+X 0` |
| real → int on assignment | `A/RTOI` |
| `x↑3`, `x↑(−2)` | inline multiplications |

`NTR 6` is emitted lazily, just before the next call or label. Temporaries go on the stack:
`15 ATX 0` pushes, `15 op 0` pops.

### B.6 Procedure declaration (V)

```
15 MTJ 5 ; 15 UTM n
   …
 5 XTA result ; 5 MTJ 15 ; UJ A/PROCRT
```

- Bodies are inline in the code and jumped over with `UJ`.
- Parameter cell i is at M5 + 2i.
- A procedure passed as an actual parameter somewhere also gets a value-parameter prologue
  (I: the exact rule).

### B.7 Procedure call (V)

```
   ITA 15 ; 15 UTM 8
   <one A/PARxxx call per parameter>
14 VTM entry ; 13 VJM A/CALLP          ; no parameters: 14 VTM entry; 13 VJM A/CALLS
```

- A formal procedure is called with `5 UTC 2k; 13 VJM 0`.
- An array is passed with PARPAR on its descriptor.

### B.8 Name parameters in the callee (V)

- Fast path when every actual is a simple variable: `5 WTC 0; XTA 0` or `… ATX 0`.
- General case: call the thunk with `5 UTC 2k; 13 VJM 0`; on return A = value and M12 =
  address. `X := X + 1` calls it twice.

### B.9 Booleans and relations (V)

| Expression | Code |
|---|---|
| a < b | `A-X; AAX A/MINUS` |
| a > b | `X-A; AAX A/MINUS` |
| a ≤ b | `X-A; AAX A/MINUS; AEX A/MINUS` |
| a = b | `AEX; VJM A/NOT` |
| a ≠ b | `AEX` alone (a non-canonical TRUE) |
| a OR b | `AOX` inline |
| AND, NOT, IMPL, EQUIV | routine calls |

In conditions the compiler branches directly: `X-A; UZA` or `AEX; U1A`.

### B.10 Labels and goto (V)

- A label that can be reached from a deeper context starts by resetting the stack:
  `6 UTC n; 15 VTM 0`, or `6 WTC mark; 15 VTM 0` when its block has arrays.
- A goto out of a procedure is a plain `UJ` to that reset.
- A label parameter is passed with PARLAB; the callee jumps with `5 UJ 0`.

### B.11 Switches (V)

```
head:  8 UJ head          ; jump to head + M8
         UJ <element 1>
         UJ <element 2>
goto S[I]:  6 WTC I ; 8 VTM 0 ; UJ head
```

There is no bounds check: `S[0]` loops forever.

### B.12 for statements (V)

- Integer step +1: increment with `ARX` of the raw word `0000000000000001`; test with
  `X-A end; U1A exit`.
- Negative or variable step: a hidden frame cell holds a return address, and the test uses
  the sign of the pushed step (`15 AVX 0`).
- For-lists: each element stores the address of the next one in that cell; the body ends
  with `6 WTC h; UJ 0`.
- `while`: a plain loop with a test and a branch.
- Loops whose variable only indexes arrays use `A/VLM1` or `A/OPTLIN`.

### B.13 Arrays (V)

```
   <push bounds, the last upper bound in A>
11 VTM ndim ; 12 VTM narr ; 6 UTC fw ; 13 VTM 0
14 VJM A/ARRD/K   (or A/ARRD/V)
   ITA 15 ; 6 ATX mark
```

- `A/ARRD/K` is used only when all bounds are literals. A bound written `-1` counts as an
  expression and gives `A/ARRD/V`.
- Access:
  - 1-D with constant bounds: a direct frame offset;
  - 1-D with dynamic bounds: `6 WTC d; 5 VTM 0; 6 WTC I; 5 XTA 0`;
  - 2-D: `6 WTC d; WTC j; XTA i`.
- A block or procedure with arrays resets the stack on exit.

### B.14 Printing and strings (V)

- `PRINT` makes one call per argument, chosen by its type.
- A string literal is passed as an absolute address: `7 UTC off; 14 VTM 0; ITA 14`.
- `S[i]`: push i twice, A = S, `VJM A/SLICE`.
- String parameters: PARVAL by value, PARSTR by name.

### B.15 Not supported

`'OWN'` (error 34), value arrays, and `'DIV'` (error 05). A type mismatch between an actual
and a formal parameter is a compile error.

## Open questions

- When `A/SUBCH` is emitted without `*CHECK` (quinio).
- The exact rule for the value-parameter prologue.
- The order of constants that code generation creates.
- Why `'CODE'` calls need a description-table row holding 77777.
