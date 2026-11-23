_begin
    _comment BOOLEAN EXPRESSIONS AND RELATIONS;
    _boolean A, B;
    _integer I;
    A := _true; B := _false;
    PRINT(A _and B, A _or B, _not A, A _impl B, A _equiv B);
    PRINT(_not A _or B _and A, _not (A _or B));
    I := 5;
    PRINT(I < 6, I _le 5, I = 5, I _ne 5, I _ge 6, I > 4);
    PRINT(I _less 6, I _greater 6, I _equal 5, I _notequal 5);
    PRINT(I _lt 6, I _gt 6, I _eq 5,
        I _notless 5, I _notgreater 4);
_end
_eop
