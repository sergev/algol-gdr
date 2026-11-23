# Reference toolchain

The original Algol-GDR compiler runs under the `dubna` simulator. Two scripts wrap it:

```
scripts/ref-compile [--full] FILE.alg [OUTDIR]   # → NAME.std, NAME.lst (NAME.full.lst)
scripts/ref-run FILE.std                         # → job output on stdout
```

`ref-compile` exits with status 1 and writes no `.std` file when the program has errors; the
`.lst` file then holds the diagnostics.

## Side files

A program `NAME.alg` may come with:

| File | Use |
| --- | --- |
| `NAME.pre` | control cards after `*name`, in both jobs (e.g. `*call setftn:one,long`, `*check`); our compiler takes them as `--setftn=…`, `--check` |
| `NAME.run` | cards before `*execute` in the run job: `*library:…`, or other modules (`*ftn`, `*assem` sections) |
| `NAME.in` | terminal input of the program, fed to `dubna` as stdin (default: empty) |
| `NAME.norun` | the program is compiled but never run (the file says why) |

## Job layout

Compile:

```
*name NAME
*punch          ← must come before *algol, or nothing is punched
*full           ← only with --full
*algol
…program text, ending with 'EOP'…
*end file
```

The punched object comes out in `stdarray.out` (card text form, see `dubna/puncher.cpp`),
and the Braille image of the same cards in `punch.out`. `dubna` writes both into the current
directory, so the scripts run it in a temporary directory.

Run:

```
*name NAME
…contents of NAME.std…
*execute
*end file
```

## Determinism

- `dubna -r` turns off randomization and fixes the date and time to `04 ИЮЛ 24 23.45`. With
  it, the listing, object file and run output are identical from run to run.
- The closing statistics of `dubna` (elapsed time, simulation rate) vary.
  `scripts/dubna-filter` keeps only the lines from the `*NAME` card up to the line of
  60 dashes before the statistics. It also drops the banner.
- `*full` does not change the object file: only the listing gets the object code.

## What the outputs contain

- `.lst`: the control cards, the numbered source, diagnostics, the line/address table, and the
  program length line (`PROGRAM ДЛИHA: …`). With `*full`, the object code follows: the
  instructions, the constants with their text, and the table of external names (`A/START`,
  `A/SPRI`, …).
- `.out`: the control cards, the load map between the `≠` lines (each runtime routine the
  program pulled in, with its address), then the program's own output.

## Pass streams

`scripts/capture-streams FILE.alg…` compiles under a trace of memory and extracodes
(`dubna -d ime`) and writes next to the source:
- `NAME.s1` … `NAME.s5`: the stream written by each pass, one 24-bit item per line in octal,
  in memory order (pass 2 writes backward, so its items are reversed). Items are taken at the
  entries of the stream helper G27126; pass 3, which goes back over what it wrote, has the
  stream it left on drum 26 (zones 10…, up to EOP 075). A pass that did not run has no file.
- `NAME.mem`: per pass, the words it changed: in the workspace (01736–11777 with the stream
  pages, 27245–27644) as `ADDR VALUE`, and on the scratch drums 26 and 27 (data and streams
  between passes) as `drum UNIT ZONE OFFSET VALUE`; all numbers octal. The workspace at the
  start of pass N is the driver's data (`src/tables_data.cpp`) plus the changes of the passes
  before N. A pass ends when the driver goes on after it (01626 after pass 1, 01627, 01632,
  01633, 01634, 01635 after pass 6) or at its end 01716; then `reg N VALUE` gives the index
  registers it left (N octal; M17 is not listed).

- `NAME.lex`: at each return of the lexer (AG15414), the symbol word 02000 and register M4.
  The parser changes 02000 between calls, so the values are taken at the returns.

The script keeps a shadow of memory and drums from the start of pass 1 (traced writes, `*70`
transfers) and checks it against every traced read, so the dumps are exact.

`make goldens` runs it for every program. Pass 3 is skipped (no `.s3`) for the programs with
a `NAME.pre`: `*call setftn` and `*check` set `CHECKFL*` (cell 0521), so they are compiled in
check mode. `arrays_check` and `for2_check` are `arrays` and `for2` with `*check`.
After errors in pass 1 or 2 only the streams up to that pass exist.
