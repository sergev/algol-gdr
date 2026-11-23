_begin
    _comment LABELS, GOTO, JUMP OUT OF A BLOCK AND A PROCEDURE;
    _integer I;
    _procedure P(N); _value N; _integer N;
    _begin
        _if N > 2 _then _goto OUT;
        PRINT(N)
    _end;
    I := 0;
AGAIN:
    I := I + 1;
    _if I < 3 _then _goto AGAIN;
    PRINT(I);
    _begin _integer K;
        K := 5;
        _goto L1;
        PRINT(-1);
    L1: PRINT(K);
        _goto L2
    _end;
    PRINT(-2);
L2: P(1); P(2); P(3);
    PRINT(-3);
OUT:
    PRINT(999)
_end
