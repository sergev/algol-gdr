'BEGIN'
    'COMMENT' PROCEDURES: VALUE AND NAME PARAMETERS, RECURSION,
        FUNCTIONS, PROCEDURES AS PARAMETERS, JENSEN'S DEVICE;
    'INTEGER' I, K;
    'REAL' 'ARRAY' V[1:3];
    'INTEGER' 'PROCEDURE' FACT(N); 'VALUE' N; 'INTEGER' N;
        FACT := 'IF' N 'LE' 1 'THEN' 1 'ELSE' N * FACT(N - 1);
    'PROCEDURE' INC(X); 'INTEGER' X;
        X := X + 1;
    'PROCEDURE' INCV(X); 'VALUE' X; 'INTEGER' X;
        X := X + 1;
    'REAL' 'PROCEDURE' SUM(J, L, U, T); 'VALUE' L, U;
        'INTEGER' J, L, U; 'REAL' T;
    'BEGIN' 'REAL' S; S := 0;
        'FOR' J := L 'STEP' 1 'UNTIL' U 'DO' S := S + T;
        SUM := S
    'END';
    'REAL' 'PROCEDURE' SQ(X); 'VALUE' X; 'REAL' X;
        SQ := X * X;
    'REAL' 'PROCEDURE' APPLY(F, X); 'REAL' 'PROCEDURE' F; 'REAL' X;
        APPLY := F(X);
    'PROCEDURE' NOARGS;
        PRINT(77);
    K := 1;
    INC(K); INCV(K);
    PRINT(K, FACT(10));
    V[1] := 1; V[2] := 2; V[3] := 3;
    PRINT(SUM(I, 1, 3, V[I] * V[I]));
    PRINT(APPLY(SQ, 3.0));
    NOARGS
'END'
'EOP'
