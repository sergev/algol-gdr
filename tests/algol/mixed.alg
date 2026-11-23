_comment MIXED RELATIONS, BY-NAME FOR ELEMENTS, AN ELEMENT OF A
  DYNAMIC ARRAY AS A PARAMETER BY NAME;
_begin _integer I, J, N; _real X; _boolean B;
  _procedure Q(Z); _real Z; Z := Z + 1;
  _procedure R(V, W); _integer V, W;
  _begin _for V := W _do PRINT(V);
    _for V := W _step W _until 3 * W _do PRINT(V)
  _end;
  I := 1; J := 2; X := 1.5; N := 3;
  B := I _notequal X; PRINT(B);
  B := J _notequal 2.0; PRINT(B);
  _if I _ge X _then PRINT(1) _else PRINT(2);
  _if J _ge X _then PRINT(3) _else PRINT(4);
  _if I _notequal X _then PRINT(5) _else PRINT(6);
  R(I, J);
  _begin _array A[1:N];
    A[1] := 7; Q(A[1]); PRINT(A[1])
  _end
_end
