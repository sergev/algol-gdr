#!/usr/bin/env python3
# Copyright (c) 2026 Serge Vakulenko. MIT license.
"""disasm-overlay.py - disassemble overlay ALGOL with its tables resolved.

    disasm-overlay.py [--m7 BASE] [--tape MONSYS9] FIRST-LAST...

For each word in the octal ranges: the address, the word, its two halves as
instructions, and the ISO text of the word. A label from os-dubna/algol/agNNNNN.asm
(*nnnnB or AGnnnnn) is shown before its word. With --m7, operands through register 7
of the short instructions are resolved to absolute addresses, followed by the word there.
"""
import argparse
import glob
import os
import re

BASE = 0o1604
OVERLAY = (0o070 * 1024 + 0o1100) * 6

SHORT = ('atx stx mod xts a+x a-x x-a amx xta aax aex arx avx aox a/x a*x '
         'apx aux acx anx e+x e-x asx xtr rte yta *32 ext e+n e-n asn ntr '
         'ati sti ita its mtj j+m *46 *47').split()
LONG = '*20 *21 utc wtc vtm utm uza u1a uj vjm ij stop vzm v1m *36 vlm'.split()


def ovl2bin(data):
    def word(i):
        return int.from_bytes(data[i:i + 6], 'big')
    out, i = [], 0
    while True:
        h = word(i)
        i += 6
        flag, rep, cnt = h >> 47, (h >> 15) & 0o77777, h & 0o77777
        if rep == 0 and cnt == 0:
            return out
        fill = 0
        if flag and rep:
            fill = word(i)
            i += 6
        for _ in range(cnt):
            out.append(word(i))
            i += 6
        out += [fill] * rep


def iso(w):
    return ''.join(chr(c) if 32 <= c < 127 else '.' for c in w.to_bytes(6, 'big'))


def labels(asmdir):
    names = {}
    for path in glob.glob(os.path.join(asmdir, 'ag*.asm')):
        start = int(os.path.basename(path)[2:7], 8)
        for line in open(path, errors='replace'):
            m = re.match(r'^ (\*([0-7]{4})B|A[GDE]([0-7]{5})):', line)
            if m:
                a = start + int(m.group(2), 8) if m.group(2) else int(m.group(3), 8)
                names.setdefault(a, []).append(m.group(1))
    return names


def insn(h, m7, img):
    reg = (h >> 20) & 0o17
    short = not h & (1 << 19)
    if not short:
        op, addr = LONG[(h >> 15) & 0o17], h & 0o77777
    else:
        code = (h >> 12) & 0o77
        op = SHORT[code] if code < 0o50 else f'*{code:o}'
        addr = (h & 0o7777) | (0o70000 if h & (1 << 18) else 0)
    text = f'{reg:2} {op:4} {addr:05o}' if reg else f'   {op:4} {addr:05o}'
    if reg == 7 and short and m7 is not None:
        ea = (m7 + addr) & 0o77777
        text += f' ={ea:05o}'
        if BASE <= ea < BASE + len(img):
            text += f' [{img[ea - BASE]:016o}]'
    return text


def main():
    here = os.path.dirname(os.path.realpath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument('--m7', type=lambda s: int(s, 8))
    ap.add_argument('--tape', default=os.path.join(here, '../../os-dubna/monsys/monsys.9'))
    ap.add_argument('--asm', default=os.path.join(here, '../../os-dubna/algol'))
    ap.add_argument('ranges', nargs='+')
    args = ap.parse_args()
    with open(args.tape, 'rb') as f:
        f.seek(OVERLAY)
        img = ovl2bin(f.read())
    names = labels(args.asm)
    for r in args.ranges:
        lo, hi = (int(x, 8) for x in r.split('-'))
        for a in range(lo, hi + 1):
            w = img[a - BASE]
            for n in names.get(a, []):
                print(f'{n}:')
            left = insn(w >> 24, args.m7, img)
            right = insn(w & 0o77777777, args.m7, img)
            print(f'{a:05o} {w:016o}  {left:38} | {right:38} {iso(w)}')


main()
