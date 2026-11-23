_begin
    _comment LABELS, GOTO, JUMP OUT OF A BLOCK AND A PROCEDURE;
    _integer I;
    _procedure P(N); _value N; _integer N;
    _begin
        _if N > 2 _then _goto OUT;
        print(N, NEWLINE)
    _end;
    I := 0;
AGAIN:
    I := I + 1;
    _if I < 3 _then _goto AGAIN;
    print(I, NEWLINE);
    _begin _integer K;
        K := 5;
        _goto L1;
        print(-1, NEWLINE);
    L1: print(K, NEWLINE);
        _goto L2
    _end;
    print(-2, NEWLINE);
L2: P(1); P(2); P(3);
    print(-3, NEWLINE);
OUT:
    print(999, NEWLINE)
_end
