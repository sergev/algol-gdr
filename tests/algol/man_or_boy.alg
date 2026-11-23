_begin
    _real _procedure a(k, x1, x2, x3, x4, x5);
    _value k; _integer k;
    _real x1, x2, x3, x4, x5;
    _begin
        _real _procedure b;
        _begin
            k := k - 1;
            b := a := a(k, b, x1, x2, x3, x4);
        _end;
        a := _if k _le 0 _then x4 + x5
                           _else b;
    _end;
    print(a(10, 1.0, -1.0, -1.0, 1.0, 0.0), NEWLINE);
_end
