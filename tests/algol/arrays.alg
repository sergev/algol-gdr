'BEGIN'
    'COMMENT' ARRAYS: ONE AND TWO DIMENSIONS,
        CONSTANT AND DYNAMIC BOUNDS;
    'INTEGER' I, J, N;
    'REAL' 'ARRAY' A[1:5];
    'INTEGER' 'ARRAY' B[0:2, -1:1];
    'BOOLEAN' 'ARRAY' F[1:2];
    'FOR' I := 1 'STEP' 1 'UNTIL' 5 'DO' A[I] := I / 2;
    PRINT(A[1], A[5]);
    'FOR' I := 0 'STEP' 1 'UNTIL' 2 'DO'
        'FOR' J := -1 'STEP' 1 'UNTIL' 1 'DO'
            B[I, J] := 10 * I + J;
    PRINT(B[0, -1], B[2, 1], B[1, 0]);
    F[1] := 'TRUE'; F[2] := 'NOT' F[1];
    PRINT(F[1], F[2]);
    N := 4;
    'BEGIN'
        'ARRAY' C[1:N, 1:N + 1];
        'INTEGER' 'ARRAY' D[N:2 * N];
        'FOR' I := 1 'STEP' 1 'UNTIL' N 'DO'
            'FOR' J := 1 'STEP' 1 'UNTIL' N + 1 'DO'
                C[I, J] := I * J;
        D[2 * N] := N;
        PRINT(C[N, N + 1], D[8])
    'END'
'END'
'EOP'
