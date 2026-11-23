#!/usr/bin/env python3
"""gen-pass.py - translate code of overlay ALGOL into C++, one call per instruction.

    gen-pass.py [--tape MONSYS9] [--hook ADDR]... NAME FIRST-LAST... > src/NAME_gen.cpp

Emits `unsigned NAME_code(Machine &m, unsigned pc)`, which runs the words of the
octal ranges from address pc and returns the address of the first jump out of them.
Every word is translated, data included; jumps go to labels, computed ones through a
switch. A --hook address calls m.on_entry when the code reaches it. Semantics of
the instructions: src/machine.h.
"""
import argparse
import importlib.util
import os

here = os.path.dirname(os.path.realpath(__file__))
spec = importlib.util.spec_from_file_location('gen_tables', os.path.join(here, 'gen-tables.py'))

SIMPLE = {0o00: 'atx', 0o01: 'stx', 0o03: 'xts', 0o10: 'xta', 0o11: 'aax', 0o12: 'aex',
          0o13: 'arx', 0o15: 'aox', 0o20: 'apx', 0o21: 'aux', 0o22: 'acx', 0o23: 'anx',
          0o26: 'asx', 0o27: 'xtr', 0o30: 'rte', 0o31: 'yta', 0o36: 'asn', 0o37: 'ntr',
          0o40: 'ati', 0o41: 'sti', 0o42: 'ita', 0o43: 'its', 0o44: 'mtj', 0o45: 'jam',
          0o220: 'utc', 0o230: 'wtc', 0o240: 'vtm', 0o250: 'utm'}
ARITH = {0o04, 0o05, 0o06, 0o07, 0o14, 0o16, 0o17, 0o24, 0o25, 0o34, 0o35}
ILLEGAL = {0o02, 0o32, 0o33, 0o46, 0o47, 0o320}
NAMES = ('atx stx mod xts a+x a-x x-a amx xta aax aex arx avx aox a/x a*x apx aux acx '
         'anx e+x e-x asx xtr rte yta *32 *33 e+n e-n asn ntr ati sti ita its mtj j+m '
         '*46 *47').split()
LONG = '*20 *21 utc wtc vtm utm uza u1a uj vjm *32 stop vzm v1m *36 vlm'.split()


def decode(rk):
    reg = rk >> 20
    if rk & (1 << 19):
        return reg, (rk >> 12) & 0o370, rk & 0o77777
    addr = rk & 0o7777
    if rk & (1 << 18):
        addr |= 0o70000
    return reg, (rk >> 12) & 0o77, addr


def mnemonic(op):
    if op >= 0o200:
        return LONG[(op >> 3) - 0o20]
    return NAMES[op] if op < 0o50 else f'*{op:02o}'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tape', default=os.path.join(here, '../../os-dubna/monsys/monsys.9'))
    ap.add_argument('--hook', action='append', default=[])
    ap.add_argument('name')
    ap.add_argument('ranges', nargs='+')
    args = ap.parse_args()

    gt = importlib.util.module_from_spec(spec)
    gt.__name__ = 'gen_tables'
    src = open(spec.origin).read().replace('\nmain()\n', '\n')
    exec(compile(src, spec.origin, 'exec'), gt.__dict__)
    with open(args.tape, 'rb') as f:
        f.seek(gt.OVERLAY)
        img = gt.ovl2bin(f.read())

    ranges = [tuple(int(x, 8) for x in r.split('-')) for r in args.ranges]
    words = sorted({a for lo, hi in ranges for a in range(lo, hi + 1)})
    inside = set(words)
    hooks = {int(h, 8) for h in args.hook}

    def half(a, right):
        w = img[a - gt.BASE]
        return (w if right else w >> 24) & 0o77777777

    out = []
    p = out.append

    def jump(target, cond, static):
        # A jump to `target` (a C++ expression, or an int when static) if cond.
        if static and target in inside:
            go = f'goto L{target:05o};'
        else:
            t = f'0{target:05o}' if static else target
            go = f'{{ pc = {t}; goto dispatch; }}'
        if cond:
            p(f'    if ({cond}) {go}')
        else:
            p(f'    {go}')

    prev_mod = False
    for a in words:
        if a - 1 not in inside:
            prev_mod = False
        p(f'L{a:05o}:')
        if a in hooks:
            p(f'    if (m.on_entry) m.on_entry(m, 0{a:05o});')
        for right in (False, True):
            reg, op, addr = decode(half(a, right))
            p(f'    // {a:05o}{"R" if right else "L"} {reg:2} {mnemonic(op)} {addr:05o}')
            p(f'    if (m.trace) m.trace(m, 0{a:05o}, {int(right)});')
            nxt = (a + 1) & 0o77777
            static = reg == 0 and not prev_mod
            ad = f'0{addr:05o}'
            if op in SIMPLE:
                p(f'    m.{SIMPLE[op]}({reg}, {ad});')
            elif op in ARITH:
                p(f'    m.arith({reg}, {ad}, 0{op:03o});')
            elif op in ILLEGAL:
                p(f'    m.illegal(0{a:05o});')
            elif 0o50 <= op <= 0o77 or op in (0o200, 0o210):
                p(f'    m.extracode(0{op:03o}, {reg}, {ad});')
            elif op in (0o260, 0o270, 0o300):
                cond = {0o260: 'm.uza()', 0o270: 'm.u1a()', 0o300: None}[op]
                if static:
                    jump(addr, cond, True)
                else:
                    p('    {')
                    p(f'    unsigned t = m.ea({reg}, m.field({ad}));')
                    jump('t', cond, False)
                    p('    }')
            elif op in (0o310, 0o340, 0o350, 0o360, 0o370):
                cond = {0o310: None, 0o340: f'!m.M[{reg}]', 0o350: f'm.M[{reg}]',
                        0o360: f'!m.M[{reg}]', 0o370: f'm.vlm({reg})'}[op]
                if op == 0o310:
                    p(f'    m.set_m({reg}, 0{nxt:05o});')
                if not prev_mod:
                    jump(addr, cond, True)
                else:
                    p('    {')
                    p(f'    unsigned t = m.field({ad});')
                    jump('t', cond, False)
                    p('    }')
            elif op == 0o330:
                p(f'    throw std::runtime_error("stop at {a:05o}");')
            else:
                raise SystemExit(f'{a:05o}: unknown opcode {op:o}')
            prev_mod = op in (0o220, 0o230)
        if a + 1 not in inside:
            jump(a + 1, None, True)

    print('// Generated by scripts/gen-pass.py from overlay ALGOL of monsys.9. Do not edit.')
    print(f'// {args.name}: {" ".join(args.ranges)}')
    print('// clang-format off')
    print('#include <stdexcept>')
    print()
    print('#include "machine.h"')
    print()
    print(f'unsigned {args.name}_code(Machine &m, unsigned pc)')
    print('{')
    print('    goto dispatch;')
    for line in out:
        print(line)
    print('dispatch:')
    print('    switch (pc) {')
    for a in words:
        print(f'    case 0{a:05o}: goto L{a:05o};')
    print('    default: return pc;')
    print('    }')
    print('}')


main()
