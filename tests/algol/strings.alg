'BEGIN'
    'COMMENT' STRING VARIABLES, ARRAYS, LENGTH AND INDEXING;
    'STRING' S, T;
    'STRING' 'ARRAY' A[1:2];
    'INTEGER' I;
    S := ''ABCDEF'';
    T := S;
    A[1] := ''XY''; A[2] := S;
    PRINT(LENGTH(S), LENGTH(A[1]));
    'FOR' I := 1 'STEP' 1 'UNTIL' LENGTH(S) 'DO' PRINT(S[I]);
    PRINT(NEWLINE, T, A[1], A[2])
'END'
'EOP'
