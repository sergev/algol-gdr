_begin
    _common ABC/ X;
    _common /ABCDEFG/ X;
    _common /B1/ X.Y;
    _common /B2/ Y(2) Z;
    _common /B3/ Z(2).Y;
    _common /B4/ U(2).,V(3,4).,W;
    _real X, Y, Z, U, V, W;
    X := 1;
_end
