'COMMENT' FOR LIST ELEMENTS, SECOND PART;
'BEGIN' 'INTEGER' I, J; 'REAL' X; 'BOOLEAN' B;
  'INTEGER' 'ARRAY' IA[1:3]; 'REAL' 'ARRAY' RA[1:3];
  'BOOLEAN' 'ARRAY' BA[1:3];
  'SWITCH' S := L, M;
  'INTEGER' 'PROCEDURE' IP(N); 'VALUE' N; 'INTEGER' N; IP := N;
  'REAL' 'PROCEDURE' RP; RP := 1;
  'BOOLEAN' 'PROCEDURE' BP; BP := 'TRUE';
  'PROCEDURE' P0; ;
  'PROCEDURE' PP(A1, A2, A3, A4, A5, A6, A7, A8);
    'VALUE' A1; 'INTEGER' A1; 'REAL' A2; 'BOOLEAN' A3; 'ARRAY' A4;
    'LABEL' A5; 'PROCEDURE' A6; 'STRING' A7; 'SWITCH' A8;
  'BEGIN' A2 := A1 'END';
L: I := 1;
M: X := 1;
  I := 1; J := 2; X := 2; B := 'TRUE';
  IA[1] := 1; IA[2] := 2; IA[3] := 3;
  'FOR' I := I+I 'STEP' 1 'UNTIL' 2 'DO' X := 1; PRINT(I);
  'FOR' I := 'IF' B 'THEN' I 'ELSE' X 'STEP' 1 'UNTIL' 2 'DO' X := 1;
  PRINT(I);
  'FOR' I := ABS(I) 'STEP' 1 'UNTIL' 2 'DO' X := 1; PRINT(I);
  'FOR' I := 1 'STEP' I 'UNTIL' 2 'DO' X := 1; PRINT(I);
  'FOR' I := 1 'STEP' IA[1] 'UNTIL' 2 'DO' X := 1; PRINT(I);
  'FOR' I := 1 'STEP' IA[X] 'UNTIL' 2 'DO' X := 1; PRINT(I);
  'FOR' I := 1 'STEP' 1 'UNTIL' J 'DO' X := 1; PRINT(I);
  'FOR' X := 1 'STEP' 1 'UNTIL' 2 'DO' I := 1; PRINT(X)
'END'
'EOP'
