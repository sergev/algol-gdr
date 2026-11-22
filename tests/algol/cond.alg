'BEGIN'
    'COMMENT' CONDITIONAL STATEMENTS AND EXPRESSIONS;
    'INTEGER' I, J;
    'FOR' I := 1 'STEP' 1 'UNTIL' 4 'DO'
    'BEGIN'
        'IF' I = 1 'THEN' PRINT(100)
        'ELSE' 'IF' I = 2 'THEN' PRINT(200)
        'ELSE' PRINT(300);
        'IF' I > 2 'THEN' PRINT(I);
        J := 'IF' I < 3 'THEN' I * 10 'ELSE' -I;
        PRINT(J, 'IF' I = 4 'THEN' 1.5 'ELSE' 2.5)
    'END'
'END'
'EOP'
