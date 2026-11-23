_begin
    _comment STRING VARIABLES, ARRAYS, length AND INDEXING;
    _string S, T;
    _string _array A[1:2];
    _integer I;
    S := `ABCDEF';
    T := S;
    A[1] := `XY'; A[2] := S;
    print(length(S), length(A[1]), NEWLINE);
    _for I := 1 _step 1 _until length(S) _do print(S[I]);
    print(T, A[1], A[2], NEWLINE)
_end
