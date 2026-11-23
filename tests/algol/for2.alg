_comment FOR LOOPS REWRITTEN BY PASS 3, AND LOOPS IT GIVES UP ON;
_begin _integer I, J, K; _boolean B;
  _integer _array IA[-10:20];
  _procedure SHOW;
  _begin _for K := -10 _step 1 _until 20 _do print(IA[K], NEWLINE);
    _for K := -10 _step 1 _until 20 _do IA[K] := 0
  _end;
  J := 1; B := _true;
  _for K := -10 _step 1 _until 20 _do IA[K] := 0;
  _for I := 1 _step 1 _until 3 _do IA[I] := I;
  _for I := 1 _step 1 _until 3 _do IA[I+1] := IA[I]; SHOW;
  _for I := 1 _step 1 _until 3 _do IA[2*I-1] := I;
  _for I := 1 _step 1 _until 3 _do IA[-I+5] := I+10;
  _for I := 1 _step 1 _until 3 _do IA[I-J] := I+20; SHOW;
  _for I := 1 _step 1 _until 3 _do _if B _then IA[I] := I;
  _for I := 1 _step 1 _until 3 _do IA[3-I] := I+10;
  _for I := 1 _step 1 _until 3 _do IA[1] := I; SHOW;
  _for I := 1 _step 1 _until 3 _do _if B _then _goto L1;
L1: print(I, NEWLINE);
  _for I := 1 _step 1 _until 3 _do _goto L2;
L2: print(I, NEWLINE);
  _for I := 1 _step 1 _until 3 _do I := I; print(I, NEWLINE);
  _for I := 1 _step 1 _until 3 _do
  _begin L3: IA[I] := IA[I] + 1;
    _if IA[I] _lt 3 _then _goto L3
  _end;
  _for I := 1 _step 1 _until 3 _do
    _if J _lt 2 _then IA[I+3] := I;
  SHOW;
  _for I := 3 _step -1 _until 1 _do IA[I] := I;
  _for I := 3 _step -1 _until 1 _do IA[(I+1)+(I+2)] := I; SHOW;
  _for I := 1 _step J _until 3 _do IA[I+1] := IA[I]+1;
  _for I := 1 _step J _until 3 _do _if B _then IA[I+5] := I; SHOW;
  _for I := 1 _step 1 _until 3 _do IA[(I+1)-(I+2)] := I;
  _for I := 1 _step 1 _until 3 _do IA[(I-1)*(2+1)] := I+10;
  _for I := 1 _step 1 _until 3 _do IA[1+2] := I+20; SHOW;
  _for I := 1 _step 1 _until 3 _do IA[I+(2-1)] := I;
  _for I := 1 _step 1 _until 3 _do IA[I+2*3] := I+10;
  _for I := 1 _step 1 _until 3 _do IA[I*(-2)] := I+20;
  _for I := 1 _step 1 _until 3 _do IA[2-I+5] := I+30; SHOW
_end
