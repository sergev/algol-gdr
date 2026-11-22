'BEGIN'
    'COMMENT' NESTED BLOCKS AND SCOPES;
    'INTEGER' X;
    X := 1;
    'BEGIN' 'INTEGER' X;
        X := 2;
        'BEGIN' 'REAL' X;
            X := 3.5;
            PRINT(X)
        'END';
        PRINT(X)
    'END';
    'BEGIN'
        X := X + 10
    'END';
    PRINT(X)
'END'
'EOP'
