'BEGIN'
    'COMMENT' BOOLEAN EXPRESSIONS AND RELATIONS;
    'BOOLEAN' A, B;
    'INTEGER' I;
    A := 'TRUE'; B := 'FALSE';
    PRINT(A 'AND' B, A 'OR' B, 'NOT' A, A 'IMPL' B, A 'EQUIV' B);
    PRINT('NOT' A 'OR' B 'AND' A, 'NOT' (A 'OR' B));
    I := 5;
    PRINT(I < 6, I 'LE' 5, I = 5, I 'NE' 5, I 'GE' 6, I > 4);
    PRINT(I 'LESS' 6, I 'GREATER' 6, I 'EQUAL' 5, I 'NOTEQUAL' 5);
    PRINT(I 'LT' 6, I 'GT' 6, I 'EQ' 5,
        I 'NOTLESS' 5, I 'NOTGREATER' 4);
'END'
'EOP'
