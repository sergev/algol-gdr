_comment _common ELEMENTS DECLARED OUT OF ORDER, NOT DECLARED,
    OR DECLARED AS A PROCEDURE;
_begin
    _common /A/ X, Y;
    _real Y, X;
    _common /B/ U, V;
    _real U;
    _common /C/ F;
    _integer _procedure F; F := 1;
    X := 1;
    PRINT(X)
_end
