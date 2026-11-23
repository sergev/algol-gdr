_begin
    _comment NESTED BLOCKS AND SCOPES;
    _integer X;
    X := 1;
    _begin _integer X;
        X := 2;
        _begin _real X;
            X := 3.5;
            PRINT(X)
        _end;
        PRINT(X)
    _end;
    _begin
        X := X + 10
    _end;
    PRINT(X)
_end
_eop
