_begin
    _comment SWITCH DECLARATIONS AND DESIGNATIONAL EXPRESSIONS;
    _integer I;
    _switch S := A, B, _if I > 2 _then C _else A;
    I := 0;
NEXT:
    I := I + 1;
    _if I > 3 _then _goto DONE;
    _goto S[I];
A:  print(1, NEWLINE); _goto NEXT;
B:  print(2, NEWLINE); _goto NEXT;
C:  print(3, NEWLINE); _goto NEXT;
DONE:
    _goto _if I = 4 _then E _else S[1];
E:  print(I, NEWLINE)
_end
