#!/usr/bin/env python3
"""capture-streams.py - pass streams and data from a dubna -d imer trace of Algol-GDR.

    capture-streams.py TRACE OUTBASE [TABLES_DATA]

Writes OUTBASE.sN (stream written by pass N, one item per line in octal, in memory
order: a stream written backward is reversed) and OUTBASE.mem (per pass, the words
it changed: workspace lines `ADDR VALUE`, drum lines `drum UNIT ZONE OFFSET VALUE`,
and the index registers it left for the next pass: `reg N VALUE`), and OUTBASE.lex
(at each return of the lexer AG15414, a UJ through M12 in 15414-16471: the symbol
word 02000 and register M4, in octal).

Passes start at the driver's calls (01625 … 01634). Items are the A register at the
entries of G27126 (27114, 27126 halves; 27137 words), taken from the push at their
first instruction.

Memory and the scratch drums are shadowed from the start of pass 1: memory starts
as the data set by the driver (TABLES_DATA, default src/tables_data.cpp), then
follows the traced writes and drum transfers. Every traced read is checked
against the shadow; mismatches are reported on stderr.
"""
import os
import re
import sys

PASSES = {'13666': 1, '17223': 2, '22162': 3, '23523': 4, '26260': 5, '26666': 6}
RETURNS = {1: '01626', 2: '01627', 3: '01632', 4: '01633', 5: '01634', 6: '01635'}
CALL = re.compile(r'^016[23][0-7] [LR]: 15 31 (\d{5}) vjm')
WRITER = re.compile(r'^27(114|126|137) L: 00 003 2001 xts 2001')
MEM = re.compile(r'^\s+Memory (Read|Write) \[([0-7]{5})\] = ([0-7 ]{19})')
DRUM = re.compile(r'^\s+Drum ([0-7]+) (Read|Write) \[([0-7]{5})-([0-7]{5})\] = '
                  r'Zone ([0-7]+)(?: Sector ([0-7]))?')
REG = re.compile(r'^\s+M([0-7]+) = ([0-7]{5})$')
LEXRET = re.compile(r'^1(5[4-7]|6[0-4])[0-7]{2} [LR]: 14 30 00000 uj')
INSN = re.compile(r'^[0-7]{5} [LR]: ')
INIT = re.compile(r'^    \{ 0([0-7]+), 0([0-7]+) \},')
RANGES = [(0o1736, 0o12000), (0o27245, 0o27645)]
DRUMS = {'26', '27'}     # scratch drums: data and streams between passes


def in_ranges(a):
    return any(lo <= a < hi for lo, hi in RANGES)


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    trace, out = sys.argv[1], sys.argv[2]
    data = sys.argv[3] if len(sys.argv) > 3 else \
        os.path.join(os.path.dirname(os.path.realpath(__file__)), '../src/tables_data.cpp')
    init = {}
    for line in open(data):
        m = INIT.match(line)
        if m:
            init[int(m.group(1), 8)] = int(m.group(2), 8)

    npass = 0
    streams = {}            # pass -> [(entry, value)]
    mem = dict(init)        # shadow memory
    drum = {}               # (unit, zone, offset) -> value
    start = None            # (mem, drum) at the start of the current pass
    dumps = []              # (pass, mem changes, drum changes, registers)
    regs = {}               # index registers
    mismatches = 0
    pending = None          # writer entry waiting for its push
    lex = []                # (02000, M4) at each lexer return

    def end_pass(final=True):
        dm = {a: v for a, v in mem.items() if in_ranges(a) and start[0].get(a, 0) != v}
        dd = {k: v for k, v in drum.items()
              if k[0] in DRUMS and start[1].get(k, 0) != v}
        dumps.append((npass, dm, dd, dict(regs) if final else {}))

    for line in open(trace, errors='replace'):
        if INSN.match(line):
            pending = None
            m = CALL.match(line)
            if m and m.group(1) in PASSES:
                if npass:
                    end_pass()
                npass = PASSES[m.group(1)]
                start = (dict(mem), dict(drum))
                continue
            if npass and line[:7] in (RETURNS[npass] + ' L', '01716 L'):
                # Back in the driver, or its end: the pass is over.
                end_pass()
                npass = 0
                continue
            if npass == 1 and LEXRET.match(line) and int(line[:5], 8) <= 0o16471:
                lex.append((mem.get(0o2000, 0), regs.get(4, 0)))
                continue
            m = WRITER.match(line)
            if m and npass:
                pending = m.group(1)
            continue
        m = MEM.match(line)
        if m:
            a = int(m.group(2), 8)
            v = int(m.group(3).replace(' ', ''), 8)
            if pending and m.group(1) == 'Write':
                streams.setdefault(npass, []).append((pending, v))
                pending = None
            if m.group(1) == 'Read' and npass and in_ranges(a) and mem.get(a, 0) != v:
                mismatches += 1
            mem[a] = v
            continue
        m = REG.match(line)
        if m:
            regs[int(m.group(1), 8)] = int(m.group(2), 8)
            continue
        m = DRUM.match(line)
        if m:
            unit, op = m.group(1), m.group(2)
            lo, hi = int(m.group(3), 8), int(m.group(4), 8)
            zone = int(m.group(5), 8)
            base = int(m.group(6) or '0', 8) * 256
            for i in range(hi - lo + 1):
                k = (unit, zone, base + i)
                if op == 'Write':
                    drum[k] = mem.get(lo + i, 0)
                else:
                    mem[lo + i] = drum.get(k, 0)
    if npass:
        end_pass(False)
    if mismatches:
        print(f'{out}: {mismatches} reads differ from the shadow memory', file=sys.stderr)

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
    with open(f'{out}.lex', 'w') as f:
        f.writelines(f'{w:016o} {r:05o}\n' for w, r in lex)
    with open(f'{out}.mem', 'w') as f:
        for n, dm, dd, rr in dumps:
            f.write(f'pass {n}\n')
            for r in sorted(rr):
                if 0 < r < 0o17:
                    f.write(f'reg {r:o} {rr[r]:05o}\n')
            for a in sorted(dm):
                f.write(f'{a:05o} {dm[a]:016o}\n')
            for unit, zone, off in sorted(dd):
                f.write(f'drum {unit} {zone:o} {off:04o} {dd[unit, zone, off]:016o}\n')


main()
