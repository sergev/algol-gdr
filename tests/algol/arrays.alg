_begin
    _comment ARRAYS: ONE AND TWO DIMENSIONS,
        CONSTANT AND DYNAMIC BOUNDS;
    _integer I, J, N;
    _real _array A[1:5];
    _integer _array B[0:2, -1:1];
    _boolean _array F[1:2];
    _for I := 1 _step 1 _until 5 _do A[I] := I / 2;
    print(A[1], A[5], NEWLINE);
    _for I := 0 _step 1 _until 2 _do
        _for J := -1 _step 1 _until 1 _do
            B[I, J] := 10 * I + J;
    print(B[0, -1], B[2, 1], B[1, 0], NEWLINE);
    F[1] := _true; F[2] := _not F[1];
    print(F[1], F[2], NEWLINE);
    N := 4;
    _begin
        _array C[1:N, 1:N + 1];
        _integer _array D[N:2 * N];
        _for I := 1 _step 1 _until N _do
            _for J := 1 _step 1 _until N + 1 _do
                C[I, J] := I * J;
        D[2 * N] := N;
        print(C[N, N + 1], D[8], NEWLINE)
    _end
_end
