# Reference toolchain

The original Algol-GDR compiler runs under the `dubna` simulator. Two scripts wrap it:

```
scripts/ref-compile [--full] FILE.alg [OUTDIR]   # → NAME.std, NAME.lst (NAME.full.lst)
scripts/ref-run FILE.std                         # → job output on stdout
```

`ref-compile` exits with status 1 and writes no `.std` file when the program has errors; the
`.lst` file then holds the diagnostics.

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
