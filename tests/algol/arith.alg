'BEGIN'
    'COMMENT' INTEGER AND REAL ARITHMETIC, PRECEDENCE, POWER;
    'INTEGER' I, J, K;
    'REAL' X, Y;
    I := 7; J := 3;
    PRINT(I + J, I - J, I * J, I / J);
    PRINT(I + J * 2, (I + J) * 2, -I + J, I - J - 1);
    PRINT(I 'POWER' 2, 2 'POWER' 10, 2.0 'POWER' (-1));
    X := 1.5; Y := -0.25;
    PRINT(X + Y, X * Y, X / Y, X - Y);
    K := X + 0.6;
    PRINT(K, X + I, 1.0⏨3, 2.5⏨-2);
'END'
'EOP'
