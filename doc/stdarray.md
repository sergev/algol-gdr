# Standard array (object module)

The *standard array* (стандартный массив) is the object module that Dubna compilers write and
that `WRITLIB*` and the dynamic loader read. This note describes it as Algol-GDR makes it.
All numbers are octal.

Sources:
- `mesm6/toolchain/ld/stdobj.{h,c}`: decoder of the format (the layout and symbol types below);
- `os-dubna/docs/Writlib.md`, `Dynamic_Loader.md`, `Madlen.md` §17;
- `dubna/puncher.cpp` and `machine.cpp` (`load_script`): the card text form.

Every claim below was checked on the objects of `tests/algol/` (hello, test1–3).

## Card text form

`*punch` before `*algol` makes the compiler punch the object. `dubna` writes each object card
into `stdarray.out` as 17 lines:

```
`77761 PROGRAM  1          ← card header
`6062574762415500          ← 16 lines, one 48-bit word each
…
```

- **Punched card.** A card has 80 columns of 12 bits.
  - Column 1 is `01200`, the mark of an object card.
  - Column 2 is the card number.
  - Columns 77–80 hold the module name in TEXT code (8 characters).
  - The data words take 8 groups of 8 columns, starting at columns 5, 14, 23, … 68. Each group
    holds 2 words. The column after each group is not copied into the text form.
  - The first card the compiler punches is a title card (column 2 = 0). `dubna` leaves it out.
- **Reading it back.** `dubna` turns each backquoted line of a job into **one word**. It parses
  the octal number after the backquote and ignores the rest of the line. So the header line is
  the word `077761`, and the name and number after it are comments: `` `77761 `` alone works
  just as well.
- **Full cards.** Every card must have all 16 words. When the object ends, the rest of the last
  card is filled with zero words. A shorter last card is rejected at `*execute` with
  `ILLEGAL PLACE FOR BINARY CARD`.

## Logical layout

Words are numbered from 0, after the card headers are removed.

| Part | Length | Contents |
| --- | --- | --- |
| Entries | 2 per entry | pairs *name, info* |
| Header | 10 | lengths of the parts |
| Code | `cmd` | instructions |
| Constants | `const` | initialized data |
| Data | `data` | transient data for SET |
| SET | `set` | SET directives |
| Description table | `head` + `sym` + `long` | the module name, the symbols, the long names |
| Debug | `debug` | debug information |
| Padding | | zeros up to a multiple of 16 words |

### Entries

These are pairs of a name in TEXT code and an info word. The list ends at the first pair whose
second word has zero in bits 48–46. That pair is the start of the header.

Algol-GDR makes one entry: `PROGRAM` / `2000000000000000`. The info word has mark 2 in bits
48–46, and the entry address is in the low bits (here 0).

### Header (unpacked form, 10 words)

| Word | Name | Meaning |
| --- | --- | --- |
| 0 | `head` | length of the description table head (1: the module name) |
| 1 | `sym` | number of symbols |
| 2 | — | 0 (the MESM-6 linker uses it for entry/base addresses) |
| 3 | `debug` | length of debug information |
| 4 | `long` | number of long names |
| 5 | `cmd` | length of code |
| 6 | `bss` | length of uninitialized data |
| 7 | `const` | length of constants |
| 8 | `data` | length of data for SET |
| 9 | `set` | length of SET directives |

The module occupies `cmd + const + bss` words in memory. The listing line
`PROGRAM ДЛИHA: 16 00020B` of Hello, World is `cmd + const` = 5 + 13 = 20 (octal), which is 16
decimal.

Algol-GDR writes `bss = data = set = debug = 0`. (`stdobj.c` also decodes a packed 3-word
header, used by the library, the compact form in Writlib.md.)

### Description table

The description table is the module name, then the symbols, then the long names, numbered as
rows: row 0 is the module name (`head` = 1), rows 1…`sym` are the symbols, and the long names
come after them.

A symbol is a 48-bit word:

| Bits | Field | Meaning |
| --- | --- | --- |
| 48–25 | `ref` | 4 TEXT characters (short name), or `4000+row` of a long name |
| 24–16 | `type` | symbol type |
| 15–1 | `addr` | address or value |

The types Algol-GDR uses:

- `0630`, an external reference with a long name: `ref` = `04000 + row` of a long name.
- `0430`, an external reference with a short name: `ref` holds 4 TEXT characters, as in
  `SIN*`, `COS*`, `LN*`, `EXP*`.

`stdobj.h` lists the other types: entries, commons, expressions and so on.

A long name is one word of 8 TEXT characters, padded with blanks (code 0): `A/START`,
`A/SPRI`, `SQRT*`, …

### References from code

Each instruction holds its address in one of these forms (Madlen.md §17):

| Form | Meaning |
| --- | --- |
| absolute | as is |
| `40000+N` (long address) | address N relative to the start of the module |
| `74000+N` (long address) | the object of row N of the description table |
| `4000+N` (short address) | row N of the description table |

The loader resolves them. For example, `13 VJM 74001` calls the external of row 1, which is
`A/START`.

## TEXT code

TEXT code is 6 bits per character, 8 characters per word, with the first character in the
high bits.

```
00  ␠ . Б Ц Д Ф Г И     20  0 1 2 3 4 5 6 7     40  З A B C D E F G     60  P Q R S T U V W
10  ( ) * Й Л Я Ж /     30  8 9 Ь , П - + Ы     50  H I J K L M N O     70  X Y Z Ш Э Щ Ч Ю
```

## Worked example: Hello, World

```
'begin'
    print(''Hello, World!'');
'end'
'eop'
```

The object is 43 words of content (35 decimal), padded to 60 words (48 decimal, 3 cards).

| Words | Part | Value |
| --- | --- | --- |
| 0–1 | entry | `6062574762415500` = `PROGRAM`, `2000000000000000` |
| 2–13 | header | head 1, sym 3, 0, debug 0, long 3, cmd 5, bss 0, const 13, data 0, set 0 |
| 14–20 | code | 5 words, below |
| 21–33 | constants | 13 words, below |
| 34 | description table, row 0 | `6062574762415500` = `PROGRAM` |
| 35–37 | rows 1–3, symbols | `0000400463000000`, `…400563…`, `…400663…`: type 0630, rows 4, 5, 6 |
| 40–42 | rows 4–6, long names | `A/START`, `A/SPRI`, `A/STOP` |
| 43–57 | padding | zeros |

Code and constants, from the `*full` listing (`40005` is module+5, the start of the
constants; `74001`–`74003` are rows 1–3, the externals):

```
 0   7 VTM 40005          constants
    13 VJM 74001     00  0000000000000000
 1  15 UTM 0         01  0020000000000000
     7 UTC 10        02  6400000000000064
 2  14 VTM 0          …
       ITA 16        07  6400000000000000
 3  13 VJM 74002     10  2204251423047454   HELLO,
       UTC 0         11  1005351724446104   ␠WORLD
 4     UJ  74003     12  1020000000000000   !
       UTC 0
```

The meaning of the constant pool and of the calling sequence is the subject of A6.5–A6.6
([doc/runtime-abi.md](runtime-abi.md), to be written).
