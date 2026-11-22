#!/usr/bin/env python3
"""trace-passes.py - which modules of ALGOL (and A/PRINTM) a dubna trace ran.

    trace-passes.py [--sequence | --coverage] OVERLAY.layout.py IMAGE.bin [...] -- TRACE...

Each overlay is a pair: its layout file (os-dubna/algol/OV.layout.py, with BASE and
LAYOUT) and its image (OV.gold.bin, from the tape). A traced instruction counts
only if it is the half word of the image at that address, as in ovltrace.py.

Prints per module, in the order of first use: instructions executed, number of
stays (entries from another module), and the modules execution came from.
With --sequence: every stay in order, with its number of instructions.
With --coverage: per module, half words executed / half words of code.
"""
import re
import sys

LINE = re.compile(r'^([0-7]{5}) ([LR]): ([0-7]{2}) ([0-7]{2,3}) ([0-7]{4,5})')


def load_overlay(layout_path, image_path):
    env = {}
    exec(open(layout_path).read(), env)
    base = env['BASE']
    d = open(image_path, 'rb').read()
    img = [int.from_bytes(d[i:i + 6], 'big') for i in range(0, len(d) - 5, 6)]
    rename = env.get('RENAME', {})
    items = sorted(env['LAYOUT'])
    mods = []
    for k, (addr, kind, name) in enumerate(items):
        end = items[k + 1][0] if k + 1 < len(items) else env['END']
        mods.append((addr, end, kind, rename.get(name, name)))
    return base, img, mods


def module_of(overlays, a):
    for base, img, mods in overlays:
        if base <= a < base + len(img):
            for start, end, kind, name in mods:
                if start <= a < end:
                    return base, img, start, end, kind, name
    return None


def main():
    args = sys.argv[1:]
    mode = 'summary'
    if args and args[0] in ('--coverage', '--sequence'):
        mode = args[0][2:]
        args = args[1:]
    if '--' not in args:
        sys.exit(__doc__)
    k = args.index('--')
    pairs, traces = args[:k], args[k + 1:]
    if len(pairs) % 2 or not traces:
        sys.exit(__doc__)
    overlays = [load_overlay(pairs[i], pairs[i + 1]) for i in range(0, len(pairs), 2)]

    seen = set()
    seq = []                    # [name, count]
    for path in traces:
        for line in open(path, errors='replace'):
            m = LINE.match(line)
            if not m:
                continue
            a = int(m.group(1), 8)
            hit = module_of(overlays, a)
            if not hit:
                continue
            base, img, start, end, kind, name = hit
            reg, op, addr = m.group(3), m.group(4), m.group(5)
            if len(op) == 2:
                h = int(reg, 8) << 20 | int(op, 8) << 15 | int(addr, 8)
            else:
                h = int(reg, 8) << 20 | int(op, 8) << 12 | int(addr, 8)
            w = img[a - base]
            if h != (w >> 24 if m.group(2) == 'L' else w & 0o77777777):
                continue
            seen.add((a, m.group(2)))
            if seq and seq[-1][0] == name:
                seq[-1][1] += 1
            else:
                seq.append([name, 1])

    if mode == 'coverage':
        for base, img, mods in overlays:
            for start, end, kind, name in mods:
                if kind != 'M':
                    continue
                n = sum(1 for (a, s) in seen if start <= a < end)
                total = 2 * (end - start)
                print('%-9s %05o-%05o %5d / %5d  %3d%%' %
                      (name, start, end - 1, n, total, 100 * n // total))
        return

    if mode == 'sequence':
        for name, n in seq:
            print('%-9s %d' % (name, n))
        return

    order, total, stays, callers = [], {}, {}, {}
    prev = None
    for name, n in seq:
        if name not in total:
            order.append(name)
            total[name], stays[name], callers[name] = 0, 0, []
        total[name] += n
        stays[name] += 1
        if prev and prev not in callers[name]:
            callers[name].append(prev)
        prev = name
    print('%-9s %9s %6s  %s' % ('module', 'insns', 'stays', 'entered from'))
    for name in order:
        print('%-9s %9d %6d  %s' % (name, total[name], stays[name], ' '.join(callers[name])))


main()
