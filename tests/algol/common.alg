_begin
    _comment _common BLOCKS: SCALARS AND ARRAYS OF ONE TO THREE
        DIMENSIONS, EXPORTED AS *ABC*, *DATA* AND *S*;
    _common /ABC/ X, Y(10), Z(2,3);
    _real X; _array Y[1:10]; _integer _array Z[1:2,1:3];
    _common /DATA/ N, T(2, 2, 2).,
    _integer N; _array T[1:2, 1:2, 1:2];
    _common /S/ W;
    _boolean W;
    X := 1; Y[1] := 2; Z[2,3] := 3;
    N := 4; T[2, 2, 2] := 5; W := _true;
    PRINT(X, Y[1], Z[2,3], N, T[2, 2, 2], W);
_end
_eop
