'BEGIN'
    'COMMENT' LABELS, GOTO, JUMP OUT OF A BLOCK AND A PROCEDURE;
    'INTEGER' I;
    'PROCEDURE' P(N); 'VALUE' N; 'INTEGER' N;
    'BEGIN'
        'IF' N > 2 'THEN' 'GOTO' OUT;
        PRINT(N)
    'END';
    I := 0;
AGAIN:
    I := I + 1;
    'IF' I < 3 'THEN' 'GOTO' AGAIN;
    PRINT(I);
    'BEGIN' 'INTEGER' K;
        K := 5;
        'GOTO' L1;
        PRINT(-1);
    L1: PRINT(K);
        'GOTO' L2
    'END';
    PRINT(-2);
L2: P(1); P(2); P(3);
    PRINT(-3);
OUT:
    PRINT(999)
'END'
'EOP'
