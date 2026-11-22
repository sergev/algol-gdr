'BEGIN'
    'COMMENT' FOR STATEMENTS: STEP-UNTIL, WHILE, LISTS;
    'INTEGER' I, S;
    'REAL' X;
    S := 0;
    'FOR' I := 1 'STEP' 1 'UNTIL' 10 'DO' S := S + I;
    PRINT(S);
    'FOR' I := 10 'STEP' -3 'UNTIL' 0 'DO' PRINT(I);
    'FOR' I := 5 'STEP' 1 'UNTIL' 4 'DO' PRINT(-1);
    S := 1;
    'FOR' I := S * 2 'WHILE' S < 100 'DO' S := I;
    PRINT(S);
    'FOR' I := 1, 3, 7, 10 'STEP' 5 'UNTIL' 20, S 'DO' PRINT(I);
    'FOR' X := 0.5 'STEP' 0.25 'UNTIL' 1.0 'DO' PRINT(X);
    PRINT(I)
'END'
'EOP'
