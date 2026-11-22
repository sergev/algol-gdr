#!/usr/bin/env python3
"""capture-streams.py - pass streams and memory from a dubna -d im trace of Algol-GDR.

    capture-streams.py TRACE OUTBASE [TABLES_DATA]

Writes OUTBASE.sN (stream written by pass N, one item per line in octal, in memory
order: a stream written backward is reversed) and OUTBASE.mem (per pass, the words
of the workspace it wrote, with their values at its end; a zero only where the
word was nonzero before the pass: seen so, or set so in TABLES_DATA, default
src/tables_data.cpp).

Passes start at the driver's calls (01625 … 01634). Items are the A register at the
entries of G27126 (27114, 27126 halves; 27137 words), taken from the push at their
first instruction. Drum reads into the workspace are not in the trace.
"""
import os
import re
import sys

PASSES = {'13666': 1, '17223': 2, '22162': 3, '23523': 4, '26260': 5, '26666': 6}
CALL = re.compile(r'^016[23][0-7] [LR]: 15 31 (\d{5}) vjm')
WRITER = re.compile(r'^27(114|126|137) L: 00 003 2001 xts 2001')
MEM = re.compile(r'^\s+Memory (Read|Write) \[([0-7]{5})\] = ([0-7 ]{19})')
INSN = re.compile(r'^[0-7]{5} [LR]: ')
INIT = re.compile(r'^    \{ 0([0-7]+), 0([0-7]+) \},')
RANGES = [(0o2000, 0o6000), (0o27245, 0o27645)]


def in_ranges(a):
    return any(lo <= a < hi for lo, hi in RANGES)


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    trace, out = sys.argv[1], sys.argv[2]
    data = sys.argv[3] if len(sys.argv) > 3 else \
        os.path.join(os.path.dirname(os.path.realpath(__file__)), '../src/tables_data.cpp')
    npass = 0
    streams = {}            # pass -> [(entry, value)]
    mem = {}                # words written by the current pass
    known = {}              # last value seen of each word
    for line in open(data):
        m = INIT.match(line)
        if m:
            known[int(m.group(1), 8)] = int(m.group(2), 8)
    before = {}             # known values at the start of the current pass
    dumps = []              # (pass, written, before)
    pending = None          # writer entry waiting for its push
    for line in open(trace, errors='replace'):
        if INSN.match(line):
            pending = None
            m = CALL.match(line)
            if m and m.group(1) in PASSES:
                if npass:
                    dumps.append((npass, mem, before))
                    mem = {}
                before = dict(known)
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
        elif in_ranges(a):
            known[a] = v
            if m.group(1) == 'Write':
                mem[a] = v
    if npass:
        dumps.append((npass, mem, before))

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
        for n, d, old in dumps:
            f.write(f'pass {n}\n')
            for a in sorted(d):
                if d[a] or old.get(a):
                    f.write(f'{a:05o} {d[a]:016o}\n')


main()
