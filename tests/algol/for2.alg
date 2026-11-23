'COMMENT' FOR LOOPS REWRITTEN BY PASS 3, AND LOOPS IT GIVES UP ON;
'BEGIN' 'INTEGER' I, J, K; 'BOOLEAN' B;
  'INTEGER' 'ARRAY' IA[-10:20];
  'PROCEDURE' SHOW;
  'BEGIN' 'FOR' K := -10 'STEP' 1 'UNTIL' 20 'DO' PRINT(IA[K]);
    'FOR' K := -10 'STEP' 1 'UNTIL' 20 'DO' IA[K] := 0
  'END';
  J := 1; B := 'TRUE';
  'FOR' K := -10 'STEP' 1 'UNTIL' 20 'DO' IA[K] := 0;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[I] := I;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[I+1] := IA[I]; SHOW;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[2*I-1] := I;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[-I+5] := I+10;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[I-J] := I+20; SHOW;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' 'IF' B 'THEN' IA[I] := I;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[3-I] := I+10;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[1] := I; SHOW;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' 'IF' B 'THEN' 'GOTO' L1;
L1: PRINT(I);
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' 'GOTO' L2;
L2: PRINT(I);
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' I := I; PRINT(I);
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO'
  'BEGIN' L3: IA[I] := IA[I] + 1;
    'IF' IA[I] 'LT' 3 'THEN' 'GOTO' L3
  'END';
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO'
    'IF' J 'LT' 2 'THEN' IA[I+3] := I;
  SHOW;
  'FOR' I := 3 'STEP' -1 'UNTIL' 1 'DO' IA[I] := I;
  'FOR' I := 3 'STEP' -1 'UNTIL' 1 'DO' IA[(I+1)+(I+2)] := I; SHOW;
  'FOR' I := 1 'STEP' J 'UNTIL' 3 'DO' IA[I+1] := IA[I]+1;
  'FOR' I := 1 'STEP' J 'UNTIL' 3 'DO' 'IF' B 'THEN' IA[I+5] := I; SHOW;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[(I+1)-(I+2)] := I;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[(I-1)*(2+1)] := I+10;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[1+2] := I+20; SHOW;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[I+(2-1)] := I;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[I+2*3] := I+10;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[I*(-2)] := I+20;
  'FOR' I := 1 'STEP' 1 'UNTIL' 3 'DO' IA[2-I+5] := I+30; SHOW
'END'
'EOP'
