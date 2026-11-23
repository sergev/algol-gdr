_comment MIXED RELATIONS, BY-NAME FOR ELEMENTS, AN ELEMENT OF A
  DYNAMIC ARRAY AS A PARAMETER BY NAME;
_begin _integer I, J, N; _real X; _boolean B;
  _procedure Q(Z); _real Z; Z := Z + 1;
  _procedure R(V, W); _integer V, W;
  _begin _for V := W _do print(V, NEWLINE);
    _for V := W _step W _until 3 * W _do print(V, NEWLINE)
  _end;
  I := 1; J := 2; X := 1.5; N := 3;
  B := I _notequal X; print(B, NEWLINE);
  B := J _notequal 2.0; print(B, NEWLINE);
  _if I _ge X _then print(1, NEWLINE) _else print(2, NEWLINE);
  _if J _ge X _then print(3, NEWLINE) _else print(4, NEWLINE);
  _if I _notequal X _then print(5, NEWLINE) _else print(6, NEWLINE);
  R(I, J);
  _begin _array A[1:N];
    A[1] := 7; Q(A[1]); print(A[1], NEWLINE)
  _end
_end
