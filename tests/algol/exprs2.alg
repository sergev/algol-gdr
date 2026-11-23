_comment FOR LIST ELEMENTS, SECOND PART;
_begin _integer I, J; _real X; _boolean B;
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
  I := 1; J := 2; X := 2; B := _true;
  IA[1] := 1; IA[2] := 2; IA[3] := 3;
  _for I := I+I _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := _if B _then I _else X _step 1 _until 2 _do X := 1;
  PRINT(I);
  _for I := abs(I) _step 1 _until 2 _do X := 1; PRINT(I);
  _for I := 1 _step I _until 2 _do X := 1; PRINT(I);
  _for I := 1 _step IA[1] _until 2 _do X := 1; PRINT(I);
  _for I := 1 _step IA[X] _until 2 _do X := 1; PRINT(I);
  _for I := 1 _step 1 _until J _do X := 1; PRINT(I);
  _for X := 1 _step 1 _until 2 _do I := 1; PRINT(X)
_end
