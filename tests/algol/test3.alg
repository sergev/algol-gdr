_begin
    _comment SWITCHES, GOTO, BOOLEANS, WHILE, BLOCKS, ARRAYS, STRINGS;
    _switch S := L1, L2, L3;
    _integer I, K; _boolean B, C;
    _array A[0:4, -1:1];
    _procedure P(N, M); _value N; _integer N; _array M;
        M[N, 0] := N + 0.5;
    I := 1;
L0: _goto S[I];
L1: I := I + 1; _goto L0;
L2: I := I + 1; _goto L0;
L3: B := I = 3 _and _not (I > 5) _or _false;
    C := B _impl _true _equiv _not B;
    K := 0;
    _for I := 1, 2, 5 _step 2 _until 9 _do K := K + 1;
    _begin _integer _array X[1:K];
        _for I := 1 _step 1 _until K _do X[I] := I * I;
        PRINT(X[K])
    _end;
    _for I := 0 _step 1 _until 4 _do P(I, A);
    PRINT(A[4, 0], B, C, K);
    PRINT(`HELLO FROM ALGOL')
_end
