_comment Algol-GDR input and output: _write and _read with a format on a
unit, _rewind, _backspace, _endfile, _encode, _decode. Unit 7 is a file,
54 the terminal;
_begin _integer i, n; _real x; _string s; _boolean b; _integer _array a[1:3];
    _write (7, `(I5, F8.2, L2)') 42, 3.25, _true;
    _write (7, `(3I4)') 1, 2, 3;
    _rewind (7);
    _read (7, `(I5, F8.2, L2)') n, x, b;
    _read (7, `(3I4)') a;
    print(n, x, b, a[1], a[2], a[3], NEWLINE);

    _comment The last record again;
    _backspace (7);
    _read (7, `(4X, I4)') i;
    print(i, NEWLINE);

    _encode (12, `(I4, A5)', s) 77, `abc';
    print(s, `|', NEWLINE);
    _decode (40, `(I4)', s) i;
    print(i, NEWLINE);

    _comment Without a point the last d digits are the fraction;
    _decode (6, `(F6.2)', ` 12345') x;
    print(x, NEWLINE);

    _comment The file ends after the first record;
    _rewind (7); _write (7, `(I3)') 9; _endfile (7); _rewind (7);
    _read (7, `(A)') s;
    print(s, `|', NEWLINE);

    _read (54, `(2I3)') i, n;
    print(i, n, NEWLINE)
_end
