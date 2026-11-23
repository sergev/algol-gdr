_begin
    _comment TYPE ERRORS;
    _integer I;
    _real X;
    _boolean B;
    I := _true;
    B := 1;
    X := B + 1;
    _if X _then I := 0;
    B := I _and B;
_end
