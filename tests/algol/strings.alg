_begin
    _comment STRING VARIABLES, ARRAYS, LENGTH AND INDEXING;
    _string S, T;
    _string _array A[1:2];
    _integer I;
    S := ''ABCDEF'';
    T := S;
    A[1] := ''XY''; A[2] := S;
    PRINT(LENGTH(S), LENGTH(A[1]));
    _for I := 1 _step 1 _until LENGTH(S) _do PRINT(S[I]);
    PRINT(NEWLINE, T, A[1], A[2])
_end
_eop
