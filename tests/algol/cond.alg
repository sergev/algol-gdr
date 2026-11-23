_begin
    _comment CONDITIONAL STATEMENTS AND EXPRESSIONS;
    _integer I, J;
    _for I := 1 _step 1 _until 4 _do
    _begin
        _if I = 1 _then print(100, NEWLINE)
        _else _if I = 2 _then print(200, NEWLINE)
        _else print(300, NEWLINE);
        _if I > 2 _then print(I, NEWLINE);
        J := _if I < 3 _then I * 10 _else -I;
        print(J, _if I = 4 _then 1.5 _else 2.5, NEWLINE)
    _end
_end
