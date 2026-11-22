#!/usr/bin/env python3
"""capture-streams.py - pass streams and memory from a dubna -d im trace of Algol-GDR.

    capture-streams.py TRACE OUTBASE

Writes OUTBASE.sN (stream written by pass N, one item per line in octal, in memory
order: a stream written backward is reversed) and OUTBASE.mem (per pass, the words
of the workspace it wrote, with their values at its end; zeros left out).

Passes start at the driver's calls (01625 … 01634). Items are the A register at the
entries of G27126 (27114, 27126 halves; 27137 words), taken from the push at their
first instruction. Drum reads into the workspace are not in the trace.
"""
import re
import sys

PASSES = {'13666': 1, '17223': 2, '22162': 3, '23523': 4, '26260': 5, '26666': 6}
CALL = re.compile(r'^016[23][0-7] [LR]: 15 31 (\d{5}) vjm')
WRITER = re.compile(r'^27(114|126|137) L: 00 003 2001 xts 2001')
MEM = re.compile(r'^\s+Memory (Read|Write) \[([0-7]{5})\] = ([0-7 ]{19})')
INSN = re.compile(r'^[0-7]{5} [LR]: ')
RANGES = [(0o2000, 0o6000), (0o27245, 0o27645)]


def in_ranges(a):
    return any(lo <= a < hi for lo, hi in RANGES)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    trace, out = sys.argv[1], sys.argv[2]
    npass = 0
    streams = {}            # pass -> [(entry, value)]
    mem = {}                # words written by the current pass
    dumps = []              # (pass, dict)
    pending = None          # writer entry waiting for its push
    for line in open(trace, errors='replace'):
        if INSN.match(line):
            pending = None
            m = CALL.match(line)
            if m and m.group(1) in PASSES:
                if npass:
                    dumps.append((npass, mem))
                    mem = {}
                npass = PASSES[m.group(1)]
                continue
            m = WRITER.match(line)
            if m and npass:
                pending = m.group(1)
            continue
        m = MEM.match(line)
        if not m:
            continue
        a = int(m.group(2), 8)
        v = int(m.group(3).replace(' ', ''), 8)
        if pending and m.group(1) == 'Write':
            streams.setdefault(npass, []).append((pending, v))
            pending = None
        elif m.group(1) == 'Write' and in_ranges(a):
            mem[a] = v
    if npass:
        dumps.append((npass, mem))

    for n, items in streams.items():
        if n == 6:
            continue                # the object: see NAME.std
        entries = {e for e, v in items}
        if entries - {'114', '126'}:
            sys.exit(f'pass {n}: unexpected writers {sorted(entries)}')
        if len(entries) > 1:
            sys.exit(f'pass {n}: written both forward and backward')
        vals = [v & 0o77777777 for e, v in items]
        if entries == {'114'}:
            vals.reverse()
        with open(f'{out}.s{n}', 'w') as f:
            f.writelines(f'{v:08o}\n' for v in vals)
    with open(f'{out}.mem', 'w') as f:
        for n, d in dumps:
            f.write(f'pass {n}\n')
            for a in sorted(d):
                if d[a]:
                    f.write(f'{a:05o} {d[a]:016o}\n')


main()
