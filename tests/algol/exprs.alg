_comment FOR LIST ELEMENTS, CONDITIONAL AND POWER EXPRESSIONS;
_begin _integer I; _real X; _boolean B;
  _integer _array IA[1:3]; _real _array RA[1:3];
  _boolean _array BA[1:3];
  _switch S := L, M;
  _integer _procedure IP(N); _value N; _integer N; IP := N;
  _real _procedure RP; RP := 1;
  _boolean _procedure BP; BP := _true;
  _procedure P0; ;
  _procedure PP(A1, A2, A3, A4, A5, A6, A7, A8);
    _value A1; _integer A1; _real A2; _boolean A3; _array A4;
    _label A5; _procedure A6; _string A7; _switch A8;
  _begin A2 := A1 _end;
L: I := 1;
M: X := 1;
  I := 1; X := 2; B := _true;
  IA[1] := 1; IA[2] := 2; IA[3] := 3;
  I := IA[X]; PRINT(I);
  I := _if B _then I _else X; PRINT(I);
  B := X _lt 1; PRINT(B);
  B := X _equal 1; PRINT(B);
  X := 2 _power I; PRINT(X);
  X := 2 _power IP(1); PRINT(X);
  PRINT(IA);
  _for I := I _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := X _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := IA[1] _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := RP _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := 1.5 _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := -I _step 1 _until 2 _do X := 1; PRINT(I)
_end
_eop
