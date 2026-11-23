'BEGIN'
    'COMMON' ABC/ X;
    'COMMON' /ABCDEFG/ X;
    'COMMON' /B1/ X.Y;
    'COMMON' /B2/ Y(2) Z;
    'COMMON' /B3/ Z(2).Y;
    'COMMON' /B4/ U(2).,V(3,4).,W;
    'REAL' X, Y, Z, U, V, W;
    X := 1;
'END'
'EOP'
