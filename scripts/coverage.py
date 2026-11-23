#!/usr/bin/env python3
"""coverage.py - code of the translated passes never run by the test programs.

    coverage.py [--compiler BIN] [--asm DIR] [--all] [ALG...]

Compiles each program (default tests/algol/*.alg, with the options of its .pre)
with ALGOL_COVERAGE set, then prints per pass the halves of code run, and the
code never run: ranges of words, each with the label before it in the Madlen
source (os-dubna/algol). Code is what algol.sym/.tsym mark C, and whatever ran,
but not a word of six printable characters (message texts are marked C too).
A word run in one half only is listed with --all. Routines replaced by C++
(the replaced ranges and natives of src/*_gen.cpp) are not counted.
"""
import argparse
import glob
import importlib.util
import os
import re
import subprocess
import tempfile

here = os.path.dirname(os.path.realpath(__file__))
root = os.path.dirname(here)

PASSES = [
    ('shared', [(0o01644, 0o01735), (0o27114, 0o27244), (0o27733, 0o30000),
                (0o30001, 0o30033)]),
]


def code_words(asm):
    words = set()
    for name in ('algol.sym', 'algol.tsym'):
        for line in open(os.path.join(asm, name)):
            f = line.split()
            if len(f) >= 2 and f[1] == 'C':
                lo, hi = (int(x, 8) for x in f[0].split('-'))
                words.update(range(lo, hi + 1))
    return words


def labels(asm):
    names = {}
    for path in glob.glob(os.path.join(asm, 'ag*.asm')):
        start = int(os.path.basename(path)[2:7], 8)
        for line in open(path, errors='replace'):
            m = re.match(r'^ (\*([0-7]{4})B|A[GDE]([0-7]{5})):', line)
            if m:
                a = start + int(m.group(2), 8) if m.group(2) else int(m.group(3), 8)
                names.setdefault(a, m.group(1))
    return names


def image(tape):
    spec = importlib.util.spec_from_file_location('gen_tables', os.path.join(here, 'gen-tables.py'))
    gt = importlib.util.module_from_spec(spec)
    src = open(spec.origin).read().replace('\nmain()\n', '\n')
    exec(compile(src, spec.origin, 'exec'), gt.__dict__)
    with open(tape, 'rb') as f:
        f.seek(gt.OVERLAY)
        img = gt.ovl2bin(f.read())
    return lambda a: img[a - gt.BASE]


def is_text(w):
    return all(0o40 <= (w >> s) & 0o377 < 0o177 for s in range(0, 48, 8))


def pre_options(alg):
    opts = []
    pre = alg[:-4] + '.pre'
    if os.path.exists(pre):
        for line in open(pre):
            if line.startswith('*call setftn:'):
                opts.append('--setftn=' + line[13:].strip())
            elif line.strip() == '*check':
                opts.append('--check')
    return opts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--compiler', default=os.path.join(root, 'build/src/algol-gdr'))
    ap.add_argument('--asm', default=os.path.join(root, '../os-dubna/algol'))
    ap.add_argument('--tape', default=os.path.join(root, '../os-dubna/monsys/monsys.9'))
    ap.add_argument('--all', action='store_true')
    ap.add_argument('alg', nargs='*')
    args = ap.parse_args()
    progs = args.alg or sorted(glob.glob(os.path.join(root, 'tests/algol/*.alg')))

    run = set()
    with tempfile.TemporaryDirectory() as tmp:
        cov = os.path.join(tmp, 'cov')
        env = dict(os.environ, ALGOL_COVERAGE=cov)
        for alg in progs:
            subprocess.run([args.compiler, *pre_options(alg), '-o', os.path.join(tmp, 'x.std'),
                            alg], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if os.path.exists(cov):
            for line in open(cov):
                a, h = line.split()
                run.add((int(a, 8), h == 'R'))

    word = image(args.tape)
    code = {a for a in code_words(args.asm) if not is_text(word(a))} | {a for a, _ in run}
    names = labels(args.asm)
    natives = set()
    for path in glob.glob(os.path.join(root, 'src/*_gen.cpp')):
        for line in open(path):
            if line.startswith('// natives:'):
                natives.update(int(a, 8) for a in line.split()[2:])
            if line.startswith('// replaced:'):
                for r in line.split()[2:]:
                    lo, hi = (int(x, 8) for x in r.split('-'))
                    code -= set(range(lo, hi + 1))
    for a in natives:
        # the routine: up to the next label that is not a local loop (crude)
        b = a
        while b + 1 in code and (b + 1, False) not in run and (b + 1, True) not in run:
            b += 1
        code -= set(range(a, b + 1))
    for title, ranges in PASSES:
        words = [a for lo, hi in ranges for a in range(lo, hi + 1) if a in code]
        done = sum((a, r) in run for a in words for r in (False, True))
        print(f'{title}: {done} of {2 * len(words)} halves run '
              f'({100 * done // max(1, 2 * len(words))}%)')
        unrun = [a for a in words if not (a, False) in run and not (a, True) in run]
        if args.all:
            unrun = [a for a in words if not ((a, False) in run and (a, True) in run)]
        group = []
        for a in unrun + [None]:
            if group and (a is None or a != group[-1] + 1):
                lo, hi = group[0], group[-1]
                lab = next((f'{names[b]}' + (f'+{lo - b:o}' if lo != b else '')
                            for b in range(lo, lo - 0o2000, -1) if b in names), '')
                span = f'{lo:05o}' if lo == hi else f'{lo:05o}-{hi:05o}'
                print(f'  {span:12} {hi - lo + 1:4}  {lab}')
                group = []
            if a is not None:
                group.append(a)


main()
