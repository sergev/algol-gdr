_begin
    _comment PROCEDURES: VALUE AND NAME PARAMETERS, RECURSION,
        FUNCTIONS, PROCEDURES AS PARAMETERS, JENSEN'S DEVICE;
    _integer I, K;
    _real _array V[1:3];
    _integer _procedure FACT(N); _value N; _integer N;
        FACT := _if N _le 1 _then 1 _else N * FACT(N - 1);
    _procedure INC(X); _integer X;
        X := X + 1;
    _procedure INCV(X); _value X; _integer X;
        X := X + 1;
    _real _procedure SUM(J, L, U, T); _value L, U;
        _integer J, L, U; _real T;
    _begin _real S; S := 0;
        _for J := L _step 1 _until U _do S := S + T;
        SUM := S
    _end;
    _real _procedure SQ(X); _value X; _real X;
        SQ := X * X;
    _real _procedure APPLY(F, X); _real _procedure F; _real X;
        APPLY := F(X);
    _procedure NOARGS;
        PRINT(77);
    K := 1;
    INC(K); INCV(K);
    PRINT(K, FACT(10));
    V[1] := 1; V[2] := 2; V[3] := 3;
    PRINT(SUM(I, 1, 3, V[I] * V[I]));
    PRINT(APPLY(SQ, 3.0));
    NOARGS
_end
