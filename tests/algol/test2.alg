_begin
    _comment PROCEDURES, REALS, RECURSION, NAME AND VALUE PARAMETERS;
    _real _procedure F(X); _value X; _real X;
        F := _if X < 1.0 _then 1.0 _else X * F(X - 1.0);
    _integer _procedure SUM(I, A, B, T); _value A, B;
        _integer I, A, B, T;
    _begin _integer S; S := 0;
        _for I := A _step 1 _until B _do S := S + T;
        SUM := S
    _end;
    _procedure SWAP(P, Q); _real P, Q;
    _begin _real T; T := P; P := Q; Q := T _end;
    _integer J; _real U, V;
    _integer _array C[1:3];
    U := 1.5; V := -0.00225;
    SWAP(U, V);
    PRINT(U, V, F(5.0), SUM(J, 1, 10, J*J));
    PRINT(SQRT(2.0), SIN(1.0), COS(1.0));
    PRINT(ARCTAN(1.0), LN(10.0), EXP(1.0));
    PRINT(ABS(-3), SIGN(-2.0), ENTIER(3.7));
    C[2] := ENTIER(7 / 2); PRINT(C[2], 7 / 2)
_end
_eop
