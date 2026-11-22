# Traces of the original compiler

`scripts/trace-compile` compiles programs with the original Algol-GDR under `dubna -d i` and
summarizes which modules of the overlays `ALGOL` and `A/PRINTM` ran:

```
scripts/trace-compile [--full] tests/algol/hello.alg      # per module: insns, stays, callers
scripts/trace-compile --full --coverage tests/algol/*.alg # per module: half words executed
python3 scripts/trace-passes.py --sequence …              # every stay, in order
```

It needs `os-dubna` next to this project (or `OS_DUBNA=…`): the layouts
`os-dubna/algol/*.layout.py`, and the tape `os-dubna/monsys/monsys.9`, from which it extracts
the overlay images, as `os-dubna/algol/Makefile` does. As in `os-dubna/tools/ovltrace.py`, a
traced instruction counts only if it matches the image at its address: other overlays (the
monitor, WRITLIB) run at the same addresses.

## Order of modules (Hello, World)

```
module        insns  stays  entered from
G22162          185     20  ALGOL G27126
ALGOL           601     29  G22162 ASMPRINT G13666 G27645 G17223 G23523 G26260 G27733 G30257 G26666 D01736
ASMPRINT      22707     14  ALGOL
G13666          160     18  ALGOL G27645 ASMPRINT G27126 G14504 G27220
G27645          708     16  G13666 G14504 G30001 ALGOL G30257
G14504         1971     22  G13666 G27645 G27220 G27126
G27126         1727    141  G14504 G17223 G22162 G23523 G26260 G26666
G30001         3318     20  G27645 G17223 G26260 G30257
G27220           41      4  G14504 G13666
G17223         3933     28  ALGOL G27126 G26666 G30001
G26666          804     64  G17223 ALGOL G27126 G30257
G23523         1105     29  ALGOL G27126 G30034
G30034          604      4  G23523
G26260         2299     37  ALGOL ASMPRINT G30001 G30120 G27126 G27733
G30120          367      1  G26260
G27733         2402      4  G26260 ALGOL G30257
G30257           54      8  G26666 G30001 G27645 G27733 ASMPRINT
D01736           54      2  ASMPRINT ALGOL
```

The driver `ALGOL` runs the passes G13666 (with G14504, line by line), G17223, G22162, G23523,
G26260, G26666 and G30257. G27126, G27645, G30001 and G30034 are helpers called by several
passes. ASMPRINT prints the listing. With `*full`, `A/PRINTM` and its modules run at the end.

## Coverage of the corpus

All of `tests/algol/*.alg` with `*full`. The totals count every half word of a module, data
included, so the real code coverage is higher. Modules with low coverage (G30120, G31375,
G14504, G30034) are where new test programs help most.

```
ALGOL     01614-01735   124 /   164   75%
G13666    13666-14503   531 /   796   66%
G14504    14504-17222  1072 /  2718   39%
G17223    17223-22161  1666 /  3006   55%
G22162    22162-23522   976 /  1474   66%
G23523    23523-26257  1757 /  2746   63%
G26260    26260-26665   335 /   524   63%
G26666    26666-27125   209 /   320   65%
G27126    27126-27217   106 /   116   91%
G27220    27220-27244    17 /    42   40%
G27645    27645-27732    61 /   108   56%
G27733    27733-30000    71 /    76   93%
G30001    30001-30033    42 /    54   77%
G30034    30034-30117    40 /   104   38%
G30120    30120-30256     7 /   190    3%
G30257    30257-30354    54 /   124   43%
A/PRINTM  30365-31205   460 /   802   57%
G31206    31206-31253    71 /    76   93%
G31254    31254-31306    46 /    54   85%
G31307    31307-31340    46 /    52   88%
G31341    31341-31374    44 /    56   78%
G31375    31375-31420     0 /    40    0%
G31421    31421-31645    97 /   298   32%
```
