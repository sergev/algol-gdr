'BEGIN'
    'COMMENT' 'COMMON' BLOCKS: SCALARS AND ARRAYS OF ONE TO THREE
        DIMENSIONS, EXPORTED AS *ABC*, *DATA* AND *S*;
    'COMMON' /ABC/ X, Y(10), Z(2,3);
    'REAL' X; 'ARRAY' Y[1:10]; 'INTEGER' 'ARRAY' Z[1:2,1:3];
    'COMMON' /DATA/ N, T(2, 2, 2).,
    'INTEGER' N; 'ARRAY' T[1:2, 1:2, 1:2];
    'COMMON' /S/ W;
    'BOOLEAN' W;
    X := 1; Y[1] := 2; Z[2,3] := 3;
    N := 4; T[2, 2, 2] := 5; W := 'TRUE';
    PRINT(X, Y[1], Z[2,3], N, T[2, 2, 2], W);
'END'
'EOP'
