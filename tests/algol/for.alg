_begin
    _comment FOR STATEMENTS: STEP-UNTIL, WHILE, LISTS;
    _integer I, S;
    _real X;
    S := 0;
    _for I := 1 _step 1 _until 10 _do S := S + I;
    PRINT(S);
    _for I := 10 _step -3 _until 0 _do PRINT(I);
    _for I := 5 _step 1 _until 4 _do PRINT(-1);
    S := 1;
    _for I := S * 2 _while S < 100 _do S := I;
    PRINT(S);
    _for I := 1, 3, 7, 10 _step 5 _until 20, S _do PRINT(I);
    _for X := 0.5 _step 0.25 _until 1.0 _do PRINT(X);
    PRINT(I)
_end
