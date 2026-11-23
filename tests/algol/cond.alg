_begin
    _comment CONDITIONAL STATEMENTS AND EXPRESSIONS;
    _integer I, J;
    _for I := 1 _step 1 _until 4 _do
    _begin
        _if I = 1 _then PRINT(100)
        _else _if I = 2 _then PRINT(200)
        _else PRINT(300);
        _if I > 2 _then PRINT(I);
        J := _if I < 3 _then I * 10 _else -I;
        PRINT(J, _if I = 4 _then 1.5 _else 2.5)
    _end
_end
