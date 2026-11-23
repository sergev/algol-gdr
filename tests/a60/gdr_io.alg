_comment Algol-GDR input and output: _write and _read with a format on a
unit, _rewind, _backspace, _endfile, _encode, _decode. Unit 7 is a file,
54 the terminal;
_begin _integer i, n; _real x; _string s; _boolean b; _integer _array a[1:3];
    _write (7, ''(I5, F8.2, L2)'') 42, 3.25, _true;
    _write (7, ''(3I4)'') 1, 2, 3;
    _rewind (7);
    _read (7, ''(I5, F8.2, L2)'') n, x, b;
    _read (7, ''(3I4)'') a;
    PRINT(n, x, b, a[1], a[2], a[3]);

    _comment The last record again;
    _backspace (7);
    _read (7, ''(4X, I4)'') i;
    PRINT(NEWLINE, i);

    _encode (12, ''(I4, A5)'', s) 77, ''abc'';
    PRINT(NEWLINE, s, ''|'');
    _decode (4, ''(I4)'', s) i;
    PRINT(i);

    _comment Without a point the last d digits are the fraction;
    _decode (6, ''(F6.2)'', '' 12345'') x;
    PRINT(x);

    _comment The file ends after the first record;
    _rewind (7); _write (7, ''(I3)'') 9; _endfile (7); _rewind (7);
    _read (7, ''(A)'') s;
    PRINT(NEWLINE, s, ''|'');

    _read (54, ''(2I3)'') i, n;
    PRINT(i, n)
_end
