#!/usr/bin/env python3
"""trace-diff.py - first difference between the original and a translated pass.

    dubna -r -d ir --trace=DUBNA.tr job.dub
    ALGOL_TRACE=MINE.tr build/src/algol-gdr --stop-after=N X.alg
    trace-diff.py [--entry 17223] [--ret 01627] DUBNA.tr MINE.tr

Compares the addresses of the instructions run by the pass, and A where dubna
shows it changed. Prints the first place where the control flow differs, with
the last instructions and the first differences of A before it (in 30001 they
are harmless: it does not clear its result cell).
"""
import argparse
import re

INSN = re.compile(r'^([0-7]{5}) ([LR]): ')
ACC = re.compile(r'^\s+ACC = ([0-7 ]+)$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--entry', default='17223')
    ap.add_argument('--ret', default='01627')
    ap.add_argument('dubna')
    ap.add_argument('mine')
    args = ap.parse_args()

    ref, acc, on, changed = [], 0, False, False
    for line in open(args.dubna, errors='replace'):
        m = INSN.match(line)
        if m:
            if not on:
                on = f' {args.entry} vjm' in line
                continue
            if m.group(1) == args.ret:
                break
            ref.append((m.group(1), m.group(2), acc if changed else None))
            changed = False
            continue
        m = ACC.match(line)
        if m and on:
            acc, changed = int(m.group(1).replace(' ', ''), 8), True
    mine = [tuple(line.split()) for line in open(args.mine)]

    accs = []
    for i, (x, y) in enumerate(zip(ref, mine)):
        if x[:2] != y[:2]:
            print(f'control differs at instruction {i}: original {x[0]} {x[1]}, '
                  f'translated {y[0]} {y[1]}')
            for k in range(max(0, i - 12), i + 1):
                a = '-' if ref[k][2] is None else f'{ref[k][2]:016o}'
                print(f'    {ref[k][0]} {ref[k][1]} {a} | {" ".join(mine[k])}')
            print('first differences of A:', accs[:8])
            return
        if x[2] is not None and x[2] != int(y[2], 8):
            accs.append((i, x[0] + x[1], f'{x[2]:016o}', y[2]))
    print(f'same control flow for {min(len(ref), len(mine))} instructions '
          f'({len(ref)} original, {len(mine)} translated); A differs {len(accs)} times',
          accs[:3])


main()
