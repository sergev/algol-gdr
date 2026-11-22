'BEGIN'
    'COMMENT' TYPE ERRORS;
    'INTEGER' I;
    'REAL' X;
    'BOOLEAN' B;
    I := 'TRUE';
    B := 1;
    X := B + 1;
    'IF' X 'THEN' I := 0;
    B := I 'AND' B;
'END'
'EOP'
